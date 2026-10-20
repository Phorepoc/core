// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// gcc_abi.cpp -- OURS: the RVCT-only pieces of the release's drtaeabi.dll, written for GCC
// (kernel/eka/compsupp/rvct/{drtaeabi,aeabimem,type_info}.cpp use __asm blocks, $Sub$$ symbol aliasing and
// #pragma import, which GCC has none of). Same functions, same exports;.
//
// What is NOT here because it is GCC's own, linked in from libsupc++/libgcc and exported at the release's ordinals:
// the RTTI class vtables/typeinfos (fundamental, pointer, class, si/vmi ...), __dynamic_cast, __cxa_vec_*, the
// __aeabi_vec_* helpers, __cxa_guard_*, the integer __aeabi_* helpers (idiv, ldivmod, lasr ...), unaligned access.
#include <e32std.h>
#include <e32panic.h>
#include <typeinfo>

extern "C" {

// ---- rvct/drtaeabi.cpp
EXPORT_C void abort()
	{
	User::RaiseException(EExcAbort);
	}

EXPORT_C void __cxa_pure_virtual()
	{
	User::Panic(_L("Pure virtual"), EPureVirtualCalled);
	}

// typeid(*p) of a polymorphic object: the ARM C++ ABI keeps the type_info* in the word before the vtable's first entry
EXPORT_C void* __get_typeid(void* aObject)
	{
	return (*(void***)aObject)[-1];
	}

EXPORT_C void* __ARM_get_typeid(void* p)
	{
	return __get_typeid(p);
	}

// the C string compare libsupc++'s type_info::operator== uses (the release got it from RVCT's c_5.l)
int strcmp(const char* a, const char* b)
	{
	while (*a && *a == *b)
		{
		++a;
		++b;
		}
	return (unsigned char)*a - (unsigned char)*b;
	}

// ---- libsupc++'s __cxa_bad_cast / __cxa_bad_typeid THROW std::bad_cast / std::bad_typeid through GCC's own exception
// runtime; the release's (cppsemantics.cpp, arm_exceptions_bad_cast_c) are the Symbian ones and come from there.
} // extern "C"

// ---- sized operator delete (C++14, which libsupc++'s type_info deleting destructors call) -> the unsized one, which is
// scppnwdl.dll's (toolchain/dll_rt/dll_crt.cpp has the same leaves for the other images)
void operator delete(void* aPtr, unsigned int) __NO_THROW { operator delete(aPtr); }
void operator delete[](void* aPtr, unsigned int) __NO_THROW { operator delete[](aPtr); }

// ---- rvct/type_info.cpp: the out-of-line members GCC's <typeinfo> defines inline (the exports keep their ordinals).
// type_info is { vptr, const char* __type_name } in the ARM C++ ABI, the same in GCC and RVCT.
extern "C" const char* TypeInfoName(const void* aThis) __asm__("_ZNKSt9type_info4nameEv");
extern "C" const char* TypeInfoName(const void* aThis)
	{
	return ((const char* const*)aThis)[1];
	}

// operator!= : the release's is !(*this == rhs) via the exported operator==
extern "C" bool TypeInfoEq(const void* aThis, const void* aRhs) __asm__("_ZNKSt9type_infoeqERKS_");
extern "C" bool TypeInfoNe(const void* aThis, const void* aRhs) __asm__("_ZNKSt9type_infoneERKS_");
extern "C" bool TypeInfoNe(const void* aThis, const void* aRhs)
	{
	return !TypeInfoEq(aThis, aRhs);
	}

// std::__gen_dummy_typeinfos(): RVCT's hook that makes its linker generate the typeinfos of the fundamental types
// (an empty function; the typeinfos themselves are libsupc++'s fundamental_type_info.o)
namespace std { void __gen_dummy_typeinfos() {} }

// ---- std::bad_cast / std::bad_typeid: GCC defines the constructors, copy constructor and assignment inline (so no
// out-of-line symbol exists), the release exports them. Real bodies: set the vptr (the object has no other state), and
// -- as every ARM EABI constructor does -- return `this`.
#define BAD_CLASS(NAME, LEN, VTABLE) \
	__asm__( \
		".text\n.arm\n.align 2\n" \
		".global _ZNSt" #LEN #NAME "C1Ev\n.global _ZNSt" #LEN #NAME "C2Ev\n" \
		".global _ZNSt" #LEN #NAME "C1ERKS_\n.global _ZNSt" #LEN #NAME "C2ERKS_\n" \
		".type _ZNSt" #LEN #NAME "C1Ev, %function\n.type _ZNSt" #LEN #NAME "C2Ev, %function\n" \
		".type _ZNSt" #LEN #NAME "C1ERKS_, %function\n.type _ZNSt" #LEN #NAME "C2ERKS_, %function\n" \
		"_ZNSt" #LEN #NAME "C1Ev:\n_ZNSt" #LEN #NAME "C2Ev:\n_ZNSt" #LEN #NAME "C1ERKS_:\n_ZNSt" #LEN #NAME "C2ERKS_:\n" \
		"	ldr r1, 1f\n" \
		"	str r1, [r0]\n"			/* vptr = &vtable + 8 (past offset-to-top and typeinfo slots) */ \
		"	bx lr\n" \
		"1:	.word " VTABLE " + 8\n" \
		".global _ZNSt" #LEN #NAME "aSERKS_\n.type _ZNSt" #LEN #NAME "aSERKS_, %function\n" \
		"_ZNSt" #LEN #NAME "aSERKS_:\n" \
		"	bx lr\n")					/* operator=: returns *this, nothing to copy */
BAD_CLASS(bad_cast, 8, "_ZTVSt8bad_cast");
BAD_CLASS(bad_typeid, 10, "_ZTVSt10bad_typeid");

// ---- rvct/aeabimem.cpp: the __aeabi_mem* entry points call euser's memset/memclr/memcpy/memmove (note the argument
// order of __aeabi_memset: (dest, n, value))
__asm__(
	".text\n.arm\n.align 2\n"
	".global __aeabi_memset\n.global __aeabi_memset4\n.global __aeabi_memset8\n"
	".global __aeabi_memclr\n.global __aeabi_memclr4\n.global __aeabi_memclr8\n"
	".global __aeabi_memcpy\n.global __aeabi_memcpy4\n.global __aeabi_memcpy8\n"
	".global __aeabi_memmove\n.global __aeabi_memmove4\n.global __aeabi_memmove8\n"
	".type __aeabi_memset, %function\n.type __aeabi_memset4, %function\n.type __aeabi_memset8, %function\n"
	".type __aeabi_memclr, %function\n.type __aeabi_memclr4, %function\n.type __aeabi_memclr8, %function\n"
	".type __aeabi_memcpy, %function\n.type __aeabi_memcpy4, %function\n.type __aeabi_memcpy8, %function\n"
	".type __aeabi_memmove, %function\n.type __aeabi_memmove4, %function\n.type __aeabi_memmove8, %function\n"
	"__aeabi_memset:\n__aeabi_memset4:\n__aeabi_memset8:\n"
	"	mov r3, r1\n"
	"	mov r1, r2\n"
	"	mov r2, r3\n"
	"	b memset\n"
	"__aeabi_memclr8:\n__aeabi_memclr4:\n__aeabi_memclr:\n"
	"	b memclr\n"
	"__aeabi_memcpy8:\n__aeabi_memcpy4:\n__aeabi_memcpy:\n"
	"	b memcpy\n"
	"__aeabi_memmove8:\n__aeabi_memmove4:\n__aeabi_memmove:\n"
	"	b memmove\n");
