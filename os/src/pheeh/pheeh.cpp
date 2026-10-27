// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheeh.cpp -- the parts of the exception runtime that GCC's archives expect from the C library and the dynamic linker.
// pheeh.dll depends on euser only: the unwinder's libc needs (memcpy, malloc, getenv, abort) are met here, and the
// per-thread exception state (__cxa_get_globals) lives in Dll::Tls instead of a TLS segment an E32 image does not have.
#include <e32std.h>

typedef unsigned int size_t_;

extern "C" {

void* memcpy(void* d, const void* s, size_t_ n)
	{
	unsigned char* dp = (unsigned char*)d; const unsigned char* sp = (const unsigned char*)s;
	while (n--) *dp++ = *sp++;
	return d;
	}

void* memmove(void* d, const void* s, size_t_ n)
	{
	unsigned char* dp = (unsigned char*)d; const unsigned char* sp = (const unsigned char*)s;
	if (dp < sp) { while (n--) *dp++ = *sp++; }
	else { dp += n; sp += n; while (n--) *--dp = *--sp; }
	return d;
	}

void* memset(void* d, int c, size_t_ n)
	{
	unsigned char* dp = (unsigned char*)d;
	while (n--) *dp++ = (unsigned char)c;
	return d;
	}

int memcmp(const void* a, const void* b, size_t_ n)
	{
	const unsigned char* x = (const unsigned char*)a; const unsigned char* y = (const unsigned char*)b;
	for (; n; --n, ++x, ++y) if (*x != *y) return *x < *y ? -1 : 1;
	return 0;
	}

size_t_ strlen(const char* s)
	{
	const char* p = s; while (*p) ++p;
	return (size_t_)(p - s);
	}

// (strcmp: the rtti runtime, rtti_rt.cpp, defines it for every module)

int strncmp(const char* a, const char* b, size_t_ n)
	{
	for (; n; --n, ++a, ++b) { if (*a != *b) return (unsigned char)*a - (unsigned char)*b; if (!*a) break; }
	return 0;
	}

void* malloc(size_t_ n) { return User::Alloc((TInt)n); }
void free(void* p) { User::Free(p); }
void* calloc(size_t_ a, size_t_ b) { void* p = User::Alloc((TInt)(a * b)); if (p) memset(p, 0, a * b); return p; }
char* getenv(const char*) { return 0; }
void abort() { User::Panic(_L("pheeh"), 1); for (;;) {} }

char* secure_getenv(const char*) { return 0; }
unsigned long __isoc23_strtoul(const char*, char**, int) { return 0; }
void __stack_chk_fail() { abort(); }
char* strchr(const char* s, int c)
	{
	for (;; ++s) { if (*s == (char)c) return (char*)s; if (!*s) return 0; }
	}

// libgcc / libsupc++ built with gthreads guard the frame registry and the emergency pool with pthread mutexes, and treat
// the presence of __pthread_key_create as "threads are on". The E32 image has no libpthread: these are the minimum they use.
// A mutex is a zero-initialised word (PTHREAD_MUTEX_INITIALIZER), taken with a short spin: the critical sections are a
// list walk. A condition wait may return spuriously, which its callers re-check.
int __pthread_key_create(unsigned*, void (*)(void*)) { return 0; }
int pthread_mutex_lock(volatile int* m) { while (__sync_lock_test_and_set(m, 1)) __builtin_ia32_pause(); return 0; }
int pthread_mutex_unlock(volatile int* m) { __sync_lock_release(m); return 0; }
int pthread_once(volatile int* c, void (*fn)(void))
	{
	if (__sync_bool_compare_and_swap(c, 0, 1)) { fn(); __sync_synchronize(); *c = 2; }
	else while (*c != 2) __builtin_ia32_pause();
	return 0;
	}
int pthread_cond_wait(void*, void*) { return 0; }
int pthread_cond_broadcast(void*) { return 0; }

// An E32 image has no program headers: the registered frames (__register_frame_info) are the only source.
int dl_iterate_phdr(int (*)(void*, unsigned, void*), void*) { return 0; }
// libgcc_eh (glibc >= 2.35 build) asks this first; -1 = not found, fall through to the registry.
int _dl_find_object(void*, void*) { return -1; }

// libsupc++'s per-thread state (caught / uncaught exceptions), one block per thread in Dll::Tls.
struct __cxa_eh_globals { void* caughtExceptions; unsigned int uncaughtExceptions; };

__cxa_eh_globals* __cxa_get_globals_fast()
	{
	return (__cxa_eh_globals*)Dll::Tls();
	}

__cxa_eh_globals* __cxa_get_globals()
	{
	__cxa_eh_globals* g = (__cxa_eh_globals*)Dll::Tls();
	if (!g)
		{
		g = (__cxa_eh_globals*)calloc(1, sizeof(__cxa_eh_globals));
		if (!g) abort();
		Dll::SetTls(g);
		}
	return g;
	}

} // extern "C"

// The default terminate handler of libsupc++ is the verbose one, which prints through stdio.
namespace __gnu_cxx { void __verbose_terminate_handler() { abort(); } }
