# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-1.0

"""mkcompsupp.py -- regenerate OUR build of the ARM compiler-support runtime (drtaeabi.dll) from the 2010 baseline.

drtaeabi.dll is the DLL every SDK-built Symbian application imports for the EHABI unwinder, the C++ exception
semantics (__cxa_*), RTTI, the __aeabi_* integer/memory helpers and TCppRTExceptionsGlobals. The release builds it with
RVCT: ARM's reference EHABI sources (kernel/eka/compsupp/symaehabi, "ARM EABI LICENCE.txt") plus RVCT's proprietary
libraries (c_5.l / cpprt_5.l). Here the same sources are built with GCC:

  symaehabi/*  <- baseline symaehabi/*, copied and edited by EXACT replacements (each must match exactly once, so a moved
                  anchor fails loudly, as mkswi.py does). What changes is only what RVCT's compiler accepted and GCC does not:
                    unwinder.c       the six __asm functions (VFP save/restore, core restore, _Unwind_RaiseException,
                                     _Unwind_Resume) -> the same instructions as GNU top-level asm
                    cppsemantics.cpp __cxa_end_cleanup (asm)  -> GNU asm
                    unwind_env.h     __declspec(noreturn) / __forceinline -> GCC attributes
                    cxxabi.h         EDG's declaration of the RTTI classes -> GCC's <cxxabi.h> (the layouts are the ARM C++ ABI's)
                    symbian_support.cpp  the RVCT 2.1 placeholders for XLeaveException's typeinfo off; the key function stays
  compsupp_pre.h  <- force-included: gives XLeaveException its RVCT 2.2+ shape (a virtual key function -> vptr + typeinfo
                  + vtable in drtaeabi, ordinals 206-208)
  drtaeabi.mmp    <- the release's mmp, sources re-rooted (the five RVCT-only files replaced by gcc_abi.cpp)

    py src/phorepoc/compsupp/mkcompsupp.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
KHS = os.path.join(REPO, "baseline", "symbian-2010", "oss.FCL.sf.os.kernelhwsrv", "kernel", "eka", "compsupp")
SRC = os.path.join(KHS, "symaehabi")
OUT = os.path.join(HERE, "symaehabi")
os.makedirs(OUT, exist_ok=True)


def read(p):
    return open(p, encoding="utf-8", errors="replace", newline="").read().replace("\r\n", "\n")


def write(name, text):
    open(os.path.join(OUT, name), "w", newline="\n", encoding="utf-8").write(text)


def sub1(text, old, new, what):
    assert text.count(old) == 1, f"{what}: anchor found {text.count(old)}x: {old[:60]!r}"
    return text.replace(old, new)


def replace_block(text, start, new, what):
    """Replace the function starting at the line that begins with `start` up to the first line that is exactly `}`."""
    i = text.index(start) if text.count(start) == 1 else -1
    assert i >= 0, f"{what}: start anchor {start!r} found {text.count(start)}x"
    m = re.compile(r"^\}\s*$", re.M).search(text, i)
    assert m, f"{what}: no closing brace"
    return text[:i] + new + text[m.end():]


# ---- unwinder.c
u = read(os.path.join(SRC, "unwinder.c"))

VFP = {
    # the encodings are the ones the original's own comments give (FSTMIAD / FLDMIAD of d0-d15, d16-d31); .inst so they
    # assemble without -mfpu (the soft-float images of this ROM carry no VFP build attributes)
    "__ARM_Unwind_VRS_VFPpreserve_low": "0xec800b20",
    "__ARM_Unwind_VRS_VFPpreserve_high": "0xecc00b20",
    "__ARM_Unwind_VRS_VFPrestore_low": "0xec900b20",
    "__ARM_Unwind_VRS_VFPrestore_high": "0xecd00b20",
}
for fn, enc in VFP.items():
    new = f"""extern "C" void {fn}(void *vfpp);
__asm__(
	".text\\n.arm\\n.align 2\\n.global {fn}\\n.type {fn}, %function\\n"
	"{fn}:\\n"
	"	.inst {enc}\\n"		/* r0 = vfpp (FSTMIAD/FLDMIAD r0, {{...}}) */
	"	bx lr\\n");"""
    u = replace_block(u, f"__asm void {fn}(void *vfpp)", new, fn)

u = replace_block(u, "__asm NORETURNDECL void __ARM_Unwind_VRS_corerestore(void *corep)", """extern "C" NORETURNDECL void __ARM_Unwind_VRS_corerestore(void *corep);
__asm__(
	".text\\n.arm\\n.align 2\\n.global __ARM_Unwind_VRS_corerestore\\n.type __ARM_Unwind_VRS_corerestore, %function\\n"
	"__ARM_Unwind_VRS_corerestore:\\n"
	/* We rely on corep pointing to a location in the stack, as we briefly assign it to sp (the original's comment):
	   ARMCC >= 3.0 spelling -- ldm to pc interworks on ARMv5T+ */
	"	mov r14, r0\\n"
	"	ldmia r14!, {r0-r12}\\n"
	"	ldr r13, [r14], #4\\n"
	"	ldmia r14, {r14, r15}\\n");""", "corerestore")

# the two entries that build a phase2_virtual_register_set on the stack (ARM state, ARMCC >= 3.0 spelling)
FRAME = "".join('\t"\t%s\\n"\n' % i for i in (
	"stmdb r13, {r14, r15}", "str r13, [r13, #-3*4]", "sub r13, r13, #3*4", "stmfd sp!, {r0-r12}",
	"mov r1, #0", "str r1, [sp, #-4]!", "mov r1, sp", "sub sp, sp, #4"))
u = replace_block(u, "__asm _Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Control_Block *ucbp)", """extern "C" _Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Control_Block *ucbp);
__asm__(
	".text\\n.arm\\n.align 2\\n.global _Unwind_RaiseException\\n.type _Unwind_RaiseException, %function\\n"
	"_Unwind_RaiseException:\\n"
	"	.fnstart\\n	.cantunwind\\n"
""" + FRAME + """	"	bl __ARM_Unwind_RaiseException\\n"
	"	ldr r14, [sp, #16*4]\\n"
	"	add sp, sp, #18*4\\n"
	"	bx lr\\n"
	"	.fnend\\n");""", "_Unwind_RaiseException")
u = replace_block(u, "__asm NORETURNDECL void _Unwind_Resume(_Unwind_Control_Block *ucbp)", """extern "C" NORETURNDECL void _Unwind_Resume(_Unwind_Control_Block *ucbp);
__asm__(
	".text\\n.arm\\n.align 2\\n.global _Unwind_Resume\\n.type _Unwind_Resume, %function\\n"
	"_Unwind_Resume:\\n"
	"	.fnstart\\n	.cantunwind\\n"
""" + FRAME + """	"	b __ARM_Unwind_Resume\\n"
	"	.fnend\\n");""", "_Unwind_Resume")
# the helpers the asm calls are plain C: give them C linkage in this C++ TU
# ... as are the three personality routines (unwind_pr.cpp defines them with C linkage)
u = sub1(u, "WEAKDECL _Unwind_Reason_Code __aeabi_unwind_cpp_pr0(", 'extern "C" _Unwind_Reason_Code __aeabi_unwind_cpp_pr0(', "pr0")
u = sub1(u, "IMPORT_C WEAKDECL _Unwind_Reason_Code __aeabi_unwind_cpp_pr1(", 'extern "C" _Unwind_Reason_Code __aeabi_unwind_cpp_pr1(', "pr1")
u = sub1(u, "IMPORT_C WEAKDECL _Unwind_Reason_Code __aeabi_unwind_cpp_pr2(", 'extern "C" _Unwind_Reason_Code __aeabi_unwind_cpp_pr2(', "pr2")
u = sub1(u, "_Unwind_Reason_Code __ARM_Unwind_RaiseException(_Unwind_Control_Block *ucbp,\n",
         'extern "C" _Unwind_Reason_Code __ARM_Unwind_RaiseException(_Unwind_Control_Block *ucbp,\n', "ARM_Unwind_RaiseException")
u = sub1(u, "NORETURNDECL void __ARM_Unwind_Resume(_Unwind_Control_Block *ucbp,\n",
         'extern "C" NORETURNDECL void __ARM_Unwind_Resume(_Unwind_Control_Block *ucbp,\n', "ARM_Unwind_Resume")
write("unwinder.cpp", u)       # compiled as C++ (the asm entry points are declared extern "C")

# ---- unwind_pr.c : the personality routines. Written in C (`typedef unsigned char bool`, `struct _ZSt9type_info`); built
# as C++ here (one compiler mode, the C++ ABI declarations of symbian_support.h) -- those three typedefs go, the file's
# own contents get C linkage
p = read(os.path.join(SRC, "unwind_pr.c"))
p = sub1(p, "#include <cstdlib>\n", "", "unwind_pr <cstdlib>")
p = sub1(p, "typedef unsigned char bool;\nstatic const bool false = 0;\nstatic const bool true = !false;\n", "", "unwind_pr bool")
p = sub1(p, "typedef struct _ZSt9type_info type_info; /* This names C++ type_info type */",
         "typedef std::type_info type_info; /* C++ type_info type */", "unwind_pr type_info")
p = sub1(p, "/* Forward decl: */\n", 'extern "C" {\n\n/* Forward decl: */\n', "unwind_pr extern C")
p = p.rstrip("\n") + "\n\n}  /* extern \"C\" */\n"
write("unwind_pr.cpp", p)

# ---- cppsemantics.cpp
c = read(os.path.join(SRC, "cppsemantics.cpp"))
c = replace_block(c, "EXPORT_C __asm void __cxa_end_cleanup(void) {", """extern "C" void __cxa_end_cleanup(void);
__asm__(
	".text\\n.arm\\n.align 2\\n.global __cxa_end_cleanup\\n.type __cxa_end_cleanup, %function\\n"
	"__cxa_end_cleanup:\\n"
	"	.fnstart\\n	.cantunwind\\n"
	"	stmfd r13!, {r1-r12}\\n"
	"	bl __ARM_cxa_end_cleanup\\n"		/* returns the UCB address in r0 */
	"	ldmia r13!, {r1-r12}\\n"
	"	b _Unwind_Resume\\n"			/* won't return */
	"	.fnend\\n");""", "__cxa_end_cleanup")
# GCC declares the C++ runtime entry points implicitly wherever a try/catch/throw needs them, with void* parameters
# (and `throw()`/noreturn); this file declares them with UCB* / type_info* -- the same C symbols, different types, which
# GCC rejects. Same ABI (all are pointers): the signatures become GCC's, the bodies cast back.
c = sub1(c, "  IMPORT_C void *__cxa_allocate_exception(size_t size);\n  IMPORT_C void __cxa_free_exception(void *);\n"
            "  WEAKDECL void __cxa_throw(void *, const type_info *, cppdtorptr);\n  IMPORT_C void __cxa_rethrow(void);\n"
            "  IMPORT_C void *__cxa_get_exception_ptr(UCB *);\n  void *__cxa_begin_catch(UCB *);\n",
         "  void *__cxa_allocate_exception(size_t size) throw();\n  void __cxa_free_exception(void *) throw();\n"
         "  void __cxa_throw(void *, void *, void (*)(void *)) __attribute__((noreturn));\n"
         "  void __cxa_rethrow(void) __attribute__((noreturn));\n"
         "  void *__cxa_get_exception_ptr(void *) throw();\n  void *__cxa_begin_catch(void *) throw();\n", "cppsemantics decls")
# ... and GCC's EHABI exception specifications call __cxa_call_terminate: it must be nothrow + noreturn (else an ICE)
c = sub1(c, "  IMPORT_C void __cxa_call_terminate(UCB *);\n",
         "  void __cxa_call_terminate(UCB *) throw() __attribute__((noreturn));\n", "call_terminate decl")
c = sub1(c, "EXPORT_C void __cxa_call_terminate(UCB *ucbp)\n{", "EXPORT_C void __cxa_call_terminate(UCB *ucbp) throw()\n{", "call_terminate")
c = sub1(c, "  void call_terminate_handler(UCB *);\n", "  void call_terminate_handler(UCB *) __attribute__((noreturn));\n", "call_terminate_handler")
c = sub1(c, "EXPORT_C void *__cxa_allocate_exception(size_t size)\n{", "EXPORT_C void *__cxa_allocate_exception(size_t size) throw()\n{", "allocate")
c = sub1(c, "EXPORT_C void __cxa_free_exception(void *eop)\n{", "EXPORT_C void __cxa_free_exception(void *eop) throw()\n{", "free")
c = sub1(c, "EXPORT_C void __cxa_throw(void *eop, const type_info *t, cppdtorptr d)\n{",
         "EXPORT_C void __cxa_throw(void *eop, void *t_, void (*d_)(void *))\n{\n  const type_info *t = (const type_info *)t_;\n"
         "  cppdtorptr d = (cppdtorptr)d_;\n", "throw")
c = sub1(c, "EXPORT_C void *__cxa_get_exception_ptr(UCB *ucbp)\n{",
         "EXPORT_C void *__cxa_get_exception_ptr(void *ucbp_) throw()\n{\n  UCB *ucbp = (UCB *)ucbp_;\n", "get_exception_ptr")
c = sub1(c, "void *__cxa_begin_catch(UCB *ucbp)\n{",
         "void *__cxa_begin_catch(void *ucbp_) throw()\n{\n  UCB *ucbp = (UCB *)ucbp_;\n", "begin_catch")
# <new>: e32cmn.h declares operator new itself (GCC's <new> would redeclare it with other exception specifiers)
c = sub1(c, "#include <new>\n", "/* <new>: e32cmn.h declares operator new (GCC's would clash) */\n", "cppsemantics <new>")
# GCC-compatible exception objects: a GCC personality routine (libsupc++, statically linked into device images) uses ITS
# __cxa_type_match, which only treats class "GNUCC++\0" as native (anything else is foreign -> only catch(...) matches;
# BrowserNG's typed `catch(XLeaveException&)` fell to catch(...) -> User::Invariant). The __cxa_exception layout already
# equals libsupc++'s ARM EHABI one. We stamp GCC's class and accept both as ours; a GCC catcher's _Unwind_DeleteException
# reaches exception_cleanup, which must destroy+free (libsupc++'s globals are not ours, so no catch bookkeeping).
c = sub1(c, r'#define ARMCPP_EXCEPTIONS_CLASS "ARM\0C++\0"',
         r'#define ARMCPP_EXCEPTIONS_CLASS "ARM\0C++\0"' + "\n"
         "/* Objects we allocate are stamped with GCC's native class (\"GNUCC++\\0\", __gxx_exception_class) so a GCC\n"
         " * personality routine (libsupc++ statically linked into device images; its own __cxa_type_match is used,\n"
         " * never ours) treats them as native and matches typed handlers. The __cxa_exception header layout is\n"
         " * identical to libsupc++'s ARM EHABI one. We accept both classes as our own. */\n"
         r'#define GCCCPP_EXCEPTIONS_CLASS "GNUCC++\0"', "gcc exc class")
c = sub1(c, "  return !NAMES::same_exceptions_class(&ucbp->exception_class, ARMCPP_EXCEPTIONS_CLASS);",
         "  return !NAMES::same_exceptions_class(&ucbp->exception_class, ARMCPP_EXCEPTIONS_CLASS) &&\n"
         "         !NAMES::same_exceptions_class(&ucbp->exception_class, GCCCPP_EXCEPTIONS_CLASS);", "foreign slow")
c = sub1(c, "  return !NAMES::same_exceptions_class(&ucbp->exception_class, &is_foreign_exception_static.s);",
         "  static const union {\n    const char s[EXCEPTIONS_CLASS_SIZE+1]; int dummy;\n  } is_gcc_exception_static = {GCCCPP_EXCEPTIONS_CLASS};\n"
         "  return !NAMES::same_exceptions_class(&ucbp->exception_class, &is_foreign_exception_static.s) &&\n"
         "         !NAMES::same_exceptions_class(&ucbp->exception_class, &is_gcc_exception_static.s);", "foreign fast")
c = sub1(c, "  memcpy(ucbp->exception_class, ARMCPP_EXCEPTIONS_CLASS, EXCEPTIONS_CLASS_SIZE);",
         "  memcpy(ucbp->exception_class, GCCCPP_EXCEPTIONS_CLASS, EXCEPTIONS_CLASS_SIZE);", "alloc class")
c = sub1(c, "  NAMES::eh_catch_semantics(ucbp);\n  __cxa_end_catch();\n}",
         "  // The exception is stamped with GCC's native class, so the catcher is (usually) libsupc++'s\n"
         "  // __cxa_end_catch, which calls us through _Unwind_DeleteException when its handler count drops\n"
         "  // to 0. Its globals are not ours, so do not run our catch bookkeeping: just destroy and free.\n"
         "  __cxa_exception *ep = ucbp_to_ep(ucbp);\n"
         "  if (ep->exceptionDestructor) ep->exceptionDestructor(ep + 1);\n"
         "  __cxa_free_exception(ep + 1);\n}", "external termination")
write("cppsemantics.cpp", c)

# ---- unwind_env.h
e = read(os.path.join(SRC, "unwind_env.h"))
e = sub1(e, "#define NORETURNDECL __declspec(noreturn)", "#define NORETURNDECL __attribute__((noreturn))", "NORETURNDECL")
e = sub1(e, "#define FORCEINLINE __forceinline", "#define FORCEINLINE inline __attribute__((always_inline))", "FORCEINLINE")
write("unwind_env.h", e)

# ---- symbian_support.cpp : RVCT 2.1 placeholders off (GCC has no __ARMCC_VERSION), the key function stays
s = read(os.path.join(SRC, "symbian_support.cpp"))
s = sub1(s, "#if __ARMCC_VERSION < 220000\nextern \"C\"\n{", "#if 0\nextern \"C\"\n{", "ARMCC_VERSION placeholder")
write("symbian_support.cpp", s)

# ---- cxxabi.h : GCC's (libsupc++ implements the same ARM C++ ABI classes); what the sources use beyond
write("cxxabi.h", """/* GENERATED by mkcompsupp.py -- replaces EDG's cxxabi.h: GCC's declarations of the C++ ABI classes (same layouts) */
#ifndef __SYMAEHABI_CXXABI_H
#define __SYMAEHABI_CXXABI_H
#include <stddef.h>
#include <exception>
#include <typeinfo>
/* GCC's header also declares the C entry points with GCC's own signatures; these sources declare the ARM EHABI ones
 * (__cxa_begin_catch(UCB *) ...) -- same C names, different types. Only the RTTI classes are wanted from it. */
#define __cxa_allocate_exception __gcc_cxa_allocate_exception
#define __cxa_free_exception __gcc_cxa_free_exception
#define __cxa_throw __gcc_cxa_throw
#define __cxa_rethrow __gcc_cxa_rethrow
#define __cxa_begin_catch __gcc_cxa_begin_catch
#define __cxa_end_catch __gcc_cxa_end_catch
#define __cxa_bad_cast __gcc_cxa_bad_cast
#define __cxa_bad_typeid __gcc_cxa_bad_typeid
#define __cxa_get_exception_ptr __gcc_cxa_get_exception_ptr
#define __cxa_current_exception_type __gcc_cxa_current_exception_type
#define __cxa_pure_virtual __gcc_cxa_pure_virtual
#define __cxa_call_unexpected __gcc_cxa_call_unexpected
#define __cxa_vec_new __gcc_cxa_vec_new
#define __cxa_vec_new2 __gcc_cxa_vec_new2
#define __cxa_vec_new3 __gcc_cxa_vec_new3
#define __cxa_vec_ctor __gcc_cxa_vec_ctor
#define __cxa_vec_dtor __gcc_cxa_vec_dtor
#define __cxa_vec_cctor __gcc_cxa_vec_cctor
#define __cxa_vec_delete __gcc_cxa_vec_delete
#define __cxa_vec_delete2 __gcc_cxa_vec_delete2
#define __cxa_vec_delete3 __gcc_cxa_vec_delete3
#define __cxa_vec_cleanup __gcc_cxa_vec_cleanup
#define __cxa_guard_acquire __gcc_cxa_guard_acquire
#define __cxa_guard_release __gcc_cxa_guard_release
#define __cxa_guard_abort __gcc_cxa_guard_abort
#define __cxa_atexit __gcc_cxa_atexit
#define __cxa_finalize __gcc_cxa_finalize
#define __dynamic_cast __gcc_dynamic_cast
#include_next <cxxabi.h>
#undef __cxa_allocate_exception
#undef __cxa_free_exception
#undef __cxa_throw
#undef __cxa_rethrow
#undef __cxa_begin_catch
#undef __cxa_end_catch
#undef __cxa_bad_cast
#undef __cxa_bad_typeid
#undef __cxa_get_exception_ptr
#undef __cxa_current_exception_type
#undef __cxa_pure_virtual
#undef __cxa_call_unexpected
#undef __cxa_vec_new
#undef __cxa_vec_new2
#undef __cxa_vec_new3
#undef __cxa_vec_ctor
#undef __cxa_vec_dtor
#undef __cxa_vec_cctor
#undef __cxa_vec_delete
#undef __cxa_vec_delete2
#undef __cxa_vec_delete3
#undef __cxa_vec_cleanup
#undef __cxa_guard_acquire
#undef __cxa_guard_release
#undef __cxa_guard_abort
#undef __cxa_atexit
#undef __cxa_finalize
#undef __dynamic_cast
#endif
""")


# ---- the rest verbatim
for f in ("basics.h", "emergency_buffer.h", "symbian_rtti.cpp", "symbian_support.h", "unwinder.h"):
    write(f, read(os.path.join(SRC, f)))

# ---- compsupp_pre.h
write("compsupp_pre.h", """/* GENERATED by mkcompsupp.py -- force-included into every drtaeabi TU.
 * e32cmn.h gives XLeaveException the RVCT 2.2+ shape only when __ARMCC_VERSION >= 220000: a virtual key function
 * (ForceKeyFunction), so the class has a vptr, and its typeinfo/vtable are emitted where the key function is defined
 * (symbian_support.cpp) -- drtaeabi ordinals 206-208. The binaries SDK apps are built with all assume that layout
 * (sizeof(TCppRTExceptionsGlobals) is baked into their startup code), so the class is redeclared that way here. */
/* GCC's <exception>/<typeinfo>/<cxxabi.h> FIRST: e32cmn.h then skips its own declarations of std::terminate/unexpected/
 * uncaught_exception (it does when <exception> is already in), which GCC would reject as differently specified */
#include <stddef.h>
#include <exception>
#include <typeinfo>
#define __SUPPORT_CPP_EXCEPTIONS__
#define XLeaveException XLeaveException_Plain
#include <e32cmn.h>
#undef XLeaveException
class XLeaveException
	{
public:
	inline XLeaveException() {}
	inline XLeaveException(TInt aReason) {iR = aReason;}
	inline TInt Reason() const {return iR;}
	IMPORT_C TInt GetReason() const;
private:
	virtual void ForceKeyFunction();
private:
	TInt iR;
	};
""")

# ---- drtaeabi.mmp : the release's, re-rooted
mmp = read(os.path.join(KHS, "rvct", "drtaeabi.mmp"))
mmp = re.sub(r"^(source\s+(aeabidiv0|drtaeabi|aeabimem|type_info|no_extras)\.cpp)\s*$", r"// \1  -- see gcc_abi.cpp", mmp, flags=re.M | re.I)
mmp = re.sub(r"^sourcepath\s+\.\./symaehabi\s*$", "", mmp, flags=re.M | re.I)
mmp = re.sub(r"^source\s+(symbian_support\.cpp|symbian_rtti\.cpp|unwinder\.c|unwind_pr\.c|cppsemantics\.cpp)\s*$", "", mmp, flags=re.M | re.I)
rel = os.path.relpath(KHS, HERE).replace("\\", "/")
mmp += f"""
// OURS (src/phorepoc/compsupp/mkcompsupp.py): the release's sources, built with GCC
SOURCEPATH {rel}/rvct
SOURCE aeabidiv0.cpp
SOURCE no_extras.cpp
SOURCEPATH .
SOURCE gcc_abi.cpp
SOURCE vec_abi.cpp
SOURCEPATH symaehabi
SOURCE symbian_support.cpp
SOURCE symbian_rtti.cpp
SOURCE unwinder.cpp
SOURCE unwind_pr.cpp
SOURCE cppsemantics.cpp
"""
open(os.path.join(HERE, "drtaeabi.mmp"), "w", newline="\n").write(
    "// drtaeabi.mmp -- GENERATED by src/phorepoc/compsupp/mkcompsupp.py from kernel/eka/compsupp/rvct/drtaeabi.mmp (do not edit)\n" + mmp)
print("compsupp: generated", len(os.listdir(OUT)), "files in", os.path.relpath(OUT, REPO))
