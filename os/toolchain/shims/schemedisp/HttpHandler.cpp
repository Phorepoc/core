/*
* Copyright (c) 2002 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:
*      Implementation of Scheme handler interface implementation for http:// scheme
*
*      PHOREPOC: the release hard-coded the S60 browser (0x10008D39, embedded through BrowserLauncher). Here the
*      browser is the default application of the data type "x-scheme-handler/http" -- apparc's own default-app
*      mapping (RApaLsSession::AppForDataType; the Control Panel's "Default browser" item sets it with
*      InsertDataMapping). Applications declare the type in their registration (phebrowser does). With no app
*      mapped, the highest-priority declarer wins (apparc's own choice); with none at all, phebrowser's UID.
*      The URL goes as the document name: StartDocument when the browser is not running, the open-file
*      message (TApaTask::SwitchOpenFile) when it is. Embedded launches open standalone: no browser embeds.
*/

// INCLUDE FILES

#include "HttpHandler.h"
#include "SchemeDispLogger.h"
#include <ecom/ecom.h>		// For REComSession
#include <eikenv.h>
#include <apgcli.h>
#include <apparc.h>
#include <apmrec.h>
#include <eikdoc.h>
#include <eikproc.h>
#include <f32file.h>
#include <apgtask.h>
#include <w32std.h>
#include <e32debug.h>

_LIT8( KPheSchemeHttpType, "x-scheme-handler/http" );
LOCAL_C const TUid KUidPheBrowser = { 0xE7016B20 };	// phebrowser: when no application declares the type

// ================= MEMBER FUNCTIONS =======================

CHttpHandler* CHttpHandler::NewL( const TDesC& aUrl )
	{
	CLOG_ENTERFN( "CHttpHandler::NewL()" );
	CHttpHandler* self=new(ELeave) CHttpHandler();
	CleanupStack::PushL(self);
	self->ConstructL( aUrl );
	CleanupStack::Pop(self);
	CLOG_LEAVEFN( "CHttpHandler::NewL()" );
	return self;
	}

CHttpHandler::~CHttpHandler()
	{
	CLOG_ENTERFN( "CHttpHandler::~CHttpHandler()" );
	if(iDoc != NULL)
		{
		CEikProcess*  hostProcess = CEikonEnv::Static()->Process();
		hostProcess->DestroyDocument(iDoc);
		iDoc = NULL;
		}
    delete iLauncher;		// (PHOREPOC: never created)
	CLOG_LEAVEFN( "CHttpHandler::~CHttpHandler()" );
	}

CHttpHandler::CHttpHandler() : CBaseHandler()
	{
	}

void CHttpHandler::ConstructL( const TDesC& aUrl )
	{
	iParsedUrl = HBufC::NewL( aUrl.Length() );
	iParsedUrl->Des().Copy( aUrl );
	}

// PHOREPOC: no browser runs embedded; the link opens in the default browser
void CHttpHandler::HandleUrlEmbeddedL()
	{
	CLOG_ENTERFN( "CHttpHandler::HandleUrlEmbeddedL()" );
	HandleUrlStandaloneL();
	CLOG_LEAVEFN( "CHttpHandler::HandleUrlEmbeddedL()" );
	}

void CHttpHandler::HandleUrlStandaloneL()
	{
	CLOG_ENTERFN( "CHttpHandler::HandleUrlStandaloneL()" );
	RApaLsSession appArcSession;
	User::LeaveIfError( appArcSession.Connect() );
	CleanupClosePushL<RApaLsSession>( appArcSession );

	// PHOREPOC: the default browser = apparc's application for the scheme data type
	TUid browser = KNullUid;
	const TDataType type( KPheSchemeHttpType );
	if ( appArcSession.AppForDataType( type, browser ) != KErrNone || browser == KNullUid )
		browser = KUidPheBrowser;
	RDebug::Printf( "PH: [schemedisp] http(s) link -> app 0x%08x", browser.iUid );

    RWsSession wsSession;
	User::LeaveIfError( wsSession.Connect() );
	CleanupClosePushL<RWsSession>( wsSession );
    TApaTaskList taskList( wsSession );
    TApaTask task = taskList.FindApp( browser );
    if ( task.Exists() )
        {
        User::LeaveIfError( task.SwitchOpenFile( *iParsedUrl ) );
        task.BringToForeground();
        }
    else
        {
	    TThreadId id;
        User::LeaveIfError( appArcSession.StartDocument( *iParsedUrl, browser, id ) );
        }
    CleanupStack::PopAndDestroy( 2 );  // wsSession, appArcSession
	CLOG_LEAVEFN( "CHttpHandler::HandleUrlStandaloneL()" );
	}

void CHttpHandler::HandleServerAppExit(TInt aReason)
	{
	CLOG_ENTERFN( "CHttpHandler::HandleServerAppExit" );
	if( NULL != iSchemeDoc )
		{
	    iSchemeDoc->HandleServerAppExit( aReason );
		}
	CLOG_LEAVEFN( "CHttpHandler::HandleServerAppExit" );
	}
