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
#include <ecamuids.hrh>

#include <mm/mmpluginutils.h>
#include "phe_ecamresolve.h"

//
// CCameraInfo
//

CCameraInfoPlugin* CCameraInfoPlugin::NewL()
	{
	TUid interfaceUid = {KUidOnboardCameraInfoPlugin};
	CCameraInfoPlugin* self = 
		static_cast<CCameraInfoPlugin*>
			(REComSession::CreateImplementationL(PheEcamSelectL(interfaceUid), _FOFF(CCameraInfoPlugin,iDestructorKey)));	// PHOREPOC: EcamPluginSupport.cpp
	return self;

	}
	
EXPORT_C CCameraInfoPlugin::~CCameraInfoPlugin()
	{
	REComSession::DestroyedImplementation(iDestructorKey);
	}

EXPORT_C CCameraInfoPlugin::CCameraInfoPlugin()
	{
	}
