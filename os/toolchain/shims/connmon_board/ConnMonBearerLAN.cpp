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
* Description:  Implementation for bearer LAN.
*
*/

#include "ConnMonBearerLAN.h"
#include "connmoncommsdatcache.h"
#include "ConnMonIAP.h"
#include "log.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
//
TConnMonBearerLAN::TConnMonBearerLAN(
        CConnMonIAP* aConnMonIAP,
        CConnMonCommsDatCache* aCommsDatCache )
        :
        TConnMonBearer( aConnMonIAP, aCommsDatCache )
    {
    }

// ---------------------------------------------------------------------------
// Is bearer available
// ---------------------------------------------------------------------------
//
TInt TConnMonBearerLAN::GetAvailability( TBool& /*aAvailable*/ ) const
    {
    return KErrNotSupported;
    }

// ---------------------------------------------------------------------------
// Bearer ID getter
// ---------------------------------------------------------------------------
//
TUint TConnMonBearerLAN::BearerId() const
    {
    return EBearerIdLAN;
    }

// ---------------------------------------------------------------------------
// ETrue if bearer is valid (internal state is correct)
// ---------------------------------------------------------------------------
//
TBool TConnMonBearerLAN::Valid() const
    {
    return ETrue;
    }

// ---------------------------------------------------------------------------
// Sets available IAPs of this type as available in CommsDat cache
// ---------------------------------------------------------------------------
//
void TConnMonBearerLAN::FlagAvailableIaps() const
    {
    LOGENTRFN("TConnMonBearerLAN::FlagAvailableIaps()")

    RArray<TUint> idArray;
    iConnMonIAP->GetActiveConnectionsIds( EBearerIdLAN, idArray );

    if ( idArray.Count() > 0 )
        {
        LOGIT1("Found %d active LAN IAPs", idArray.Count())
        for ( TInt i = 0; i < idArray.Count(); i++ )
            {
            iCommsDatCache->SetAvailableIapWithId( idArray[i] );
            }
        }
    else
        {
        // Currently LAN bearer only supported in emulator, unless LAN IAP already active.
        // Phorepoc board delta (X4.68): Ethernet is this board's primary bearer, as on the emulator -- without
        // this, ConnMon reports no available IAP, MPM drops every LAN IAP of the Internet destination, and every
        // S60 connection start fails KErrNotFound
        LOGIT("Phorepoc board (as the emulator), enabling all LAN IAPs")
        iCommsDatCache->SetAvailableIapsWithBearerId( EBearerIdLAN );
        }
    idArray.Close();

    LOGEXITFN("TConnMonBearerLAN::FlagAvailableIaps()")
    }

// End-of-file
