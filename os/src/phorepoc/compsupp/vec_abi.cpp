// vec_abi.cpp -- OURS: the array construction/destruction helpers of the C++ ABI (Itanium C++ ABI 3.3.2 with the ARM
// C++ ABI's cookie: { element_size, element_count }, 8 bytes before the array) and the ARM RTABI 4.4.3 __aeabi_vec_*
// wrappers over them -- drtaeabi ordinals 165-176 and 192-201. The release took these from RVCT's cpprt_5.l; libsupc++'s
// vec.o is not used because its exception path reads GCC's own __cxa_eh_globals layout (caughtExceptions first), which
// is not the Symbian runtime's (symbian_support.h). Also the thread-aware __cxa_guard_* (ordinals 186-188), whose
// libsupc++ twin needs the atomics a bare-metal ARMv5 libgcc lacks.
//
// Allocation failure is Symbian's: operator new[] returns NULL, so does __cxa_vec_new*.
#include <e32std.h>
#include <e32panic.h>
#include <e32atomics.h>
#include <exception>

typedef unsigned int size_t_;
typedef void* (*TCtor)(void*);
typedef void* (*TCctor)(void*, void*);
typedef void* (*TDtor)(void*);
typedef void* (*TAlloc)(size_t_);
typedef void (*TDealloc)(void*);
typedef void (*TDealloc3)(void*, size_t_);

extern "C" {

EXPORT_C void __cxa_vec_cleanup(void* aArray, size_t_ aCount, size_t_ aSize, TDtor aDtor);
EXPORT_C void __cxa_vec_dtor(void* aArray, size_t_ aCount, size_t_ aSize, TDtor aDtor);

// ---- construction

EXPORT_C void* __cxa_vec_ctor(void* aArray, size_t_ aCount, size_t_ aSize, TCtor aCtor, TDtor aDtor)
	{
	size_t_ done = 0;
	char* p = (char*)aArray;
	if (aCtor)
		{
		try
			{
			for (; done < aCount; ++done, p += aSize)
				aCtor(p);
			}
		catch (...)
			{
			// destroy what was built, in reverse; a destructor that throws during this unwinding is terminate()
			__cxa_vec_cleanup(aArray, done, aSize, aDtor);
			throw;
			}
		}
	return aArray;
	}

EXPORT_C void* __cxa_vec_cctor(void* aDest, void* aSrc, size_t_ aCount, size_t_ aSize, TCctor aCctor, TDtor aDtor)
	{
	size_t_ done = 0;
	char* d = (char*)aDest;
	char* s = (char*)aSrc;
	if (aCctor)
		{
		try
			{
			for (; done < aCount; ++done, d += aSize, s += aSize)
				aCctor(d, s);
			}
		catch (...)
			{
			__cxa_vec_cleanup(aDest, done, aSize, aDtor);
			throw;
			}
		}
	return aDest;
	}

EXPORT_C void* __cxa_vec_new3(size_t_ aCount, size_t_ aSize, size_t_ aPadding, TCtor aCtor, TDtor aDtor,
							  TAlloc aAlloc, TDealloc3 aDealloc)
	{
	// size = count * element size + cookie; an overflow is an allocation failure
	unsigned long long total = (unsigned long long)aCount * aSize + aPadding;
	if (total > 0xFFFFFFFFu)
		return NULL;
	char* base = (char*)aAlloc((size_t_)total);
	if (!base)
		return NULL;
	char* array = base + aPadding;
	if (aPadding)
		{
		((size_t_*)array)[-2] = aSize;
		((size_t_*)array)[-1] = aCount;
		}
	try
		{
		__cxa_vec_ctor(array, aCount, aSize, aCtor, aDtor);
		}
	catch (...)
		{
		aDealloc(base, (size_t_)total);
		throw;
		}
	return array;
	}

EXPORT_C void* __cxa_vec_new2(size_t_ aCount, size_t_ aSize, size_t_ aPadding, TCtor aCtor, TDtor aDtor,
							  TAlloc aAlloc, TDealloc aDealloc)
	{
	unsigned long long total = (unsigned long long)aCount * aSize + aPadding;
	if (total > 0xFFFFFFFFu)
		return NULL;
	char* base = (char*)aAlloc((size_t_)total);
	if (!base)
		return NULL;
	char* array = base + aPadding;
	if (aPadding)
		{
		((size_t_*)array)[-2] = aSize;
		((size_t_*)array)[-1] = aCount;
		}
	try
		{
		__cxa_vec_ctor(array, aCount, aSize, aCtor, aDtor);
		}
	catch (...)
		{
		aDealloc(base);
		throw;
		}
	return array;
	}

EXPORT_C void* __cxa_vec_new(size_t_ aCount, size_t_ aSize, size_t_ aPadding, TCtor aCtor, TDtor aDtor)
	{
	TAlloc alloc = &operator new[];
	TDealloc dealloc = &operator delete[];
	return __cxa_vec_new2(aCount, aSize, aPadding, aCtor, aDtor, alloc, dealloc);
	}

// ---- destruction

// destroy aCount elements, last first; any exception from a destructor here is fatal (this runs during unwinding)
EXPORT_C void __cxa_vec_cleanup(void* aArray, size_t_ aCount, size_t_ aSize, TDtor aDtor)
	{
	if (!aDtor)
		return;
	char* p = (char*)aArray + (unsigned long long)aCount * aSize;
	try
		{
		while (aCount--)
			{
			p -= aSize;
			aDtor(p);
			}
		}
	catch (...)
		{
		std::terminate();
		}
	}

// destroy aCount elements, last first. A destructor that throws does not stop the rest being destroyed; the (first)
// exception is then propagated -- unless an exception is already propagating (terminate)
EXPORT_C void __cxa_vec_dtor(void* aArray, size_t_ aCount, size_t_ aSize, TDtor aDtor)
	{
	if (!aDtor)
		return;
	bool unwinding = std::uncaught_exception();
	char* p = (char*)aArray + (unsigned long long)aCount * aSize;
	try
		{
		while (aCount--)
			{
			p -= aSize;
			aDtor(p);
			}
		}
	catch (...)
		{
		if (unwinding)
			std::terminate();
		__cxa_vec_cleanup(aArray, aCount, aSize, aDtor);		// the elements below the one that threw
		throw;
		}
	}

EXPORT_C void __cxa_vec_delete3(void* aArray, size_t_ aSize, size_t_ aPadding, TDtor aDtor, TDealloc3 aDealloc)
	{
	if (!aArray)
		return;
	char* base = (char*)aArray - aPadding;
	size_t_ count = aPadding ? ((size_t_*)aArray)[-1] : 0;
	size_t_ total = count * aSize + aPadding;
	try
		{
		__cxa_vec_dtor(aArray, count, aSize, aDtor);
		}
	catch (...)
		{
		aDealloc(base, total);
		throw;
		}
	aDealloc(base, total);
	}

EXPORT_C void __cxa_vec_delete2(void* aArray, size_t_ aSize, size_t_ aPadding, TDtor aDtor, TDealloc aDealloc)
	{
	if (!aArray)
		return;
	char* base = (char*)aArray - aPadding;
	size_t_ count = aPadding ? ((size_t_*)aArray)[-1] : 0;
	try
		{
		__cxa_vec_dtor(aArray, count, aSize, aDtor);
		}
	catch (...)
		{
		aDealloc(base);
		throw;
		}
	aDealloc(base);
	}

EXPORT_C void __cxa_vec_delete(void* aArray, size_t_ aSize, size_t_ aPadding, TDtor aDtor)
	{
	TDealloc dealloc = &operator delete[];
	__cxa_vec_delete2(aArray, aSize, aPadding, aDtor, dealloc);
	}

// ---- ARM RTABI 4.4.3: the compiler's entry points (cookie = { element size, element count } before the array)

EXPORT_C void* __aeabi_vec_ctor_nocookie_nodtor(void* aArray, TCtor aCtor, size_t_ aSize, size_t_ aCount)
	{
	return __cxa_vec_ctor(aArray, aCount, aSize, aCtor, NULL);
	}

EXPORT_C void* __aeabi_vec_ctor_cookie_nodtor(void* aCookie, TCtor aCtor, size_t_ aSize, size_t_ aCount)
	{
	if (!aCookie)
		return NULL;
	((size_t_*)aCookie)[0] = aSize;
	((size_t_*)aCookie)[1] = aCount;
	void* array = (size_t_*)aCookie + 2;
	__cxa_vec_ctor(array, aCount, aSize, aCtor, NULL);
	return array;
	}

EXPORT_C void* __aeabi_vec_cctor_nocookie_nodtor(void* aDest, void* aSrc, size_t_ aSize, size_t_ aCount, TCctor aCctor)
	{
	return __cxa_vec_cctor(aDest, aSrc, aCount, aSize, aCctor, NULL);
	}

EXPORT_C void* __aeabi_vec_new_cookie_noctor(size_t_ aSize, size_t_ aCount)
	{
	return __cxa_vec_new(aCount, aSize, 2 * sizeof(size_t_), NULL, NULL);
	}

EXPORT_C void* __aeabi_vec_new_nocookie(size_t_ aSize, size_t_ aCount, TCtor aCtor)
	{
	return __cxa_vec_new(aCount, aSize, 0, aCtor, NULL);
	}

EXPORT_C void* __aeabi_vec_new_cookie_nodtor(size_t_ aSize, size_t_ aCount, TCtor aCtor)
	{
	return __cxa_vec_new(aCount, aSize, 2 * sizeof(size_t_), aCtor, NULL);
	}

EXPORT_C void* __aeabi_vec_new_cookie(size_t_ aSize, size_t_ aCount, TCtor aCtor, TDtor aDtor)
	{
	return __cxa_vec_new(aCount, aSize, 2 * sizeof(size_t_), aCtor, aDtor);
	}

// returns the address of the cookie
EXPORT_C void* __aeabi_vec_dtor(void* aArray, TDtor aDtor, size_t_ aSize, size_t_ aCount)
	{
	__cxa_vec_dtor(aArray, aCount, aSize, aDtor);
	return (size_t_*)aArray - 2;
	}

EXPORT_C void* __aeabi_vec_dtor_cookie(void* aArray, TDtor aDtor)
	{
	if (!aArray)
		return NULL;
	size_t_ size = ((size_t_*)aArray)[-2];
	size_t_ count = ((size_t_*)aArray)[-1];
	__cxa_vec_dtor(aArray, count, size, aDtor);
	return (size_t_*)aArray - 2;
	}

EXPORT_C void __aeabi_vec_delete(void* aArray, TDtor aDtor)
	{
	if (!aArray)
		return;
	__cxa_vec_delete(aArray, ((size_t_*)aArray)[-2], 2 * sizeof(size_t_), aDtor);
	}

EXPORT_C void __aeabi_vec_delete3(void* aArray, TDtor aDtor, TDealloc3 aDealloc)
	{
	if (!aArray)
		return;
	__cxa_vec_delete3(aArray, ((size_t_*)aArray)[-2], 2 * sizeof(size_t_), aDtor, aDealloc);
	}

EXPORT_C void __aeabi_vec_delete3_nodtor(void* aArray, TDealloc3 aDealloc)
	{
	if (!aArray)
		return;
	__cxa_vec_delete3(aArray, ((size_t_*)aArray)[-2], 2 * sizeof(size_t_), NULL, aDealloc);
	}

// ---- guards for function-local statics (ARM C++ ABI: a 32-bit guard, bit 0 of the first byte = initialised;
// the second byte is ours: the initialisation is running). A second thread arriving while one runs waits for it.

EXPORT_C int __cxa_guard_acquire(int* aGuard)
	{
	volatile TUint8* g = (volatile TUint8*)aGuard;
	if (g[0])
		return 0;
	// take the running flag; if another thread holds it, wait for that initialisation to finish (or be aborted)
	while (__e32_atomic_swp_ord8(&g[1], 1))
		{
		if (g[0])
			return 0;
		User::After(1000);
		}
	if (g[0])						// finished while we were taking the flag
		{
		g[1] = 0;
		return 0;
		}
	return 1;
	}

EXPORT_C void __cxa_guard_release(int* aGuard)
	{
	volatile TUint8* g = (volatile TUint8*)aGuard;
	__e32_memory_barrier();
	g[0] = 1;
	g[1] = 0;
	}

EXPORT_C void __cxa_guard_abort(int* aGuard)
	{
	volatile TUint8* g = (volatile TUint8*)aGuard;
	g[1] = 0;
	}

} // extern "C"
