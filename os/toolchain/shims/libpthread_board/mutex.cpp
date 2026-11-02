// Copyright (c) 2005-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the Eclipse Public License v2.0, available at
// https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
// section 7 of the Eclipse Public License v1.0).
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License, v. 2.0 are satisfied: GNU General Public License, version 2
// or later.
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
// The Phorepoc Authors
//
// Description:
// Name        : mutex.cpp
// Part of     : pthread
// Interface   : POSIX, pthread
// POSIX implementation of mutexes on Symbian
// Version     :
//
// OURS copy of genericopenlibs/openenvcore/libpthread/src/mutex.cpp (Phorepoc, 2026-10-13; emitter/components.py
// libpthread drop/extra). One fix, in _InitOnce: the static-initializer race. It read mutex->iState twice -- the
// "already initialised?" test, then the switch -- so a thread that lost the race (another thread completed the lazy init
// between the two reads) reached the switch's default and returned false: pthread_mutex_lock -> EINVAL. WebKit's WTF
// WordLock hands the first lock of a fresh std::mutex to another thread, and libstdc++ turned the EINVAL into an
// uncaught std::system_error (`[cxx] THROW std::system_error: Invalid argument (errno 22)` from WordLock::unlockSlow).
// Now the state is read once; "initialised meanwhile" is success, and _EInvalid (another thread mid-init: _mutexCreate
// sets it first) waits on the init lock instead of failing -- the actual cause, found with the [pthread] lines below.
//



#include "mutextypes.h"
#include <e32debug.h>   // Phorepoc: EINVAL diagnostics (2026-10-13)
static TInt gPhMutexCreates = 0;

/*
This function waits on the aLock semaphore. 
*/
int _doWait(RSemaphore* aLock, const struct timespec *abstime)
{
    int retval = 0;
    if(!abstime)
    {
        aLock->Wait();
    }
    else
    {
        long lSleepTime = _microsleeptime(abstime);
        if(lSleepTime <0)
        {
            if (EINVAL == errno)
            {
                errno = 0;
                return EINVAL;    
            }
            else if(ETIMEDOUT == errno)
            {
                errno = 0;
                return ETIMEDOUT;
            }
            
        }

        retval = aLock->Wait(lSleepTime);
        if(retval != KErrNone)
        {
            return  ETIMEDOUT;
        }

    }
    return 0;
}

/*
This internal function destroys the mutex object.
*/
void _closeMutex(pthread_mutex_t* mutex)
{
    _pthread_mutex_t* pMutex = mutex->iPtr;
    if(pMutex)
    {
        if(pMutex->iSemaphoreCreated)
        {
            pMutex->iSignalSemaphore.Close();       
        }
        if(pMutex->iDataLockCreated)
        {
            pMutex->iDataLock.Close();  
        }
        mutex->iPtr = NULL;
        delete pMutex; 
    }
    return;
}

int _mutexCreate (pthread_mutex_t * mutex, 
                  const pthread_mutexattr_t* attr)
{

    if( attr->iSharedType != PTHREAD_PROCESS_PRIVATE)
    {
        return ENOSYS;
    }

    mutex->iState = _EInvalid;
    _pthread_mutex_t* pMutex = new _pthread_mutex_t; 
    mutex->iPtr = pMutex;
    if(!pMutex)
    {
        return ENOMEM;
    }

    pMutex->iSharedType = attr->iSharedType;
    pMutex->iMutexType = attr->iMutexType;
    pMutex->iDataLockCreated = false;
    pMutex->iSemaphoreCreated = false;

    ++gPhMutexCreates;
    TInt phErr = pMutex->iSignalSemaphore.CreateLocal(0);	// Phorepoc: waiters only (the lock is iWord)
    if(phErr != KErrNone)
    {
        RDebug::Printf("[pthread] mutex #%d: RSemaphore::CreateLocal %d", gPhMutexCreates, phErr);
        _closeMutex(mutex);
        return EAGAIN;
    }
    pMutex->iSemaphoreCreated = true;

    phErr = pMutex->iDataLock.CreateLocal();
    if(phErr != KErrNone)
    {
        RDebug::Printf("[pthread] mutex #%d: RFastLock::CreateLocal %d", gPhMutexCreates, phErr);
        _closeMutex(mutex);
        return EAGAIN;
    }
    pMutex->iDataLockCreated = true;

    pMutex->iCount = 0;
    pMutex->iSemaphoreState = SEM_FREE;
    pMutex->iLockingThread = 0;
    pMutex->iWord = 0;
    __e32_memory_barrier();		// Phorepoc: the fields above are visible before another thread can see _EInitialized
    mutex->iState = _EInitialized;
    return 0;
}

/*
Phorepoc (2026-11-02): the lock word. 0 = free, 1 = held, 2 = held and a thread may be waiting on iSignalSemaphore.
A locker takes 0 -> 1 with one atomic (no exec). Under contention it sets 2 and waits on the semaphore; the unlocker
swaps 0 in and signals the semaphore only if it took a 2 out. A woken waiter sets 2 again before re-trying, so a
release can never be missed, and a stray semaphore count (a timed waiter that gave up after its wake was signalled)
only costs a later waiter one extra round.
*/
TBool _phTryLock(_pthread_mutex_t* aMutex)
{
    TUint32 expected = 0;
    return __e32_atomic_cas_acq32(&aMutex->iWord, &expected, 1);
}

static int _phLock(_pthread_mutex_t* aMutex, const struct timespec* abstime)
{
    if (_phTryLock(aMutex))
    {
        return 0;
    }
    for (;;)
    {
        if (__e32_atomic_swp_acq32(&aMutex->iWord, 2) == 0)
        {
            return 0;
        }
        int r = _doWait(&aMutex->iSignalSemaphore, abstime);
        if (r != 0)
        {
            return r;		// ETIMEDOUT / EINVAL: iWord stays 2, the next unlock signals once (harmless)
        }
    }
}

void _phUnlock(_pthread_mutex_t* aMutex)
{
    if (__e32_atomic_swp_rel32(&aMutex->iWord, 0) == 2)
    {
        aMutex->iSignalSemaphore.Signal();
    }
}


/*
This function is an internal  harness function used to resolve the race to dynamically initialize a static mutex
*/
bool _InitOnce(pthread_mutex_t* mutex)
{

    pthread_mutexattr_t defaultattributes = 
                            PTHREAD_MUTEX_ATTRIBUTE_DEFAULT_INITIALIZER;
    pthread_mutexattr_t recursiveattributes = 
                            PTHREAD_MUTEX_ATTRIBUTE_RECURSIVE_INITIALIZER;
    pthread_mutexattr_t errorcheckattributes = 
                            PTHREAD_MUTEX_ATTRIBUTE_ERRORCHECK_INITIALIZER;

    // Phorepoc: one read of the state (another thread may complete the lazy init at any moment)
    const _handle_state state = *(volatile _handle_state*)&mutex->iState;
    if(state == _EInitialized)
    {
        return true;
    }

    mutex->iReentry = 0xBABA;

    pthread_mutexattr_t* attr;
    switch(state)
    {
        case _ENeedsNormalInit:
        {
            attr = &defaultattributes;
            break;
        }

        case _ENeedsRecursiveInit:
        {
            attr = &recursiveattributes;
            break;
        }

        case _ENeedsErrorCheckInit:
        {
            attr = &errorcheckattributes;
            break;
        }

        // Phorepoc: _mutexCreate marks the mutex _EInvalid while it initialises it (iState = _EInvalid first,
        // _EInitialized last), under the atomic lock below. Seeing _EInvalid here = another thread is mid-init: wait for it
        // on that lock, then the re-check below finds _EInitialized (or retries a failed create). The original returned
        // false -> EINVAL (WTF ParkingLot::unparkOneImpl locking a parked thread's fresh std::mutex, 2026-10-13).
        case _EInvalid:
        {
            attr = &defaultattributes;
            break;
        }

        default:
            return false;
    }

#ifndef NOUSE_INTERNALS
    void* tlsPtr;

    // Get the TLS pointer
    tlsPtr = _pthread_getTls(); 

    if(!tlsPtr)
    {
        return false;
    }
#endif    

    int retval=0;
#ifndef NOUSE_INTERNALS
    _pthread_atomicMutexLock(tlsPtr);
#endif
    if(mutex->iState != _EInitialized)
    {
        retval = _mutexCreate(mutex,attr);
    }
#ifndef NOUSE_INTERNALS   
    _pthread_atomicMutexUnlock(tlsPtr);
#endif   
     
    if(retval == 0)
    {
        return true;
    }
    else 
    {
        return false;
    }
}


/*
This function is an internal function used to provide the POSIX mutex lock 
functionality.
*/
int _internalMutexLock(pthread_mutex_t * mutex,const struct timespec *abstime)
{
    if( !VALID_MUTEX(mutex) )
    {
        RDebug::Printf("[pthread] lock EINVAL: mutex %x state %d ptr %x (%d created) from %x", (TUint)mutex, mutex ? (TInt)mutex->iState : 0,
            mutex ? (TUint)mutex->iPtr : 0, gPhMutexCreates, (TUint)__builtin_return_address(0));
        return EINVAL;
    }

    _pthread_mutex_t* pMutex = mutex->iPtr;

    // Phorepoc (2026-11-02): the lock is pMutex->iWord (_phLock above); the data lock and the semaphore are no longer
    // taken on the uncontended path. A NORMAL mutex keeps no owner (POSIX leaves unlocking one you do not hold
    // undefined; this saves pthread_self's TLS exec on every lock and unlock). RECURSIVE / ERRORCHECK track it.
    int retval = 0;
    switch(pMutex->iMutexType)
    {
        case  PTHREAD_MUTEX_NORMAL:
            retval = _phLock(pMutex, abstime);
            break;

        case  PTHREAD_MUTEX_RECURSIVE:
        {
            pthread_t self = pthread_self();
            if(pMutex->iLockingThread == self)
            {
                pMutex->iCount++;		// only the holder gets here
                break;
            }
            retval = _phLock(pMutex, abstime);
            if(retval == 0)
            {
                pMutex->iCount = 1;
                pMutex->iLockingThread = self;
            }
            break;
        }

        case  PTHREAD_MUTEX_ERRORCHECK:
        {
            pthread_t self = pthread_self();
            if(pMutex->iLockingThread == self)
            {
                return EDEADLK;
            }
            retval = _phLock(pMutex, abstime);
            if(retval == 0)
            {
                pMutex->iLockingThread = self;
            }
            break;
        }

        default:
            retval = ENOSYS;
            break;
    }
    return retval;
}

//End of File
