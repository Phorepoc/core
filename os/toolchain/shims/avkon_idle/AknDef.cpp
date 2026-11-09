/*
* Copyright (c) 2002-2007 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Implementation to get the view ID of the
*                currently active idle view.
*
*/


// INCLUDE FILES
#include "AknDef.h"
#include <vwsdef.h> // TVwsViewId

#ifdef RD_IDLE_SEPARATION
#include <e32property.h>
#include <activeidle2domainpskeys.h>
#endif

const TInt KPhoneUid( 0x100058B3 );
const TInt KActiveIdle2Uid( 0x102750F0 );

// -----------------------------------------------------------------------------
// AknDef::GetPhoneIdleViewId()
//
// Constructs and returns the view id either from publish subscribe or from a 
// hardcoded value. This method replaces a constant definition in akndef.h.
// -----------------------------------------------------------------------------
//
EXPORT_C TInt AknDef::GetPhoneIdleViewId(TVwsViewId& aViewId)
    {
    TInt uidValue = NULL;
    TInt retVal = KErrNone;

#ifdef RD_IDLE_SEPARATION
    retVal = RProperty::Get( KPSUidAiInformation, KActiveIdleUid, uidValue );
    if(retVal == KErrNotFound)
        {
        retVal = KErrNone;
        uidValue = KPhoneUid;
        }
#else
    uidValue = KPhoneUid;  // Was defined as KAknPhoneIdleViewId in akndef.h.
#endif // RD_IDLE_SEPARATION

    if(retVal == KErrNone)
        {
        aViewId = TVwsViewId(TUid::Uid(uidValue), TUid::Uid(uidValue));
        }
        
#ifdef RD_BOOT_CUSTOMIZABLE_AI

    // ActiveIdle2 is view-based, so this is a temporary fix
    // in order to be able to activate idle from app shell with menu-key,
    // until a proper way for view uid acquisition is made.
    
    // PHOREPOC (marked change to RCL_3's AknDef.cpp, devmenu.md round 23): the range test below compared the published idle uid with
    // KAIActivePluginRangeStart, which activeidle2domainpskeys.h gives as the VALUE 0x1 -- and the code uses it as a P&S KEY, key 1 of
    // KPSUidAiInformation, which is KActiveIdleUid itself: "uid >= start" was always true, so EVERY published idle uid became ActiveIdle2
    // (0x102750F0, the S^3 ailaunch). Orbit's home screen
    // (hsapplication, 0x20022F35) publishes its own uid: the Menu key's return trip must go to IT. Only the ActiveIdle2 uid itself (and a plug-in
    // uid of the S^3 active idle framework, none is published) keeps the ActiveIdle2 view.
    if ( uidValue >= 0x10275100 && uidValue <= 0x1027FFFF )     // the S^3 Active Idle plug-in uid range (0x102750F0 itself is below)
        {
        uidValue = KActiveIdle2Uid;
        aViewId.iAppUid = TUid::Uid( uidValue );
        }

    if ( uidValue == KActiveIdle2Uid )
        {
        aViewId.iViewUid = TUid::Uid(1);
        }
        
#endif // RD_BOOT_CUSTOMIZABLE_AI

    return retVal; 
    }

//  End of File  
