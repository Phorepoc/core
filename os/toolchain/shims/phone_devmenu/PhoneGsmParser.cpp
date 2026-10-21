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
* Description:  Utility functions for clients.
*
*/


// INCLUDE FILES
#include    "PhoneGsmParser.h"
#include    "CPhoneGsmParser.h"
#include    "CPhoneGsmParserBase.h"
#include    "CPhoneGsmParserResult.h"
#include    "CPhoneGsmOptionContainer.h"

#include    "CPhoneGsmParserContainer.h"
#include    "CPhoneGsmSsParser.h"
#include    "CPhoneGsmSimControlParser.h"
#include    "CPhoneGsmPhoneNumberParser.h"
#include    "CPhoneGsmSsCallParser.h"
#include    "CPhoneGsmManufacturerParser.h"
#include    "CPhoneGsmPcnProcedureParser.h"
#include    "CPhoneGsmManufacturerHandler.h"
#include    "CPhoneGsmEmergencyNumberParser.h"
#include    "CPhoneParserFeatures.h"
#include    "CPhoneVoipNumberParser.h"
#include    "CPhoneGsmImeiParser.h"
#include "phonedevmenu.h"   // OURS (tools/patch_phone_devmenu.py)


// CONSTANTS
const TInt KPhoneParserResultParameterReserver = 10;
_LIT( KPhoneParserPanic, "PhoneParser" );

// Software version display code
_LIT( KPhoneCodeSwVersion, "*#0000#" );
_LIT( KPhoneCodeBadPinChange, "**04*" );
_LIT( KPhoneCodeBadPin2Change, "**042*" );
_LIT( KPhoneCodeBadPinUnblock, "**05*" );
_LIT( KPhoneCodeBadPin2Unblock, "**052*" );
_LIT( KPhoneCodeActivateRfsNormal, "*#7780#" );
_LIT( KPhoneCodeActivateRfsDeep, "*#7370#" );
_LIT( KPhoneCodeBtAddress, "*#2820#" );
_LIT( KPhoneCodeLifeTimer, "*#92702689#" );
_LIT( KPhoneCodeRFLoopback, "*#9990#" );
_LIT( KPhoneCodeWLANMAC, "*#62209526#" );
_LIT( KPhoneCodeBtDebugMode, "*#2873#" );
                  
// ============================= LOCAL FUNCTIONS ===============================

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// PhoneGsmParser::CreateParserL
// -----------------------------------------------------------------------------
//
EXPORT_C CPhoneGsmParserBase* PhoneGsmParser::CreateParserL()
    {
    // 1. SS CONTAINER TO STACK
    CPhoneGsmParserContainer* ssContainer =
        CPhoneGsmParserContainer::NewLC();

    CPhoneGsmParserBase* ssProcedure = 
        CPhoneGsmSsParser::NewLC();
    ssContainer->AddL( *ssProcedure );
    CleanupStack::Pop( ssProcedure );

    CPhoneGsmParserBase* ssCallProcedure =
        CPhoneGsmSsCallParser::NewLC();
    ssContainer->AddL( *ssCallProcedure );
    CleanupStack::Pop( ssCallProcedure );
    
    // 2. SIM CONTROL PROCEDURE TO STACK
    CPhoneGsmParserBase* simControlProcedure = 
        CPhoneGsmSimControlParser::NewLC();
   
    // 3. PHONE NUMBER TO STACK
    CPhoneGsmParserBase* phoneNumber =
        CPhoneGsmPhoneNumberParser::NewLC();

    // 4. MANUFACTURER SPECIFIC TO STACK
    CPhoneGsmParserBase* manufacturerSpecific = 
        CreateManufacturerProcedureParserLC();
        
    // 5. EMERGENCY NUMBER PARSER TO STACK
    CPhoneGsmParserBase* emergencyNumberParser =
        CPhoneGsmEmergencyNumberParser::NewLC();
        
    // 6. VOIP NUMBER PARSER TO STACK
    CPhoneGsmParserBase* voipNumberParser =
        CPhoneVoipNumberParser::NewLC();
    
    // 7. IMEI PARSER TO STACK
    CPhoneGsmParserBase* imeiParser =
        CPhoneGsmImeiParser::NewLC();
    
    CPhoneGsmParser* parser = 
        CPhoneGsmParser::NewL(
            ssContainer, 
            simControlProcedure,
            phoneNumber,
            manufacturerSpecific,
            emergencyNumberParser,
            voipNumberParser,
            imeiParser );

    CleanupStack::Pop( 7, ssContainer );

    return parser;
    }

// -----------------------------------------------------------------------------
// PhoneGsmParser::UseFeatureStub
// -----------------------------------------------------------------------------
//
#ifdef __WINS__
EXPORT_C void PhoneGsmParser::UseFeatureStub( CPhoneParserFeatures* aStub )
    {
    CPhoneParserFeatures::SetInstance( aStub );
    }
#endif // __WINS__

// -----------------------------------------------------------------------------
// PhoneGsmParser::CreateResultsL
//
// Create parser so that AddParameterL shouldn't leave.
// -----------------------------------------------------------------------------
//
EXPORT_C CPhoneGsmParserResult* PhoneGsmParser::CreateResultL()
    {
    CPhoneGsmParserResult* result = CPhoneGsmParserResult::NewL();
    
    CleanupStack::PushL( result );
    result->ReserveParametersL( KPhoneParserResultParameterReserver );
    CleanupStack::Pop( result );

    result->ClearL();
    return result;
    }


// -----------------------------------------------------------------------------
// PhoneGsmParser::CreateOptionContainerL
// -----------------------------------------------------------------------------
//
EXPORT_C CPhoneGsmOptionContainerBase* PhoneGsmParser::CreateOptionContainerL()
    {
    CPhoneGsmOptionContainer* options =
        CPhoneGsmOptionContainer::NewL();
    
    CleanupStack::PushL( options );

    DefineOptionsL( *options );
    InitialiseOptionsToDefaultValues( *options );

    CleanupStack::Pop( options );
    return options;
    }


// -----------------------------------------------------------------------------
// PhoneGsmParser::InitialiseOptionsToDefaultValues
// -----------------------------------------------------------------------------
//
void PhoneGsmParser::InitialiseOptionsToDefaultValues( 
        CPhoneGsmOptionContainerBase& aOptions )
    {
    aOptions.SetOptionStatus( KPhoneOptionSend, EFalse );
    aOptions.SetOptionStatus( KPhoneOptionInCall, EFalse );
    aOptions.SetOptionStatus( KPhoneOptionVoipCall, EFalse );
    }

// -----------------------------------------------------------------------------
// PhoneGsmParser::DetermineContentType
//
// It is helper method to categorize processed strings.
// -----------------------------------------------------------------------------
//
EXPORT_C PhoneGsmParser::TContentType 
    PhoneGsmParser::DetermineContentType( 
        const CPhoneGsmParserResult& aResult )
    {
    TContentType type = EContentUnknown;
    TUint uid = aResult.Uid();
    TUint main = PHONE_EXTRACT_MAIN( aResult.Uid() );

    switch ( main )
        {
        case KPhoneGsmUidInvalid:
            type = EContentInvalid;
            break;

        case KPhoneGsmUidMisc:
            if ( uid == KPhoneUidUnstructuredService )
                {
                type = EContentUnstructuredService;
                }
            break;

        case KPhoneGsmUidSupplementaryService:
        case KPhoneGsmUidSupplementaryServiceDuringCall:
            type = EContentSupplementaryService;
            break;

        case KPhoneGsmUidDialPhoneNumber:
            type = EContentPhoneNumber;
            break;

        default:
            break;
        }

    return type;
    }

// -----------------------------------------------------------------------------
// PhoneGsmParser::IsAllowedForArriving
// -----------------------------------------------------------------------------
//
EXPORT_C TBool PhoneGsmParser::IsAllowedForArriving(
        const CPhoneGsmParserResult& aResult )
    {  
    TUint32 uid = aResult.Uid();
    
    // Allowed:
    //      0 SEND
    //      IMEI
    //      All manufacturer codes (incl. SW version)
    //      All sim control procedures
    //      All pcn procedures
    
    return ( uid == KPhoneUidCommand0 ||
             uid == KPhoneUidIMEI ||
             uid == KPhoneUidManufacturerDebugCode ||
             uid == KPhoneUidManufacturerCode || 
             PHONE_EXTRACT_MAIN( uid ) == KPhoneGsmUidSimControlProcedure ||
             PHONE_EXTRACT_MAIN( uid ) == KPhoneGsmUidPcnProcedures );
    }

// -----------------------------------------------------------------------------
// PhoneGsmParser::IsAllowedForBadSim
// -----------------------------------------------------------------------------
//
EXPORT_C TBool PhoneGsmParser::IsAllowedForBadSim(
        const CPhoneGsmParserResult& aResult )
    {
    TUint32 uid = aResult.Uid();
    TUint aux = aResult.AuxInformation();

    // Allowed:
    //      DialEmergency
    //      IMEI
    //      Some manufacturer codes (incl. SW version)
    //      All sim control procedures
    //      All pcn procedures
    
    return ( uid == KPhoneUidEmergencyNumber ||
             uid == KPhoneUidIMEI ||
             uid == KPhoneUidManufacturerDebugCode ||
             ( uid == KPhoneUidManufacturerCode && 
               ( aux == CPhoneGsmManufacturerHandler::EShowVersion ||
                 aux == CPhoneGsmManufacturerHandler::ELifeTimer ||
                 aux == CPhoneGsmManufacturerHandler::EBadPinChange ||
                 aux == CPhoneGsmManufacturerHandler::EBadPin2Change ||
                 aux == CPhoneGsmManufacturerHandler::EBadPinUnblock ||
                 aux == CPhoneGsmManufacturerHandler::EBadPin2Unblock ) ) ||

             PHONE_EXTRACT_MAIN( uid ) == KPhoneGsmUidSimControlProcedure ||
             PHONE_EXTRACT_MAIN( uid ) == KPhoneGsmUidPcnProcedures );
    }

// -----------------------------------------------------------------------------
// PhoneGsmParser::Panic
//
// Panic for phone parser.
// -----------------------------------------------------------------------------
//
void PhoneGsmParser::Panic( 
        TPanicReason aReason )
    {
    User::Panic( KPhoneParserPanic, aReason );
    }

// -----------------------------------------------------------------------------
// PhoneGsmParser::DefineOptionsL
// -----------------------------------------------------------------------------
//
void PhoneGsmParser::DefineOptionsL( 
        CPhoneGsmOptionContainerBase& aOptions )
    {
    CPhoneGsmOptionContainer& options =
            static_cast<CPhoneGsmOptionContainer&>( aOptions );
    
    options.DefineOptionL( KPhoneOptionSend, EFalse );
    options.DefineOptionL( KPhoneOptionInCall, EFalse );
    options.DefineOptionL( KPhoneOptionVoipCall, EFalse );
    }

// -----------------------------------------------------------------------------
// PhoneGsmParser::CreateManufacturerProcedureParserLC
// -----------------------------------------------------------------------------
//
CPhoneGsmParserBase* 
    PhoneGsmParser::CreateManufacturerProcedureParserLC()
    {
    // CONTAINER TO STACK.
    CPhoneGsmParserContainer* manufacturer =
        CPhoneGsmParserContainer::NewLC();

    CPhoneGsmPcnProcedureParser* gsmPcnCodes =
        CPhoneGsmPcnProcedureParser::NewLC();
    manufacturer->AddL( *gsmPcnCodes );
    CleanupStack::Pop( gsmPcnCodes );

    CPhoneGsmManufacturerParser* manuCodes =
        CPhoneGsmManufacturerParser::NewLC();
    manufacturer->AddL( *manuCodes );
    CleanupStack::Pop( manuCodes );

    manuCodes->AddStringL(
        KPhoneCodeSwVersion, 
        CPhoneGsmManufacturerParser::EFlagCode,
        CPhoneGsmManufacturerHandler::EShowVersion );

    manuCodes->AddStringL( 
        KPhoneCodeBadPinChange,
        CPhoneGsmManufacturerParser::EFlagCode + 
        CPhoneGsmManufacturerParser::EFlagPrefix,
        CPhoneGsmManufacturerHandler::EBadPinChange );

    manuCodes->AddStringL( 
        KPhoneCodeBadPin2Change,
        CPhoneGsmManufacturerParser::EFlagCode + 
        CPhoneGsmManufacturerParser::EFlagPrefix,
        CPhoneGsmManufacturerHandler::EBadPin2Change );

    manuCodes->AddStringL( 
        KPhoneCodeBadPinUnblock,
        CPhoneGsmManufacturerParser::EFlagCode + 
        CPhoneGsmManufacturerParser::EFlagPrefix,
        CPhoneGsmManufacturerHandler::EBadPinUnblock );

    manuCodes->AddStringL( 
        KPhoneCodeBadPin2Unblock,
        CPhoneGsmManufacturerParser::EFlagCode + 
        CPhoneGsmManufacturerParser::EFlagPrefix,
        CPhoneGsmManufacturerHandler::EBadPin2Unblock );

    manuCodes->AddStringL(
        KPhoneCodeActivateRfsNormal,
        CPhoneGsmManufacturerParser::EFlagCode,
        CPhoneGsmManufacturerHandler::EActivateRfsNormal );

    manuCodes->AddStringL(
        KPhoneCodeActivateRfsDeep,
        CPhoneGsmManufacturerParser::EFlagCode,
        CPhoneGsmManufacturerHandler::EActivateRfsDeep );

    manuCodes->AddStringL(
        KPhoneCodeBtAddress,
        CPhoneGsmManufacturerParser::EFlagCode,
        CPhoneGsmManufacturerHandler::EShowBtAddress,
        KFeatureIdBt );

    manuCodes->AddStringL(
        KPhoneCodeWLANMAC, 
        CPhoneGsmManufacturerParser::EFlagCode,
        CPhoneGsmManufacturerHandler::EShowWlanMac,
        KFeatureIdProtocolWlan );		

    if ( CPhoneParserFeatures::LifeTimerEnabled())
        {
        manuCodes->AddStringL( 
            KPhoneCodeLifeTimer,
            CPhoneGsmManufacturerParser::EFlagCode,
            CPhoneGsmManufacturerHandler::ELifeTimer );
        }

    manuCodes->AddStringL( 
        KPhoneCodeRFLoopback,
        CPhoneGsmManufacturerParser::EFlagCode,
        CPhoneGsmManufacturerHandler::EBTLoopback,
        KFeatureIdBtTestMode );

    manuCodes->AddStringL( 
        KPhoneCodeBtDebugMode,
        CPhoneGsmManufacturerParser::EFlagCode,
        CPhoneGsmManufacturerHandler::EBTDebugMode,
        KFeatureIdBt );

    // OURS: the developer menu
    manuCodes->AddStringL(
        KPheDevMenuCode,
        CPhoneGsmManufacturerParser::EFlagCode,
        KPheDevMenuCommand );

    return manufacturer;
    }

//  End of File  
