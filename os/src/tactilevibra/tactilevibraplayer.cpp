/*
* Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Class for producing vibra feedback.
* Part of:      Tactile Feedback.
*
*/

#include <e32debug.h>
#include <hwrmvibra.h>
#include <centralrepository.h>
#include <ecom/implementationproxy.h>
#include <ProfileInternal.hrh>

#include "tactilefeedbackprivatecrkeys.h"
#include "tactilefeedbacktrace.h"

#include "tactilevibraplayer.h"
#include "OstTraceDefinitions.h"
#ifdef OST_TRACE_COMPILER_IN_USE
#include "tactilevibraplayerTraces.h"
#endif


// ---------------------------------------------------------------------------
// Constructor.
// ---------------------------------------------------------------------------
//
CTactileVibraPlayer::CTactileVibraPlayer( CRepository& aRepository ):
        iRepository ( aRepository )
    {    
    }

// ---------------------------------------------------------------------------
// 2nd phase constructor.
// ---------------------------------------------------------------------------
//
// OURS (2026-10-28): KTactileFeedbackHapticsStrength is a 0-100 strength in this release (its cenrep default is 100;
// the haptics player scales it by 100), but this player was written for the older 0-3 level and LEFT on anything
// above 3 -- so the vibra player was never created. Map the strength onto the levels.
static TInt StrengthToLevel( TInt aStrength )
    {
    if ( aStrength <= 0 )
        return EProfileTactileFeedbackOff;
    if ( aStrength <= EProfileTactileFeedbackLevel3 )
        return aStrength;                   // (an old-style level written as such)
    return aStrength <= 33 ? EProfileTactileFeedbackLevel1 :
           aStrength <= 66 ? EProfileTactileFeedbackLevel2 : EProfileTactileFeedbackLevel3;
    }

void CTactileVibraPlayer::ConstructL()
    {
    TRACE("CTactileVibraPlayer::ConstructL - Begin");

    User::LeaveIfError( iRepository.Get( KTactileFeedbackHapticsStrength, 
                                         iVibraLevel ) );
    iVibraLevel = StrengthToLevel( iVibraLevel );
                                         
    if ( iVibraLevel > EProfileTactileFeedbackLevel3 )
        {
        User::Leave( KErrGeneral );
        }
        
    ReadSettings();

    iCenRepNotifier = CCenRepNotifyHandler::NewL( *this, 
                                                  iRepository, 
                                                  CCenRepNotifyHandler::EIntKey,
                                                  KTactileFeedbackHapticsStrength );
    iCenRepNotifier->StartListeningL();     

    TRACE("CTactileVibraPlayer::ConstructL - End");
    }

// ---------------------------------------------------------------------------
// 2-phased constructor.
// ---------------------------------------------------------------------------
//
CTactileVibraPlayer* CTactileVibraPlayer::NewL( CRepository& aRepository )
    {
    CTactileVibraPlayer* self = 
        new ( ELeave ) CTactileVibraPlayer ( aRepository );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    return self;
    }

// ---------------------------------------------------------------------------
// Destructor.
// ---------------------------------------------------------------------------
//
CTactileVibraPlayer::~CTactileVibraPlayer()
    {
    delete iCenRepNotifier;
    delete iVibra;
    iVibraParams.Close();
    }


// ---------------------------------------------------------------------------
// From class CTactilePlayer
//
// Duration and intensity for the vibra are selected from the array
// according to logical feedback type.
//
// Feedback is not produced in case duration or intensity is zero.
// ---------------------------------------------------------------------------
//
TInt CTactileVibraPlayer::PlayFeedback( TTouchLogicalFeedback aFeedback )
    {
    TInt ret(KErrNotReady);
    if ( iVibraLevel != EProfileTactileFeedbackOff )
        {
        TInt effectIndex = KErrNotFound;
        
        switch ( aFeedback )
            {
            case ETouchFeedbackBasic:               // flow through
            case ETouchFeedbackBasicButton:         // flow through
            case ETouchFeedbackList:                // flow through
            case ETouchFeedbackBoundaryList:        // flow through
            case ETouchFeedbackSlider:              // flow through
            case ETouchFeedbackEdit:                // flow through
            case ETouchFeedbackLineSelection:       // flow through
            case ETouchFeedbackBlankSelection:      // flow through
            case ETouchFeedbackTextSelection:       // flow through
            case ETouchFeedbackEmptyLineSelection:  // flow through
            case ETouchFeedbackTab:                 // flow through
            case ETouchFeedbackPopUp:               // flow through
            case ETouchFeedbackIncreasingPopUp:     // flow through
            case ETouchFeedbackDecreasingPopUp:     // flow through
            case ETouchFeedbackFlick:               // flow through
            case ETouchFeedbackCheckbox:            // flow through
            case ETouchFeedbackCharacterInputButton:
            case ETouchFeedbackOptionsMenuOpened:
            case ETouchFeedbackOptionsMenuClosed:
            case ETouchFeedbackSubMenuOpened:
            case ETouchFeedbackSubMenuClosed:
            case ETouchFeedbackLongTap:
            case ETouchFeedbackMultiTouchRecognized:
                effectIndex = 0;
                break;
            case ETouchFeedbackSensitive:           // flow through
            case ETouchFeedbackSensitiveButton:     // flow through
            case ETouchFeedbackSensitiveList:      
            case ETouchFeedbackSensitiveInput:
                effectIndex = 1;                    
                break;                              
            default:                                
                effectIndex = 0;                    
                break;                              
            }
        
        if ( effectIndex < iVibraParams.Count() && 
             iVibraParams[effectIndex].iTime > 0 &&
             iVibraParams[effectIndex].iIntensity != 0)
            {
            TRACE2("CTactileVibraPlayer::PlayFeedback %d", aFeedback);  
            TRAP( ret, DoPlayFeedbackL( 
                iVibraParams[effectIndex].iTime, 
                iVibraParams[effectIndex].iIntensity ) ); 

            if (!ret)    
                {
                TRACE2("CTactileVibraPlayer: Vibra playing leave code: %d", ret);  
                }
            }        
        }
    return ret;
    }
    
// ---------------------------------------------------------------------------
// Start feedback.
// ---------------------------------------------------------------------------
//   
TInt CTactileVibraPlayer::StartFeedback( TTouchContinuousFeedback aType,
                                         TInt aIntensity )
    {
    TRACE("CTactileVibraPlayer::StartFeedback - Begin");
    TRAPD( ret, DoStartFeedbackL( aType, aIntensity ) );
    TRACE("CTactileVibraPlayer::StartFeedback - End");
    return ret;
    }
        
// ---------------------------------------------------------------------------
// Modify feedback.
// ---------------------------------------------------------------------------
//                        
TInt CTactileVibraPlayer::ModifyFeedback( TInt /*aIntensity*/ )
    {
    return KErrNotSupported;
    }
    
// ---------------------------------------------------------------------------
// Stop feedback.
// ---------------------------------------------------------------------------
//    
void CTactileVibraPlayer::StopFeedback()
    {
    TRACE("CTactileVibraPlayer::StopFeedback - Begin");
    if ( iVibra )
        {
        TRAP_IGNORE( iVibra->StopVibraL() );
        }    
    TRACE("CTactileVibraPlayer::StopFeedback - End");
    }
  
// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
//    
TInt CTactileVibraPlayer::PlayPreviewFeedback( TInt aLevel, 
                                             TTouchLogicalFeedback aFeedback )    
    {
    TRACE("CTactileVibraPlayer::PlayPreviewFeedback - Begin");
    TInt ret( KErrArgument );
    
    if ( aLevel > EProfileTactileFeedbackOff  &&
         aLevel <= EProfileTactileFeedbackLevel3 )
        {
        iOriginalLevel = iVibraLevel;
        iVibraLevel = aLevel;
        ReadSettings();    
        ret = PlayFeedback( aFeedback );
        iVibraLevel = iOriginalLevel;
        ReadSettings();
        }    
    
    TRACE("CTactileVibraPlayer::PlayPreviewFeedback - End");
    return ret;    
    }

// ---------------------------------------------------------------------------
// Read vibra effects from central repository.
// ---------------------------------------------------------------------------
//
void CTactileVibraPlayer::ReadSettings()
    {    
    TRACE("CTactileVibraPlayer::ReadSettings - Begin");
    TTactileVibraParams basicParams;
    
    basicParams.iTime = 0;
    basicParams.iIntensity = 0;

    TTactileVibraParams sensitiveParams = basicParams;
        
    if ( iVibraLevel == EProfileTactileFeedbackLevel1 )
        {
        // Read settings for ETouchFeedbackBasic
        iRepository.Get( KTactileVibraIntensityBasicLevel1, 
                          basicParams.iIntensity );
        iRepository.Get( KTactileVibraDurationBasicLevel1,  
                          basicParams.iTime );
        
        // Read settings for ETouchFeedbackSensitive
        iRepository.Get( KTactileVibraIntensitySensitiveLevel1, 
                          sensitiveParams.iIntensity );
        iRepository.Get( KTactileVibraDurationSensitiveLevel1,  
                          sensitiveParams.iTime );
        }
    else if ( iVibraLevel == EProfileTactileFeedbackLevel2 )
        {
        // Read settings for ETouchFeedbackBasic
        iRepository.Get( KTactileVibraIntensityBasicLevel2, 
                          basicParams.iIntensity );
        iRepository.Get( KTactileVibraDurationBasicLevel2,  
                          basicParams.iTime );
        
        // Read settings for ETouchFeedbackSensitive
        iRepository.Get( KTactileVibraIntensitySensitiveLevel2, 
                          sensitiveParams.iIntensity );
        iRepository.Get( KTactileVibraDurationSensitiveLevel2,  
                          sensitiveParams.iTime );
        }
    else if ( iVibraLevel == EProfileTactileFeedbackLevel3 )
        {
        // Read settings for ETouchFeedbackBasic
        iRepository.Get( KTactileVibraIntensityBasicLevel3, 
                          basicParams.iIntensity );
        iRepository.Get( KTactileVibraDurationBasicLevel3,  
                          basicParams.iTime );
        
        // Read settings for ETouchFeedbackSensitive
        iRepository.Get( KTactileVibraIntensitySensitiveLevel3, 
                          sensitiveParams.iIntensity );
        iRepository.Get( KTactileVibraDurationSensitiveLevel3,  
                          sensitiveParams.iTime );
        }
    
    iVibraParams.Reset();

    iVibraParams.Append( basicParams );        
    iVibraParams.Append( sensitiveParams );         
    TRACE("CTactileVibraPlayer::ReadSettings - End");      
    }    
    
// ---------------------------------------------------------------------------
// Actually play feedback.
// ---------------------------------------------------------------------------
//
void CTactileVibraPlayer::DoPlayFeedbackL( TInt aTime, TInt aIntensity )
    {
    TRACE("CTactileVibraPlayer::DoPlayFeedbackL - Begin");
    if ( !iVibra )
        {
        // Construct iVibra at first feedback request.
        iVibra = CHWRMVibra::NewL();
        }
    
    TRACE("CTactileVibraPlayer::DoPlayFeedbackL - Starting Vibra");
    
    OstTrace0( TACTILE_PERFORMANCE, TACTILE_PLAY_VIBRA_FEEDBACK_1, "e_TACTILE_PLAY_VIBRA_FEEDBACK 1");
    
    iVibra->PulseVibraL( aTime, aIntensity ); 

    OstTrace0( TACTILE_PERFORMANCE, TACTILE_PLAY_VIBRA_FEEDBACK_0, "e_TACTILE_PLAY_VIBRA_FEEDBACK 0");
    TRACE("CTactileVibraPlayer::DoPlayFeedbackL - End");
    }

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
//    
void CTactileVibraPlayer::DoStartFeedbackL( TTouchContinuousFeedback /*aType*/,
                                            TInt /*aIntensity*/ )
    {
    if ( !iVibra )
        {
        // Construct iVibra at first feedback request.
        iVibra = CHWRMVibra::NewL();
        }    

    if ( iVibraParams.Count() )
        {
        TRACE2("CTactileVibraPlayer: StartVibraL  Duration: %d", KHWRMVibraMaxDuration ); 
        TRACE2("CTactileVibraPlayer: StartVibraL  Intensity: %d", iVibraParams[0].iIntensity );
        iVibra->StartVibraL( KHWRMVibraMaxDuration, 
                             iVibraParams[0].iIntensity );
        }    
    }                                            

// ---------------------------------------------------------------------------
// From MCenRepNotifyHandlerCallback.
// ---------------------------------------------------------------------------
//    
void CTactileVibraPlayer::HandleNotifyInt( TUint32 aId, TInt aNewValue )    
    {
    TRACE("CTactileVibraPlayer::HandleNotifyInt - Begin");
    if ( aId == KTactileFeedbackHapticsStrength )
        {
        iVibraLevel = StrengthToLevel( aNewValue );
        if ( iVibraLevel != EProfileTactileFeedbackOff )
            {
            ReadSettings();    
            }
        }
    TRACE("CTactileVibraPlayer::HandleNotifyInt - End");
    }

//---------------------------------------------------------------------------
// ImplementationTable[]
//
//---------------------------------------------------------------------------
//
const TImplementationProxy ImplementationTable[] = 
    {
    IMPLEMENTATION_PROXY_ENTRY( 0x2001CB9B, CTactileVibraPlayer::NewL )
    };

//---------------------------------------------------------------------------
// TImplementationProxy* ImplementationGroupProxy()
//
//---------------------------------------------------------------------------
//
EXPORT_C const TImplementationProxy* ImplementationGroupProxy( TInt& aTableCount )
    {
    aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
    return ImplementationTable;
    }
   
// End of file
