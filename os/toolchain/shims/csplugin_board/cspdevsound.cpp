// Phorepoc board copy (OURS delta, telephony T1): ROM has no MODM DevSound codec and no
// TelephonyAudioRouting server yet (call audio = stage T7), so a missing audio route must not stop the CS call provider.
/*
* Copyright (c) 2006 Nokia Corporation and/or its subsidiary(-ies). 
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
* Description:  Wrapper for CMMFDevSound
*
*/


#include "cspdevsound.h"
#include "csplogger.h"
#include "mcspdevsoundobserver.h"

#include <AudioPreference.h>
#include <S60FourCC.h>

// ======== MEMBER FUNCTIONS ========

// ---------------------------------------------------------------------------
// CSPDevSound
// ---------------------------------------------------------------------------
//
CSPDevSound::CSPDevSound( MCSPDevSoundObserver& aObserver ) : 
    iObserver( aObserver )
    {
    }

// ---------------------------------------------------------------------------
// ConstructL
// ---------------------------------------------------------------------------
//
void CSPDevSound::ConstructL( 
    TMMFState aMode, 
    TUint aAudioPreference,
    TUint aAudioPriority )
    {
    CSPLOGSTRING( CSPINT, "CSPDevSound::ConstructL; Entry" );

    TFourCC modemFourCC;
    modemFourCC.Set(KS60FourCCCodeModem);

    iDevSound = CMMFDevSound::NewL();
#ifndef __WINSCW__
    // Phorepoc T1: no 'MODM' DevSound codec in the ROM (it is the stage-T7 plug-in): InitializeL leaves KErrNotSupported.
    // Keep the (uninitialised) DevSound so the call provider constructs; Activate() is already TRAPped by its callers.
    TRAPD( initErr, iDevSound->InitializeL( *this, modemFourCC, aMode  ) );
    (void)initErr;
#endif
    iPrioritySettings.iPriority = aAudioPriority;
    iPrioritySettings.iPref = 
        static_cast<TMdaPriorityPreference>( aAudioPreference );
    iPrioritySettings.iState = aMode;    
    CSPLOGSTRING( CSPINT, "CSPDevSound::ConstructL; Exit" );
    }
    
// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
CSPDevSound::~CSPDevSound()
    {
    delete iDevSound;
    }
    
// ---------------------------------------------------------------------------
// Tries to activate the audio stream if not active or activating
// ---------------------------------------------------------------------------
//
void CSPDevSound::Activate() 
    {
    CSPLOGSTRING( CSPINT, "CSPDevSound::Activate" );
    if( !IsActive() && !IsActivationOngoing() )
        {
        iActivationOngoing = ETrue;
        TInt err = KErrNone; 
        TRAP( err, DoActivateL() );
        CSPLOGSTRING2( CSPINT, "CSPDevSound::Activate %d", err );
        }
    }
    
// ---------------------------------------------------------------------------
// Deactivates the audio device.
// ---------------------------------------------------------------------------
//
void CSPDevSound::Deactivate()
    {
    CSPLOGSTRING( CSPINT, "CSPDevSound::Deactivate" );
    if( IsActive() || IsActivationOngoing() )
        {
    	CSPLOGSTRING( CSPINT, "CSPDevSound::Deactivate Stopping" );
        iDevSound->Stop();
        iActive = EFalse;
        iActivationOngoing = EFalse;
        }
    }
    
// ---------------------------------------------------------------------------
// ActivationOngoing
// ---------------------------------------------------------------------------
//
TBool CSPDevSound::IsActivationOngoing() const 
    {
    return iActivationOngoing;
    }
    
// ---------------------------------------------------------------------------
// IsActive
// ---------------------------------------------------------------------------
//
TBool CSPDevSound::IsActive() const
    {
    return iActive;
    }
    
CMMFDevSound& CSPDevSound::DevSound()
    {
    return *iDevSound;
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::InitializeComplete( TInt aError ) 
    {
    CSPLOGSTRING( CSPINT, "CSPDevSound::InitializeComplete" );
    if( aError == KErrNone )
        {
        iDevSound->SetPrioritySettings( iPrioritySettings );   
            
        }
        
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::BufferToBeFilled( CMMFBuffer* /*aBuffer*/ )
    {
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::PlayError( TInt /*aError*/ )
    {
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::ToneFinished( TInt /*aError*/ )
    {
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::BufferToBeEmptied( CMMFBuffer* /*aBuffer*/ )
    {
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::RecordError( TInt /*aError*/ )
    {
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::ConvertError( TInt /*aError*/ )
    {
    }

// ---------------------------------------------------------------------------
// From class MDevSoundObserver
// Not implemented
// ---------------------------------------------------------------------------
//
void CSPDevSound::DeviceMessage( 
    TUid /*aMessageType*/, 
    const TDesC8& /*aMsg*/ )
    {
    }
 
//  End of File

