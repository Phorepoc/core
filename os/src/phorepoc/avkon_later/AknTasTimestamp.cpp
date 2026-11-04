// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
//
// CAknTasHook's performance markers: an application (a clock, a calculator, a phonebook ...) calls AddTimestampL("<what just happened>")
// at the points a test harness measures (the application is ready, a view is up); the TAS test server reads the list back with
// GetTimestamps(). Binary compatibility with those applications: the export numbers of avkonu_later.def.
//
// A marker is its name and the home time in the form YYYYMMDDHHMMSSuuuuuu (20 characters). Markers are only kept while the TAS
// "hook storing" key is on (the same switch that decides whether the object hooks are stored); with it off every call returns at once.
// None of the calls leaves: a marker that cannot be made (no memory, no TAS repository) is dropped, as the callers expect.

#include "AknTasHook.h"
#include <coemain.h>
#include <e32base.h>

namespace
    {
    const TUid KUidTasTimestamps = { 0xE000DEC5 };   // the store's CCoeStatic key (the object hooks use 0xE000DEC4)
    const TInt KTimestampLength = 20;                // %Y%M%D%H%T%S%C = 4 + 2 + 2 + 2 + 2 + 2 + 6
    _LIT( KTimestampFormat, "%F%Y%M%D%H%T%S%C" );

    // The markers of one thread (one CCoeEnv), owned by it and deleted with it.
    class CTimestampStore : public CCoeStatic
        {
    public:
        static CTimestampStore* Self()
            {
            return static_cast<CTimestampStore*>( CCoeEnv::Static( KUidTasTimestamps ) );
            }

        static CTimestampStore* InstanceL()
            {
            CTimestampStore* store = Self();
            if ( !store )
                {
                store = new ( ELeave ) CTimestampStore;   // the CCoeStatic constructor registers it with the CCoeEnv
                }
            return store;
            }

        ~CTimestampStore()
            {
            iMarkers.ResetAndDestroy();
            }

        RPointerArray<CAknTasTimestampInfo> iMarkers;

    private:
        CTimestampStore() : CCoeStatic( KUidTasTimestamps ) {}
        };

    void DoAddTimestampL( TPtrC aMarker )
        {
        CAknTasHook* hook = CAknTasHook::InstanceL();
        if ( !hook->HookStoringEnabled() )
            {
            return;
            }
        CTimestampStore* store = CTimestampStore::InstanceL();
        CAknTasTimestampInfo* info = new ( ELeave ) CAknTasTimestampInfo;
        CleanupStack::PushL( info );
        info->ConstructL( aMarker );
        User::LeaveIfError( store->iMarkers.Append( info ) );
        CleanupStack::Pop( info );
        }
    }

CAknTasTimestampInfo::~CAknTasTimestampInfo()
    {
    delete iName;
    delete iTime;
    }

void CAknTasTimestampInfo::ConstructL( TPtrC aMarker )
    {
    iName = aMarker.AllocL();
    TTime now;
    now.HomeTime();
    iTime = HBufC::NewL( KTimestampLength );
    TPtr time( iTime->Des() );
    now.FormatL( time, KTimestampFormat );
    }

EXPORT_C const HBufC* CAknTasTimestampInfo::Name() const
    {
    return iName;
    }

EXPORT_C const HBufC* CAknTasTimestampInfo::Time() const
    {
    return iTime;
    }

EXPORT_C void CAknTasHook::AddTimestampL( TPtrC aMarker )
    {
    if ( CCoeEnv::Static() )
        {
        TRAP_IGNORE( DoAddTimestampL( aMarker ) );
        }
    }

EXPORT_C void CAknTasHook::RemoveTimestamp( TPtrC aMarker )
    {
    CTimestampStore* store = CCoeEnv::Static() ? CTimestampStore::Self() : NULL;
    if ( !store )
        {
        return;
        }
    for ( TInt i = store->iMarkers.Count() - 1; i >= 0; --i )
        {
        if ( *store->iMarkers[i]->Name() == aMarker )
            {
            delete store->iMarkers[i];
            store->iMarkers.Remove( i );
            }
        }
    }

EXPORT_C void CAknTasHook::ResetTimestamps()
    {
    CTimestampStore* store = CCoeEnv::Static() ? CTimestampStore::Self() : NULL;
    if ( store )
        {
        store->iMarkers.ResetAndDestroy();
        }
    }

EXPORT_C RPointerArray<CAknTasTimestampInfo>* CAknTasHook::GetTimestamps()
    {
    if ( !HookStoringEnabled() )
        {
        return NULL;
        }
    CTimestampStore* store = NULL;
    TRAPD( err, store = CTimestampStore::InstanceL() );
    return err == KErrNone ? &store->iMarkers : NULL;
    }
