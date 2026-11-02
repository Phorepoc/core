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
// Name        : pthread_mutex_unlock.cpp
// Part of     : pthread
// Interface   : POSIX, pthread
// POSIX implementation of mutexes on Symbian
// Version     :
//
// OURS copy of genericopenlibs/openenvcore/libpthread/src/pthread_mutex_unlock.cpp (Phorepoc, 2026-11-02): the lock is
// the mutex's lock word (mutex.cpp _phUnlock: one atomic, the semaphore only for waiters). A NORMAL mutex keeps no
// owner, so its unlock reads no TLS; RECURSIVE / ERRORCHECK check the owner as before (EPERM).
//



#include "mutextypes.h"

EXPORT_C int pthread_mutex_unlock (pthread_mutex_t * mutex)
{
    if( !VALID_MUTEX(mutex) )
    {
        return EINVAL;
    }

    _pthread_mutex_t* pMutex = mutex->iPtr;
    int retval = 0;
    switch(pMutex->iMutexType)
    {
        case  PTHREAD_MUTEX_NORMAL:
            _phUnlock(pMutex);
            break;

        case  PTHREAD_MUTEX_RECURSIVE:
        {
            if(pMutex->iLockingThread != pthread_self())
            {
                retval = EPERM;
                break;
            }
            if(--pMutex->iCount == 0)
            {
                pMutex->iLockingThread = 0;
                _phUnlock(pMutex);
            }
            break;
        }

        case  PTHREAD_MUTEX_ERRORCHECK:
        {
            if(pMutex->iLockingThread != pthread_self())
            {
                retval = EPERM;
                break;
            }
            pMutex->iLockingThread = 0;
            _phUnlock(pMutex);
            break;
        }

        default:
            retval = ENOSYS;
            break;
    }
    return retval;
}


// End of File
