/*
* Copyright (c) 2005-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the Eclipse Public License v2.0, available at
* https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
* section 7 of the Eclipse Public License v1.0).
* This Source Code may also be made available under the following Secondary
* Licenses when the conditions for such availability set forth in the Eclipse
* Public License, v. 2.0 are satisfied: GNU General Public License, version 2
* or later.
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
* The Phorepoc Authors
*
* Description:
* Name        : mutextypes.h
* Part of     : pthread
* Interface   : POSIX, pthread
* POSIX implementation of mutexes on Symbian
* Version     :
*
* OURS copy of genericopenlibs/openenvcore/libpthread/inc/mutextypes.h (Phorepoc, 2026-11-02; registry libpthread
* incs_first, so every libpthread TU sees this layout). The data lock that guards each mutex's bookkeeping is an
* RFastLock instead of an RMutex: it is held only for a few instructions inside these functions, so it is almost never
* contended, and an RFastLock takes and releases it with one atomic each (an exec only when another thread holds it).
* The RMutex cost two execs per Wait/Signal pair, four per pthread_mutex_lock + unlock; Mesa's mutexes made that
* ~11 % of WSERV's GL worker while scrolling the app grid.
* The mutex itself (iWord, same date) is a lock word with the semaphore behind it for waiters only: an uncontended
* pthread_mutex_lock + unlock takes no exec at all (it was a semaphore Wait + Signal, plus pthread_self's TLS read).
*/



#ifndef  MUTEXTYPES_H
#define  MUTEXTYPES_H

#include <stddef.h>
#include <limits.h>
#include <e32def.h>
#include <e32std.h>

#include <errno.h>
#include <stdlib.h>

#include "semaphore.h"
#include "pthread.h"
#include "sysif.h"

// RMutex's interface as libpthread uses it. IsHeld (pthread_mutex_destroy's "does this thread hold the data lock"):
// never true there -- no function returns with the data lock held -- so EFalse is the RMutex answer too.
class RPhDataLock : public RFastLock
{
public:
    inline TBool IsHeld() const { return EFalse; }
};

struct _pthread_mutex_t
{
    int iSharedType;
    int iMutexType;
    RPhDataLock iDataLock;
    bool  iDataLockCreated;
    RSemaphore iSignalSemaphore;
    bool iSemaphoreCreated;
    int iCount; //for recursive mutexes
    int iSemaphoreState;
    pthread_t iLockingThread;
    // the lock itself (Phorepoc, 2026-11-02): 0 free, 1 held, 2 held with waiters. Taken and released with one atomic
    // each; iSignalSemaphore (count 0 at creation) is only waited on / signalled under contention (mutex.cpp _phLock)
    volatile TUint32 iWord;
	/*******************************************************************
	Overloading new and delete operators so that they will
	allocate and deallocare memory from/to the private heap of backend
	********************************************************************/
	inline TAny* operator new(TUint aSize, TAny* aBase) __NO_THROW
		{
		Mem::FillZ(aBase, aSize); return aBase;
		}
		
	inline TAny* operator new(TUint aSize) __NO_THROW
		{
		return Backend()->Alloc(aSize);
		}
		
	inline TAny* operator new(TUint aSize, TLeave)
		{
		TAny* ptr = Backend()->Alloc(aSize);
		if (ptr == NULL)
			{
			User::Leave(KErrNoMemory);
			}
		return ptr;
		}
		
	inline TAny* operator new(TUint aSize, TUint aExtraSize) __NO_THROW
		{
		return Backend()->Alloc(aSize + aExtraSize);
		}
		
	inline TAny* operator new(TUint aSize, TLeave, TUint aExtraSize)
		{
		TAny* ptr = Backend()->Alloc(aSize + aExtraSize);
		if (ptr == NULL)
			{
			User::Leave(KErrNoMemory);
			}
		return ptr;
		}
	
	inline void operator delete(TAny *aPtr) __NO_THROW
		{
		Backend()->Free( aPtr );
		}
};  


#define SEM_FREE -1

#define PTHREAD_MUTEX_ATTRIBUTE_DEFAULT_INITIALIZER  { _EInitialized, PTHREAD_PROCESS_PRIVATE,PTHREAD_MUTEX_DEFAULT }
#define PTHREAD_MUTEX_ATTRIBUTE_RECURSIVE_INITIALIZER  { _EInitialized, PTHREAD_PROCESS_PRIVATE,PTHREAD_MUTEX_RECURSIVE }
#define PTHREAD_MUTEX_ATTRIBUTE_ERRORCHECK_INITIALIZER  { _EInitialized, PTHREAD_PROCESS_PRIVATE,PTHREAD_MUTEX_ERRORCHECK }


#define VALID_MUTEX(mutex) (!mutex || !(_InitOnce(mutex)) || !(mutex->iPtr) || (mutex)->iState != _EInitialized) ? 0 : 1 
#define VALID_MUTEX_ATTR(attr) ( !attr || attr->iState != _EInitialized) ? 0 : 1

extern void* _pthread_getTls();
extern int _pthread_atomicMutexLock(void *tlsPtr);
extern int _pthread_atomicMutexUnlock(void *tlsPtr);
extern long _microsleeptime(const struct timespec* abstime);


extern int _doWait(RSemaphore* aLock, const struct timespec *abstime);

extern bool _InitOnce(pthread_mutex_t* mutex);

extern int _internalMutexLock(pthread_mutex_t * mutex,const struct timespec *abstime);

// the lock word (mutex.cpp): trylock = 0 -> 1; unlock = -> 0, signalling the semaphore when there were waiters
extern TBool _phTryLock(_pthread_mutex_t* aMutex);
extern void _phUnlock(_pthread_mutex_t* aMutex);

extern void _closeMutex(pthread_mutex_t* mutex);

#endif  // MUTEXTYPES_H
