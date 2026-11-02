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
// Name        : pthread_mutex_destroy.cpp
// Part of     : pthread
// Interface   : POSIX, pthread
// POSIX implementation of mutexes on Symbian
// Version     :
//
// OURS copy of genericopenlibs/openenvcore/libpthread/src/pthread_mutex_destroy.cpp (Phorepoc, 2026-11-02): "is it
// locked" is the mutex's lock word (mutex.cpp), no longer the semaphore bookkeeping.
//



#include "mutextypes.h"

EXPORT_C int pthread_mutex_destroy (pthread_mutex_t * mutex)
{

    if(!mutex)
    {
        return EINVAL;
    }

    if(mutex->iState == _EDestroyed)
    {
        return EINVAL;
    }

    if(mutex->iState != _EInitialized)
    {
        return 0;
    }

    _pthread_mutex_t* pMutex = mutex->iPtr;

    // claim it (free -> held) so no thread can take it while it is torn down; held = EBUSY, as POSIX allows
    if(!_phTryLock(pMutex))
    {
        return EBUSY;
    }

    mutex->iState = _EDestroyed;
    _closeMutex(mutex);
    return 0;
}


// End of File
