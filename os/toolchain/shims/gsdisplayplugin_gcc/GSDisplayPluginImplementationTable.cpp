/*
* Copyright (c) 2005 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  ECOM proxy table for GSDisplayPlugin
*
*/


// INCLUDES
// PHOREPOC-DELTA: no power-save display plug-in -- its API (power_save_display_mode) was never
// released; the S60 variant's FF_POWER_SAVE would list CPSMDisplayPlugin here
#undef FF_POWER_SAVE
#include "GSDisplayPlugin.h"

#ifdef FF_POWER_SAVE
    #include "PSMDisplayPlugin.h"
#endif // FF_POWER_SAVE

#include <e32std.h>
#include <implementationproxy.h>


// CONSTANTS
const TImplementationProxy KGSDisplayPluginImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY( 0x10207441,	CGSDisplayPlugin::NewL )
#ifdef FF_POWER_SAVE
    ,
	IMPLEMENTATION_PROXY_ENTRY( 0x2000B592,	CPSMDisplayPlugin::NewL )
#endif // FF_POWER_SAVE    
	};


// ---------------------------------------------------------------------------
// ImplementationGroupProxy
//
// Gate/factory function
// ---------------------------------------------------------------------------
//
EXPORT_C const TImplementationProxy* ImplementationGroupProxy(
                                                  TInt& aTableCount )
	{
	aTableCount = sizeof( KGSDisplayPluginImplementationTable )
        / sizeof( TImplementationProxy );
	return KGSDisplayPluginImplementationTable;
	}


// End of File
