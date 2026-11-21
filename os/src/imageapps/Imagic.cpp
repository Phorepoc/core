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
#include <eikstart.h>
#include "ImagicApp.h"
#include <apgtask.h>
#include <apgwgnam.h>
#include <apacmdln.h>
#include <eikenv.h>
#include <e32debug.h>

/**
 * factory function to create the Hello World Basic application class
 */
// PHOREPOC PATCH (not in the 2010 release). A second launch of a running app (File Manager -> StartDocument) never
// gets as far as CImagicAppUi: CAknApplication::PreDocConstructL finds the running instance, moves it to the front and
// leaves with KLeaveExit (CBaActiveScheduler::Exit), without passing the document on. The factory runs before that,
// so a second process hands its document name to the running one here (TApaTask::SwitchOpenFile ->
// CEikAppUi::ProcessMessageL -> CImagicAppUi::OpenFileL).
LOCAL_C void ForwardDocumentToRunningInstance()
	{
	CApaCommandLine* cl = NULL;
	if (CApaCommandLine::GetCommandLineFromProcessEnvironment(cl) != KErrNone || !cl)
		return;
	TFileName doc(cl->DocumentName());
	delete cl;
	RDebug::Printf("PH: imagic process start, document name length %d", doc.Length());
	if (doc.Length() == 0)
		return;
	CEikonEnv* env = CEikonEnv::Static();
	RWsSession& ws = env->WsSession();
	const TInt myWg = env->RootWin().Identifier();
	TInt wgId = 0;
	do
		{
		CApaWindowGroupName::FindByAppUid(KUidImagic, ws, wgId);
		if (wgId >= 0 && wgId != myWg)
			{
			TApaTask task(ws);
			task.SetWgId(wgId);
			const TInt r = task.SwitchOpenFile(doc);
			RDebug::Printf("PH: imagic handed the document to the running instance (wg %d) = %d", wgId, r);
			return;
			}
		} while (wgId >= 0);
	RDebug::Printf("PH: imagic no other instance running");
	}

LOCAL_C CApaApplication* NewApplication()
	{
	ForwardDocumentToRunningInstance();
	return new CImagicApp;
	}

/**
 * A normal Symbian OS executable provides an E32Main() function which is
 * called by the operating system to start the program.
 */
GLDEF_C TInt E32Main()
	{

	return EikStart::RunApplication( NewApplication );
	}

