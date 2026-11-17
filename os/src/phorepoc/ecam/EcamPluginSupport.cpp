// Copyright (c) 2003-2009 Nokia Corporation and/or its subsidiary(-ies).
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
//

#include <ecam/ecamplugin.h>
#include <ecam/ecaminfoplugin.h>

#include <ecom/ecom.h>
#include <mm/mmpluginutils.h>

#include <ecamuids.hrh>
#include "phe_ecamresolve.h"

// PHOREPOC: camera plug-ins may also come from an installed package (a SIS on C:), not only from ROM. The release
// resolved them with KRomOnlyResolverUid (and with the default resolver in debug builds). Now every registered
// implementation is a candidate and the highest version_no wins, so an installed driver package (a camera driver
// Catalogue package) replaces the image's fallback plug-in. What makes this safe is the platform's own rules, not the
// resolver: ECom reads registrations on C: only from \resource\plugins\, which only the software installer writes,
// and the loader refuses a plug-in DLL whose capabilities are not a superset of the client process's, so a plug-in
// installed from a SIS the installer did not trust with those capabilities cannot load into a camera client.
static void PheResetAndDestroy(TAny* aArray) { ((RImplInfoPtrArray*)aArray)->ResetAndDestroy(); }

TUid PheEcamSelectL(TUid aInterfaceUid)
	{
	RImplInfoPtrArray impls;
	CleanupStack::PushL(TCleanupItem(PheResetAndDestroy, &impls));
	TEComResolverParams params;
	params.SetDataType(KECamPluginMatchString);
	params.SetWildcardMatch(ETrue);
	REComSession::ListImplementationsL(aInterfaceUid, params, impls);
	TUid best = KNullUid;
	TInt bestVersion = -1;
	for (TInt i = 0; i < impls.Count(); ++i)
		{
		const CImplementationInformation* info = impls[i];
		if (info->Disabled())
			continue;
		const TBool rom = info->RomBased();
		// highest version; on a tie the installed one (it is the newer package)
		if (info->Version() > bestVersion || (info->Version() == bestVersion && !rom))
			{
			best = info->ImplementationUid();
			bestVersion = info->Version();
			}
		}
	CleanupStack::PopAndDestroy();	// impls
	if (best == KNullUid)
		User::Leave(KErrNotFound);
	return best;
	}

//
// CCameraPlugin
//

EXPORT_C TInt CCameraPlugin::CamerasAvailable()
	{
	CCameraInfoPlugin* info = NULL;
	TRAPD(error, info = CCameraInfoPlugin::NewL());
	if (error!=KErrNone)
		{
		// error during open, default to 0
		// TODO - can we return an error here?
		return 0;
		}
	TInt result = info->CamerasAvailable();
	delete info;
	REComSession::FinalClose(); // don't have to do this here, but might as well tidy up
	return result;
	}
	
CCameraPlugin* CCameraPlugin::NewL(TInt aCameraVersion)
	{
	TUid interfaceUid = {KUidOnboardCameraPlugin};
	TUid dtor;

	CCameraPlugin* self = 
		static_cast<CCameraPlugin*>
			(REComSession::CreateImplementationL(PheEcamSelectL(interfaceUid), dtor));	// PHOREPOC: see PheEcamSelectL

	//create CCameraStructure here
	self->iCameraStructure = new CCameraStructure();
	if (!self->iCameraStructure)
		{
		delete self;
		REComSession::DestroyedImplementation(dtor);
		User::Leave(KErrNoMemory);
		}
	else
		{
		self->iCameraStructure->iCameraVersion = aCameraVersion;
	 	self->iCameraStructure->iDestructorKey = dtor;	
		}
	return self;
	}

CCameraPlugin* CCameraPlugin::NewLC(TInt aCameraVersion)
	{
	CCameraPlugin* self = NewL(aCameraVersion);
	CleanupStack::PushL(self);
	return self;	
	}

EXPORT_C CCameraPlugin* CCameraPlugin::NewL(MCameraObserver2& aObserver,TInt aCameraIndex,TInt aPriority,TInt aCameraVersion)
	{
	CCameraPlugin* self = NewLC(aCameraVersion);
	self->Construct2L(aObserver, aCameraIndex, aPriority);
	CleanupStack::Pop(self);
	return self;
	}

EXPORT_C CCameraPlugin* CCameraPlugin::NewDuplicateL(MCameraObserver2& aObserver,TInt aCameraHandle,TInt aCameraVersion)
	{
	CCameraPlugin* self = NewLC(aCameraVersion);
	self->Construct2DupL(aObserver, aCameraHandle);
	CleanupStack::Pop(self);
	return self;
	}


EXPORT_C CCameraPlugin* CCameraPlugin::NewL(MCameraObserver& aObserver,TInt aCameraIndex,TInt aCameraVersion)
	{
	CCameraPlugin* self = NewLC(aCameraVersion);
	self->Construct2L(aObserver, aCameraIndex);
	CleanupStack::Pop(self);
	return self;
	}

EXPORT_C CCameraPlugin* CCameraPlugin::NewDuplicateL(MCameraObserver& aObserver,TInt aCameraHandle,TInt aCameraVersion)
	{
	CCameraPlugin* self = NewLC(aCameraVersion);
	self->Construct2DupL(aObserver, aCameraHandle);
	CleanupStack::Pop(self);
	return self;
	}

EXPORT_C CCameraPlugin::~CCameraPlugin()
	{
	if (iCameraStructure)
		{
		REComSession::DestroyedImplementation(iCameraStructure->iDestructorKey);
		}
	delete iCameraStructure;
	}
	
EXPORT_C CCameraPlugin::CCameraPlugin()
	{
	}
	
EXPORT_C TInt CCameraPlugin::CameraVersion()
	{
	return iCameraStructure->iCameraVersion;
	}

