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
* Contributors: Juha Kauppinen, Mika Hokkanen
* 
* Description: Photo Browser
*
*/


// INCLUDE FILES
#include    "ImagicApp.h"
#include    "ImagicDocument.h"
#include <apgwgnam.h>
#include <e32debug.h>

 

// PHOREPOC PATCH (not in the 2010 release). A file opened from File Manager while Photo Browser is already running
// starts a second process (RApaLsSession::StartDocument). CAknApplication::PreDocConstructL finds the running instance,
// moves it to the front and leaves with KLeaveExit, so the new process ends before the framework has given it the document
// (the name and the file handle are only reachable in CImagicAppUi::ProcessCommandParametersL). So a second process is
// let through here; CImagicAppUi builds nothing, hands the document to the running instance
// (TApaTask::SwitchOpenFile -> CEikAppUi::ProcessMessageL -> OpenFileL) and ends.
TBool gPheSecondInstance = EFalse;

// ================= MEMBER FUNCTIONS =======================

// ---------------------------------------------------------
// CImagicApp::AppDllUid()
// Returns application UID
// ---------------------------------------------------------
//
TUid CImagicApp::AppDllUid() const
    {
    return KUidImagic;
    }

   
// ---------------------------------------------------------
// CImagicApp::CreateDocumentL()
// Creates CImagicDocument object
// ---------------------------------------------------------
//
CApaDocument* CImagicApp::CreateDocumentL()
    {
 
       return (static_cast<CApaDocument*>
                       ( CImagicDocument::NewL( *this ) ) );
    }




    

void CImagicApp::PreDocConstructL()
    {
    CEikonEnv* env = CEikonEnv::Static();
    RWsSession& ws = env->WsSession();
    const TInt myWg = env->RootWin().Identifier();
    TInt wgId = 0;
    do
        {
        CApaWindowGroupName::FindByAppUid(AppDllUid(), ws, wgId);
        if (wgId >= 0 && wgId != myWg)
            {
            gPheSecondInstance = ETrue;
            break;
            }
        } while (wgId >= 0);
    RDebug::Printf("PH: imagic PreDocConstructL, a first instance is running: %d", gPheSecondInstance);
    if (gPheSecondInstance)
        CEikApplication::PreDocConstructL();
    else
        CAknApplication::PreDocConstructL();
    }

// End of File  

