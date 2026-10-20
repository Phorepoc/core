// ph_throw.cpp -- OURS (Path C, x86 / x32): libstdc++'s throws become readable errors.
//
// C++ exceptions do not work in our x32 E32 images yet (docs/phorepoc/browser-plan.md, "Not working: C++ exceptions": ld
// rewrites .eh_frame and the emitter relocates it with stale offsets), and the engine is -fno-exceptions anyway. libstdc++
// still throws on its own behalf -- std::mutex::lock -> std::system_error, std::vector -> length_error, new -> bad_alloc --
// and an uncatchable throw died somewhere in the unwinder with the cause lost (2026-10-13). Every such throw goes through one
// of these std::__throw_* helpers. build_libstdcxx.sh weakens libstdc++'s definitions (objcopy -W); these strong ones, linked
// as an object of the image (mmp SOURCE), win. Each logs the exception, its message / errno and the caller (`ph sym <addr>`),
// replays the program's stdout, and panics CXX-THROW <reason>: errno for system_error / future_error, else the kind's number.
// What a -fno-exceptions libstdc++ does (abort), except that it says why. Throws that bypass the helpers (a `throw` in user
// code, operator new's bad_alloc) still end in std::terminate, whose message abort() (phsym.cpp) now replays.
#include <e32std.h>
#include <e32debug.h>
#include <string.h>

extern "C" void ph_replay_stdout();

namespace {

enum TKind { EBadCast = 1, EBadAlloc, EBadTypeid, ELogic, ERange, EDomain, ELength, EOutOfRange, EBadException, ERuntime,
    EOverflow, EUnderflow, EInvalidArgument, EBadArrayNewLength, EBadFunctionCall, EFuture, EIos, ERegex, ESystem };

__attribute__((noreturn)) void phThrow(TKind aKind, const char* aName, const char* aWhat, int aErr, void* aCaller)
    {
    if (aErr)
        RDebug::Printf("[cxx] THROW std::%s: %s (errno %d) from %x", aName, aWhat ? aWhat : "", aErr, (unsigned)(TUint)aCaller);
    else
        RDebug::Printf("[cxx] THROW std::%s: %s from %x", aName, aWhat ? aWhat : "", (unsigned)(TUint)aCaller);
    ph_replay_stdout();
    _LIT(KCat, "CXX-THROW");
    User::Panic(KCat, aErr ? aErr : aKind);
    for (;;) { }
    }

} // namespace

#define PH_RA __builtin_return_address(0)

namespace std {
__attribute__((noreturn)) void __throw_bad_cast() { phThrow(EBadCast, "bad_cast", 0, 0, PH_RA); }
__attribute__((noreturn)) void __throw_bad_alloc() { phThrow(EBadAlloc, "bad_alloc", 0, 0, PH_RA); }
__attribute__((noreturn)) void __throw_bad_typeid() { phThrow(EBadTypeid, "bad_typeid", 0, 0, PH_RA); }
__attribute__((noreturn)) void __throw_logic_error(const char* s) { phThrow(ELogic, "logic_error", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_range_error(const char* s) { phThrow(ERange, "range_error", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_domain_error(const char* s) { phThrow(EDomain, "domain_error", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_length_error(const char* s) { phThrow(ELength, "length_error", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_out_of_range(const char* s) { phThrow(EOutOfRange, "out_of_range", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_out_of_range_fmt(const char* s, ...) { phThrow(EOutOfRange, "out_of_range", s, 0, PH_RA); }   // the format, unexpanded
__attribute__((noreturn)) void __throw_bad_exception() { phThrow(EBadException, "bad_exception", 0, 0, PH_RA); }
__attribute__((noreturn)) void __throw_runtime_error(const char* s) { phThrow(ERuntime, "runtime_error", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_overflow_error(const char* s) { phThrow(EOverflow, "overflow_error", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_underflow_error(const char* s) { phThrow(EUnderflow, "underflow_error", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_invalid_argument(const char* s) { phThrow(EInvalidArgument, "invalid_argument", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_bad_array_new_length() { phThrow(EBadArrayNewLength, "bad_array_new_length", 0, 0, PH_RA); }
__attribute__((noreturn)) void __throw_bad_function_call() { phThrow(EBadFunctionCall, "bad_function_call", 0, 0, PH_RA); }
__attribute__((noreturn)) void __throw_future_error(int e) { phThrow(EFuture, "future_error", 0, e, PH_RA); }
__attribute__((noreturn)) void __throw_ios_failure(const char* s) { phThrow(EIos, "ios_base::failure", s, 0, PH_RA); }
__attribute__((noreturn)) void __throw_ios_failure(const char* s, int e) { phThrow(EIos, "ios_base::failure", s, e, PH_RA); }
__attribute__((noreturn)) void __throw_system_error(int e) { phThrow(ESystem, "system_error", strerror(e), e, PH_RA); }
} // namespace std

// regex_constants::error_type is an enum: declared here without <regex>, C++98 style (the mangled name must match
// _ZSt19__throw_regex_errorNSt15regex_constants10error_typeE)
namespace std { namespace regex_constants { enum error_type { ePhErrorTypeUnused }; } }
namespace std {
__attribute__((noreturn)) void __throw_regex_error(regex_constants::error_type e) { phThrow(ERegex, "regex_error", 0, static_cast<int>(e), PH_RA); }
}
