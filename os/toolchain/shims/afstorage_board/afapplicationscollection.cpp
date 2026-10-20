/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:
*
*/
#ifndef PHOREPOC_S3_APPARC
#include <apgupdate.h>
#endif
#include <apgcli.h>

#include "afapplicationscollection.h"
//------------------------------------------------------------------------------
CAfApplicationsCollection* CAfApplicationsCollection::NewL(RApaLsSession& serviceProvider,
                                                           MAfApplicationsObserver* observer)
{
    CAfApplicationsCollection *self = new(ELeave) CAfApplicationsCollection(serviceProvider, observer);
    return self;
}

//------------------------------------------------------------------------------
CAfApplicationsCollection::CAfApplicationsCollection(RApaLsSession& serviceProvider, MAfApplicationsObserver* observer)
:
    mServiceProvider(serviceProvider),
    mObserver(observer)
{}

//------------------------------------------------------------------------------
CAfApplicationsCollection::~CAfApplicationsCollection()
{
    mDeinstalledApplications.Close();
}

//------------------------------------------------------------------------------
void CAfApplicationsCollection::applicationsChanged()
{
    TRAP_IGNORE(collectsApplicationsInfoL());
    if (mObserver) {
        mObserver->applicationsChanged();
    }
}

//------------------------------------------------------------------------------
TArray<TUid> CAfApplicationsCollection::removedApplications()const
{
    return mDeinstalledApplications.Array();
}

//------------------------------------------------------------------------------
void CAfApplicationsCollection::setObserver(MAfApplicationsObserver& observer)
{
    mObserver = &observer;
}

//------------------------------------------------------------------------------
#ifdef PHOREPOC_S3_APPARC
// PHOREPOC board delta (S4.7): our AppArc is Symbian^3's, which has no per-app update list (TApaAppUpdateInfo /
// RApaLsSession::UpdatedAppsInfoL came with Symbian^4's). The removed applications are the ones the previous full
// scan listed and this one does not (one collection per storage server: the previous list is file-local).
static RArray<TUid> sKnownApplications;
static TBool sKnownValid = EFalse;

void CAfApplicationsCollection::collectsApplicationsInfoL()
{
    mDeinstalledApplications.Close();
    RArray<TUid> present;
    CleanupClosePushL(present);
    User::LeaveIfError(mServiceProvider.GetAllApps());
    TApaAppInfo info;
    while (mServiceProvider.GetNextApp(info) == KErrNone) {
        present.AppendL(info.iUid);
    }
    if (sKnownValid) {
        for (TInt offset(0); offset < sKnownApplications.Count(); ++offset) {
            if (present.Find(sKnownApplications[offset]) == KErrNotFound) {
                mDeinstalledApplications.AppendL(sKnownApplications[offset]);
            }
        }
    }
    sKnownApplications.Close();
    for (TInt offset(0); offset < present.Count(); ++offset) {
        sKnownApplications.AppendL(present[offset]);
    }
    sKnownValid = ETrue;
    CleanupStack::PopAndDestroy(&present);
}
#else
void CAfApplicationsCollection::collectsApplicationsInfoL()
{
    mDeinstalledApplications.Close();
    RArray< TApaAppUpdateInfo > updatedApps;
    CleanupClosePushL(updatedApps);
    TInt errNo = mServiceProvider.UpdatedAppsInfoL(updatedApps);
    User::LeaveIfError(errNo);
    for (TInt offset(0); offset < updatedApps.Count(); ++offset) {
        const TApaAppUpdateInfo::TApaAppAction action(updatedApps[offset].iAction);
        switch (action) {
        case TApaAppUpdateInfo::EAppNotPresent:
            mDeinstalledApplications.AppendL(updatedApps[offset].iAppUid);
            break;
        //TODO add implementation for other cases when needed
        }
    }
    CleanupStack::PopAndDestroy(&updatedApps);
}
#endif
