// OURS (2026-10-28): the stub plug-in's header + iPulseEnd, for src/hwrm/VibraPlugin.cpp
/*
* Copyright (c) 2002-2005 Nokia Corporation and/or its subsidiary(-ies). 
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
* Description:  Hardware Resource Manager stub plugins Vibra plugin header
*
*/


#ifndef __VIBRAPLUGIN_H_
#define __VIBRAPLUGIN_H_

#include <hwrmpluginservice.h>
#include "PluginTimer.h"

#ifdef PUBLISH_STATE_INFO
#include <e32property.h>
#include <HWRMVibra.h>
const TUint32 KHWRMTestVibraCommand             = 0x11110001;
const TUint32 KHWRMTestVibraDataPckg            = 0x11110002;
#endif // PUBLISH_STATE_INFO

class CPluginTimer;

class CVibraPlugin : public CHWRMPluginService,
                     public MPluginTimerCallback
    {
    public:

        static CVibraPlugin* NewL();
        ~CVibraPlugin();

        /**
        * Method to invoke a particular command in the plugin
        * @param aCommandId Command ID
        * @param aTransId   Transaction ID
        * @param aData      Data associated with command
        */
        virtual void ProcessCommandL( const TInt aCommandId,
                                      const TUint8 aTransId,
                                      TDesC8& aData );

        /**
        * Method to cancel a particular command
        * @param aTransId   Transaction ID
        * @param aCommandId Command ID to optionally double check with the 
        *                   transaction ID
        */
        virtual void CancelCommandL( const TUint8 aTransId, 
                                     const TInt aCommandId);

        // From MPluginTimerCallback
        void GenericTimerFired(MHWRMPluginCallback* aService,
                         const TInt aCommandId,
                         const TUint8 aTransId,
                         TInt aRetVal);

    private: // Construction

        CVibraPlugin();
        void ConstructL();
        
        RPointerArray<CPluginTimer> iTimers;
        CPluginTimer* iPulseEnd;    // OURS: the running pulse's end (owned; NULL = none)
        
#ifdef PUBLISH_STATE_INFO
        RProperty iCmdProperty;   // for publishing command ID
        RProperty iDataProperty;  // for publishing data
#endif // PUBLISH_STATE_INFO        

    };

#endif // __VIBRAPLUGIN_H_