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
    TInt phErr = pMutex->iSignalSemaphore.CreateLocal(1);
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
    mutex->iState = _EInitialized;
    return 0;
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


    int retval = 0;
    pthread_t self;
    switch(pMutex->iMutexType)
    {
        case  PTHREAD_MUTEX_NORMAL:
        	{
        	  self = pthread_self();          
        	  pMutex->iDataLock.Wait();
        	  if(mutex->iState == _EDestroyed)
        	  {
        	     pMutex->iDataLock.Signal();
        	     return EINVAL;
        	  }
        	  pMutex->iSemaphoreState++;   
        	  pMutex->iDataLock.Signal();
        	           
        	  retval = _doWait(&pMutex->iSignalSemaphore,abstime);
        	  if(retval == 0)
        	  {
        	     pMutex->iDataLock.Wait();
        	     if(mutex->iState == _EDestroyed)
        	     {
        	        pMutex->iDataLock.Signal();
        	        return EINVAL;
        	     }
        	     pMutex->iLockingThread  = self;
        	     pMutex->iDataLock.Signal();
               }
        	   break;
        	 }

        case  PTHREAD_MUTEX_RECURSIVE:
        	
        {
            self = pthread_self();
            if(pMutex->iLockingThread == self)
            {
                pMutex->iDataLock.Wait();
                if(mutex->iState == _EDestroyed)
                {
                   pMutex->iDataLock.Signal();
                   return EINVAL;
                }
                pMutex->iCount++;
                pMutex->iDataLock.Signal();
            }
            else
            {
                pMutex->iDataLock.Wait();
                if(mutex->iState == _EDestroyed)
                {
                   pMutex->iDataLock.Signal();
                   return EINVAL;
                }
                pMutex->iSemaphoreState++;   
                pMutex->iDataLock.Signal();
                
                retval = _doWait(&pMutex->iSignalSemaphore,abstime);
                if(retval == 0)
                {
                    pMutex->iDataLock.Wait();
                    if(mutex->iState == _EDestroyed)
                    {
                      pMutex->iDataLock.Signal();
                      return EINVAL;
                    }
                    pMutex->iCount = 1;
                    pMutex->iLockingThread = self;
                    pMutex->iDataLock.Signal();
                }
            }
            break;
        }                          

        case  PTHREAD_MUTEX_ERRORCHECK:
        {
            self = pthread_self();
            if(pMutex->iLockingThread == self)
            {
                return EDEADLK;
            }
            else
            {
                pMutex->iDataLock.Wait();
                if(mutex->iState == _EDestroyed)
                {
                   pMutex->iDataLock.Signal();
                   return EINVAL;
                }
                pMutex->iSemaphoreState++;     
                pMutex->iDataLock.Signal();
                
                retval = _doWait(&pMutex->iSignalSemaphore,abstime);
                if(retval == 0)
                {
                  pMutex->iDataLock.Wait();
                  if(mutex->iState == _EDestroyed)
                  {
                     pMutex->iDataLock.Signal();
                     return EINVAL;
                  }
                  pMutex->iLockingThread  = self;
                  pMutex->iDataLock.Signal();
                }
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
