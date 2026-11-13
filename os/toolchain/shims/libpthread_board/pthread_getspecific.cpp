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
// Name     : pthread_getspecific.cpp
// Part of  : PThread library
// pthread_getspecific call is implemented.
// Version:
//



#include <pthread.h>
#include <errno.h>
#include "threadglobals.h"
#include "pthreadmisc.h"

EXPORT_C void* pthread_getspecific(pthread_key_t key)
{
    _global_data_t *glbPtr;
    _pthread_node_t *selfNodePtr;
    _pkey_node_t *tlsPtr;
    void * result;

    THR_PRINTF("[pthread] Begin pthread_getspecific\n");

    if (key >= PTHREAD_KEYS_MAX)
    {
        THR_PRINTF("[pthread] End pthread_getspecific\n");
        return NULL;
    }

    //Get the TLS value (self node pointer)
    selfNodePtr = (_pthread_node_t*) _pthread_getTls();
	//coverity[var_compare_op]
    if (NULL == selfNodePtr)    // This should never happen
    {
        THR_PRINTF("[pthread] FATAL :TLS is not initialized \n");
        THR_PRINTF("[pthread] Terminating the process\n");
        RProcess rp;
        rp.Kill(0);                // Terminate the process
    }

	//coverity[var_deref_op]
    glbPtr = selfNodePtr->glbDataPtr;  // point to global struct
    // Phorepoc (2026-11-13): no locks here. The thread table lock and the node lock were two RMutexes = four execs per
    // call (WebKit's thread-locals made Exec::MutexWait/MutexSignal half of the engine's CPU). Lock-free is safe because
    // a thread's key list is only appended to (with a barrier), key nodes are never freed while the thread lives (pthread_key_delete
    // clears the value and keeps the node), and an insert publishes a fully built node (pthread_key_create.cpp).

    int idx;
    idx = key / 32;

    int bitPos;
    bitPos = key % 32;

    if (((glbPtr->statusflag[idx]) & (0x1<<bitPos)) == _KEY_UNUSED)
    {
        THR_PRINTF("[pthread] End pthread_getspecific\n");
        return NULL;
    }

    for (tlsPtr = selfNodePtr->tlsHead; tlsPtr != NULL; tlsPtr = tlsPtr->next)
    {
        if (tlsPtr->keyNumber == key)
        {
                result = tlsPtr->tls;
                break;
        }
    }

    if (tlsPtr == NULL) // First node
    {
        // Phorepoc: without the table lock this is the window of a pthread_key_create in another thread (the key's
        // bit is set before its node reaches this thread's list): the value is NULL then, as for a fresh key
        return NULL;
    }

    THR_PRINTF("[pthread] End pthread_getspecific\n");
    return result;
}
// End of file
