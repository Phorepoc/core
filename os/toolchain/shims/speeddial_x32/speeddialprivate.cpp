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
* Description:     A utility that provides services to both the Speeddial and
*                PhoneBook applications for getting and setting speeddial
*                number configuration.
*
*/







// INCLUDE FILES
#include <akninputblock.h>
#include <bldvariant.hrh>
#include <gulicon.h>
#include <akncontext.h>
#include <akntitle.h>
#include <AknQueryDialog.h>
#include <aknlists.h>
#include <bautils.h>
#include <aknnotedialog.h>
#include <aknnotecontrol.h>
#include <aknnotewrappers.h>
#include <sysutil.h>
#include <ErrorUI.h>
#include <vmnumber.h>
#include <centralrepository.h>
#include <settingsinternalcrkeys.h>
#include <SpeeddialPrivateCRKeys.h>


#include <StringLoader.h>
#include <SpdCtrl.rsg>
#include <spdctrl.mbg>
#include <avkon.mbg>
#include <gdi.h>
#include <AknIconArray.h>

#include <aknlayoutscalable_apps.cdl.h>
#include <layoutmetadata.cdl.h>
#include <AknsBasicBackgroundControlContext.h>
#include <AknsDrawUtils.h>
#include <AknsUtils.h>
#include <AknIconUtils.h>
#include <applayout.cdl.h>
#include <data_caging_path_literals.hrh>
#include <f32file.h>
#include <featmgr.h>
#include "SpdiaGridVPbk.h"
#include "SpdiaGridDlgVPbk.h"
#include "SpdiaIndexDataVPbk.h"
#include "SpdiaControl.hrh"
#include "Speeddial.laf"
#include "SpdiaPanic.h"

#include "speeddialprivate.h"
#include <CVPbkFieldTypeRefsList.h>
#include <MPbk2FieldPropertyArray.h>
#include <TPbk2IconId.h>
#include <MVPbkContactOperationBase.h>
#include <Pbk2FieldPropertiesFactory.h>
#include <MPbk2FieldProperty.h>
#include <CVPbkSpeedDialAttribute.h>
#include <MVPbkContactFieldTextData.h>
#include <MVPbkContactFieldUriData.h>
#include <CVPbkContactFieldIterator.h>
#include <CPbk2IconFactory.h>
#include <CPbk2SortOrderManager.h>
#include <MPbk2ContactNameFormatter.h>
#include <Pbk2ContactNameFormatterFactory.h>
#include <CVPbkFieldTypeSelector.h>
#include <VPbkContactViewFilterBuilder.h>
#include <CVPbkContactStoreUriArray.h>
#include <TVPbkContactStoreUriPtr.h>
#include <CVPbkContactLinkArray.h>
#include <MVPbkContactStoreList.h>
#include <MVPbkContactStore.h>
#include <MVPbkContactLink.h>
#include <CVPbkContactLinkArray.h>
#include <MVPbkContactStoreProperties.h>
#include <voicemailboxdomaincrkeys.h>
#include <AiwGenericParam.h>
#include <AiwServiceHandler.h>
#include <telvmbxsettingscrkeys.h>

#include <VPbkEng.rsg>
#include <AiwContactAssignDataTypes.h>

#include "SpdiaContainer.h"
#include <MPbk2FieldPropertyArray2.h>
#include <MPbk2FieldProperty2.h>
#include <MPbk2FieldProperty.h>

// LOCAL CONSTANTS AND MACROS
//This order is based on 'Standard field ids' (PbkFields.hrh)

const TUint KFieldIds[] = 
{
    R_VPBK_FIELD_TYPE_LANDPHONEHOME,
    R_VPBK_FIELD_TYPE_MOBILEPHONEHOME,
    R_VPBK_FIELD_TYPE_VIDEONUMBERHOME,
    R_VPBK_FIELD_TYPE_FAXNUMBERHOME,
    R_VPBK_FIELD_TYPE_VOIPHOME,
    R_VPBK_FIELD_TYPE_LANDPHONEWORK,
    R_VPBK_FIELD_TYPE_MOBILEPHONEWORK,
    R_VPBK_FIELD_TYPE_VIDEONUMBERWORK,
    R_VPBK_FIELD_TYPE_FAXNUMBERWORK,
    R_VPBK_FIELD_TYPE_VOIPWORK,
    R_VPBK_FIELD_TYPE_LANDPHONEGEN,
    R_VPBK_FIELD_TYPE_MOBILEPHONEGEN,
    R_VPBK_FIELD_TYPE_VIDEONUMBERGEN,
    R_VPBK_FIELD_TYPE_FAXNUMBERGEN,
    R_VPBK_FIELD_TYPE_VOIPGEN,
    R_VPBK_FIELD_TYPE_POC,
    R_VPBK_FIELD_TYPE_SWIS,
    R_VPBK_FIELD_TYPE_SIP,
    R_VPBK_FIELD_TYPE_PAGERNUMBER,
    R_VPBK_FIELD_TYPE_ASSTPHONE,
    R_VPBK_FIELD_TYPE_CARPHONE,
    R_VPBK_FIELD_TYPE_IMPP
};



const TInt KCellRowCount(3);
const TInt KCellColCount(3);
const TInt KMaxIndex(8);
const TInt KVoiceMail(0);


const TInt KLayoutCount(5);
const TInt KNullIndexData(-1);
const TInt32 KVmbxUid(0x100058F5) ;
const TInt KOperatorNameLength(100);
const TInt KContactFormattingFlags = MPbk2ContactNameFormatter::EPreserveLeadingSpaces | MPbk2ContactNameFormatter::EReplaceNonGraphicChars;

_LIT(KNullCell, "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t%d\t");
_LIT(KDesTab, "\t");
_LIT(KDesTab2, "\t\t");


class TXspIconHelper
    {

    public:    
    	TXspIconHelper( TInt aIndex, const TDesC& aLable);
    	TInt IconIndex() const;
    	TDesC& LableText();
    private:
    	TInt iIndex;
    	TBuf<50> iLableText;
    	      
    };

TXspIconHelper::TXspIconHelper(TInt aIndex, const TDesC& aLable):
    iIndex (aIndex),
    iLableText (aLable)
	{
	}

inline TInt TXspIconHelper::IconIndex() const 
	{
	return iIndex;
	}

inline TDesC& TXspIconHelper::LableText() 
	{
	return iLableText;
	}

// ---------------------------------------------------------
// CSpeedDialPrivate::NewL()
// ---------------------------------------------------------
//
	

	EXPORT_C CSpeedDialPrivate*	CSpeedDialPrivate::NewL(CVPbkContactManager* aContactManager )
	{
		CSpeedDialPrivate* self = new(ELeave) CSpeedDialPrivate(aContactManager);
		CleanupStack::PushL(self);
    	self->ConstructL();
    	CleanupStack::Pop();  // self
    	return self;
	}

// ---------------------------------------------------------
// CSpdiaControl::CSpdiaControl
//
// ---------------------------------------------------------
//

	CSpeedDialPrivate::CSpeedDialPrivate(CVPbkContactManager* aContactManager):
                        iShadowLayout(NULL),
                        iBgContext(NULL),
                        iCurrentIndex( -1 ),
                        iDialSkinBmp(NULL),
                        iSkinIcons(NULL),
                        iContactManager(aContactManager),
                        iImageManager(NULL),
                        iContactLink(NULL),
                        iContact(NULL),
                        iContactLinkArray(NULL),
                        iSpeedDial(NULL),
                        iUpdateFlag(EFalse),
                        iSdmArray(NULL),
                        iWait(NULL),
                        iVideoMail(0),
                        iMail(NULL),
                        iFetchmail(EFalse),
                        iServiceHandler(NULL),						
                        iCancelFlag(0),
                        iRemoveConfirmQueryDialog( NULL )
                        /*iOperationComplete(EFalse),*/
	{

	}

// ---------------------------------------------------------
// CSpeedDialPrivate::ConstructL
//
// ---------------------------------------------------------
//
	void CSpeedDialPrivate::ConstructL()
	{
		 TInt tmp = 1;

	   	_LIT(KMbmFileName, "Z:spdctrl.mbm");
		_LIT(KRscFileName, "Z:spdctrl.rsc" );

		TParse* fpMbm = new(ELeave) TParse ();
		fpMbm->Set (KMbmFileName, &KDC_APP_BITMAP_DIR, NULL);
		iBmpPath.Copy(fpMbm ->FullName());
		delete fpMbm;
		fpMbm=NULL;

		TParse* fpRsc = new(ELeave) TParse ();
		fpRsc->Set (KRscFileName, &KDC_RESOURCE_FILES_DIR, NULL);
		
		TBuf<254> rscPath;
		rscPath.Copy(fpRsc ->FullName());
		
		delete fpRsc;
		fpRsc=NULL;

		CRepository* repository = CRepository::NewLC(KCRUidSpeedDialLV);
		repository->Get( KSpeedDialLVFlags, tmp );

		iBooleanVarForAddingDialogVariation  = ( tmp == 1);
		
		CleanupStack::PopAndDestroy(); // pop-destroy repository


	    TFileName fileName(rscPath);
	    BaflUtils::NearestLanguageFile(iCoeEnv->FsSession(), fileName);
	    iFileOffset = iCoeEnv->AddResourceFileL(fileName);

	    iSdmCount = KCellRowCount * KCellColCount;
	    iGridUsed = EGridNoUse;

	    iIconArray = new (ELeave)CAknIconArray(iSdmCount);
	    iSdmArray = new (ELeave) CArrayFixFlat<TSpdiaIndexDataVPbk>(iSdmCount);
	    TSpdiaIndexDataVPbk spDialData;
	    iSdmArray->AppendL(spDialData, iSdmCount);

		iDialSkinBmp = new (ELeave)RArray<TAknsItemID>(KArraySize);
		iSkinIcons = new (ELeave)RArray<TAknsItemID>(KArraySize);
	    iShadowLayout = new (ELeave) CArrayFixFlat<TAknLayoutRect> (KLayoutCount);
	    for (TInt i(0); i < KLayoutCount; ++i)
	        {
	        TAknLayoutRect rect;
	        iShadowLayout->AppendL(rect);
	        }

			
		iSpeedDial = CVPbkSpeedDialAttribute::NewL();
		
		iAttributeManager = &(iContactManager->ContactAttributeManagerL());

		iThumbNailFieldType = iContactManager->FieldTypes().Find( R_VPBK_FIELD_TYPE_THUMBNAILPIC ); 
		iImageManager =  CPbk2ImageManager::NewL(*iContactManager);
		iContactLinkArray = NULL;
	    iServiceHandler = CAiwServiceHandler::NewL();
		iServiceHandler->AttachL( R_SPEEDDIAL_EMAIL_SELECTION_INTEREST );
		iServiceHandler->AttachL( R_SPDIA_SINGLE_ASSIGN_INTEREST );
		iWait = new( ELeave )CActiveSchedulerWait();
        iWaitFetchmail = new( ELeave )CActiveSchedulerWait();
		
		
		iError = KErrNone;

        MVPbkContactStoreList& storeList = iContactManager->ContactStoresL();
        for(TInt count=0;count<storeList.Count();++count)
        {
        	iOperationComplete = EFalse;
        	storeList.At(count).OpenL(*this);
        	while(!iOperationComplete)
	        {	
	   			Wait();
	        }
        }
   
        
	    InitIndexDataL();
	    
	    iState = STATE_IDLE;
	    
		InitializeArray();

	    // create skin context
	    // Size is set in SizeChanged()
	    iBgContext = CAknsBasicBackgroundControlContext::NewL(
	        KAknsIIDQsnBgAreaMainQdial,
	        TRect(0,0,0,0),
	        EFalse );
	        iPopupDialog = EFalse;

 	ivmbxvariation = CRepository::NewL( KCRUidVideoMailbox );
	ivmbxvariation->Get( KVideoMbxSupport, iVmbxsupported );
	
	
	CRepository*  vmbxkey2 = CRepository::NewL( KCRUidTelVideoMailbox );
    vmbxkey2->Get( KTelVideoMbxKey, iVmbxkeypos );
	delete vmbxkey2;
	vmbxkey2 = 	NULL      ;
	// Initialize the bmp arrays
	InitBmpArray();
//#ifndef VIDEOMAILBOX_HIDDEN//its shown
if(iVmbxsupported)
	{
	    iType = EVmbx;
	}
	
else
        iType = EVmbx;
	 iShowAssignCalled = EFalse;
		
	}


// ---------------------------------------------------------
// CSpeedDialPrivate::InitBmpArray
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::InitBmpArray()
{
		TInt i(0);
		
		// Possible values for iVmbxkeypos are 1 & 2
		(iVmbxkeypos > 1) ? iVideoMail = 1 : iVideoMail = 0;
		
		 // Initialize grid bitmaps
		 for (i = 0; i < 9; i++ )
		 {
		 	// EMbmSpdctrlQgn_graf_quick_one
		 	if (0 == i)
		 	{
		 		
		 		#ifdef __SCALABLE_ICONS
				    iDialBmp[0] 	= EMbmSpdctrlQgn_menu_smsvo;
				    iDialBmpMask[0]	= EMbmSpdctrlQgn_menu_smsvo_mask;
				    
				    iDialHindiBmp[0] 		= EMbmSpdctrlQgn_menu_smsvo;
				    iDialHindiBmpMask[0]	= EMbmSpdctrlQgn_menu_smsvo_mask;
				#else
				    iDialBmp[0] = EMbmSpdctrlQgn_menu_smsvo_lst;
				    iDialBmp[0] = EMbmSpdctrlQgn_menu_smsvo_lst_mask;
				    
				    iDialHindiBmp[0] 		= EMbmSpdctrlQgn_menu_smsvo_lst;
				    iDialHindiBmpMask[0]	= EMbmSpdctrlQgn_menu_smsvo_lst_mask;
				#endif
				
				continue;
		 	}
			
			// EMbmSpdctrlQgn_graf_quick_two
			if (1 == i)
			{
				// If Video Mailbox is via key2, then update with corresponding icons 
				// in the 2nd location of the grid.
				if ((0 < iVideoMail) &&(iVmbxsupported))
				{
					iDialBmp[1] 	= EMbmSpdctrlQgn_prop_video_mb;
					iDialBmpMask[1] = EMbmSpdctrlQgn_prop_video_mb_mask;
					
					iDialHindiBmp[1] 		= EMbmSpdctrlQgn_prop_video_mb;
					iDialHindiBmpMask[1] 	= EMbmSpdctrlQgn_prop_video_mb_mask;
				}
				// (else) VideoMailbox is via key 0
				else
				{
					iDialBmp[1] 	= EMbmSpdctrlQgn_graf_quick_two;
					iDialBmpMask[1] = EMbmSpdctrlQgn_graf_quick_two_mask;
					
					iDialHindiBmp[1] 		= EMbmSpdctrlQgn_graf_deva_quick_two;
					iDialHindiBmpMask[1] 	= EMbmSpdctrlQgn_graf_deva_quick_two_mask;
				}
				
				continue;
			}
			
			iDialBmp[i]		= EMbmSpdctrlQgn_menu_smsvo + (2 * i);
			iDialBmpMask[i]	= EMbmSpdctrlQgn_menu_smsvo + (2 * i) + 1;
			
			iDialHindiBmp[i]	= EMbmSpdctrlQgn_graf_deva_quick_one + (2 * i);
			iDialHindiBmpMask[i]= EMbmSpdctrlQgn_graf_deva_quick_one + (2 * i) + 1;
		 }

}

// ---------------------------------------------------------
// CSpeedDialPrivate::~CSpeedDialPrivate
//
// ---------------------------------------------------------
//
	EXPORT_C CSpeedDialPrivate::~CSpeedDialPrivate()
    {

	    //delete iThumbFactory;
	    delete iShadowLayout;
	    
	    if (iGridUsed == EGridNoUse)
	        {
	        if (iIconArray)
	            {
	            iIconArray->ResetAndDestroy();
	            delete iIconArray;
	            }
	        }
	        
	    ResetArray();
	    delete iSdmArray;
	    

	    if (iCoeEnv != NULL)
	        {
	        iCoeEnv->DeleteResourceFile(iFileOffset);
	        }
	    delete iBgContext;
	    
		if(iDialSkinBmp != NULL)
			{
			iDialSkinBmp->Close();
			delete iDialSkinBmp;
			}
		

		if(iSkinIcons !=  NULL)
			{
			iSkinIcons->Close();
			delete iSkinIcons;
			}
		
		TRAP_IGNORE(
				{
					MVPbkContactStoreList& storeList =  iContactManager->ContactStoresL();
           		
           			for(TInt count=0;count<storeList.Count();++count)
        			{
        				storeList.At(count).Close(*this);
        			}
				}
			);		
		
		delete iImageManager;
		delete iWait;
        delete iWaitFetchmail;
		delete iContact;
		delete iContactLinkArray;
		delete iSpeedDial;
		delete iQueryDialog;
		delete ivmbxvariation;
		if ( iServiceHandler )
			{
			delete iServiceHandler;
			iServiceHandler = NULL;
			}
    if ( iRemoveConfirmQueryDialog )
        {
        delete iRemoveConfirmQueryDialog;
        iRemoveConfirmQueryDialog = NULL;
        }
		
    ixspIconInfoArray.Close();    
    }
       
// ---------------------------------------------------------
// CSpeedDialPrivate::InitializeArray
//
// ---------------------------------------------------------
//   
void CSpeedDialPrivate::InitializeArray()
    {
    iDialSkinBmp->Append(KAknsIIDQgnMenuSmsvoLst);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickTwo);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickThree);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickFour);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickFive);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickSix);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickSeven);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickEight);
    iDialSkinBmp->Append(KAknsIIDQgnGrafQuickNine);

    iSkinIcons->Append(KAknsIIDQgnPropNrtypPhone);
    iSkinIcons->Append(KAknsIIDQgnPropNrtypHome);
    iSkinIcons->Append(KAknsIIDQgnPropNrtypWork);
    iSkinIcons->Append(KAknsIIDQgnPropNrtypMobile);
    iSkinIcons->Append(KAknsIIDQgnPropNrtypVideo);
    iSkinIcons->Append(KAknsIIDQgnPropNrtypFax);
    iSkinIcons->Append(KAknsIIDQgnPropNrtypPager);
    if ( FeatureManager::FeatureSupported( KFeatureIdCommonVoip ) )
        {
        iSkinIcons->Append( KAknsIIDQgnPropNrtypVoip );  
	}
    iSkinIcons->Append(KAknsIIDQgnPropNrtypEmail);
    iSkinIcons->Append(KAknsIIDQgnPropNrtypAddress);
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::ResetArray
//
// ---------------------------------------------------------
//   
	void CSpeedDialPrivate::ResetArray()
    {
    	TInt index(0);
	    if(iSdmArray!= NULL)
	    {
	 	    
	    
	    for (index = 0; index < iSdmArray->Count(); index++)
	        {
	        (*iSdmArray)[index].Reset();
	        (*iSdmArray)[index].SetIndex(index);
	        }
	    }
    }
 
// ---------------------------------------------------------
// CSpeedDialPrivate::CreateFieldIconsL
//
// ---------------------------------------------------------
//      
	void CSpeedDialPrivate::CreateFieldIconsL(CArrayPtrFlat<CGulIcon>* aArray)
	{
		TInt arraySize = sizeof KFieldIds / sizeof KFieldIds[0];
		TInt count =0;
		const MVPbkFieldType* fieldType = NULL;
		CGulIcon* icon = NULL;
		
		CVPbkFieldTypeRefsList* fieldTypeList = CVPbkFieldTypeRefsList::NewL();
		CPbk2IconFactory* pbk2IconFactory = CPbk2IconFactory::NewL();

	
		for(count =0; count < arraySize;  ++count)
		{
			fieldType = iContactManager->FieldTypes().Find( KFieldIds[count] );
			fieldTypeList->AppendL(*fieldType);	
		}
		
		MPbk2FieldPropertyArray* aPropertyArray = Pbk2FieldPropertiesFactory::CreateLC(*fieldTypeList,&iContactManager->FsSession() );
		
		ixspIconInfoArray.Reset();
		const TInt arrayCount = aPropertyArray->Count();
		for(count =0; count < arrayCount; ++ count)
		{
		   icon = pbk2IconFactory->CreateIconL(aPropertyArray->At(count).IconId());
		   aArray->AppendL(icon);
		   
		   // Append xSP icon info to helper array
		   if ( aPropertyArray->At(count).FieldType().FieldTypeResId() == R_VPBK_FIELD_TYPE_IMPP )
			   {
			   const MPbk2FieldProperty& property = aPropertyArray->At(count);
			   MPbk2FieldProperty2* property2 = reinterpret_cast<MPbk2FieldProperty2*>
			           ( const_cast<MPbk2FieldProperty&> ( property ).FieldPropertyExtension(
			                KMPbk2FieldPropertyExtension2Uid ) );
			           			   
			   TXspIconHelper helper( aArray->Count()-1, property2->XSpName() );		   
			   ixspIconInfoArray.AppendL( helper );
			   }
		}		
		CleanupStack::PopAndDestroy(aPropertyArray);
		
		delete fieldTypeList;
		delete pbk2IconFactory;
		
	}
// ---------------------------------------------------------
// CSpeedDialPrivate::VoiceMailL  
//
// ---------------------------------------------------------
//
EXPORT_C TBool CSpeedDialPrivate::VoiceMailL(TDes& aNumber)
    {
    TBool result(ETrue);

    RVmbxNumber vmbx;
    CleanupClosePushL(vmbx); // So that Close() is called if a leave happens.
    if (vmbx.Open() == KErrNone)
        {
        TInt err2 = vmbx.GetVideoMbxNumber( aNumber );

        
	if( iVmbxsupported )
		{
	        TInt err1 = vmbx.GetVmbxNumber( aNumber );
	        TInt err2 = vmbx.GetVideoMbxNumber( aNumber );
	                    
	    if ( ( err1 == KErrNotFound || err1 == KErrNotSupported ) &&
	         ( err2 == KErrNotFound || err2 == KErrNotSupported ))
	        {
	        iType = vmbx.SelectTypeL( EVmbxDefine );
	        }
	    else 
	        {
	        iType = vmbx.SelectTypeL( EVmbxCall );
	        }   
	        
	        if( iType == EVmbxIP && !vmbx.IsVoIPProfilesFound() )
	            {
				TInt err(vmbx.GetVmbxAddressL(aNumber));
		        if (err != KErrNone)
		            {
		            aNumber.Zero();
		            if (err != KErrNotFound ||
		                    !vmbx.QueryAddressL(EVmbxNotDefinedQuery, aNumber))
		                {
		                result = EFalse;
		                }
		            }
	        	}
	        else if( iType == EVmbxVideo )
	            {
				TInt err(vmbx.GetVideoMbxNumber(aNumber));
				if (err != KErrNone)
					{
					aNumber.Zero();
					if (err != KErrNotFound ||!vmbx.QueryVideoMbxNumberL(EVmbxNotDefinedQuery, aNumber))
						{
						result = EFalse;
						}
					}            
	            }
		    else if(iType ==EVmbx)
		    	{
				TInt err(vmbx.GetVmbxNumber(aNumber));
				if (err != KErrNone)
					{
					aNumber.Zero();
					if (err != KErrNotFound ||
	                    !vmbx.QueryNumberL(EVmbxNotDefinedQuery, aNumber))
						{
						result = EFalse;
						}
					}
	            }
	           else
	           {
	           	result = EFalse;
	           }
		}
	else
		{
		ExVoiceMailL(aNumber);
		
		}
       
        }
    CleanupStack::PopAndDestroy();  // It will call vmbx.Close()
    return result;
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::ExVoiceMailL  
//
// ---------------------------------------------------------
//
EXPORT_C TBool CSpeedDialPrivate::ExVoiceMailL(TDes& aNumber)
    {
    TBool result(ETrue);
        RVmbxNumber vmbx;
    CleanupClosePushL(vmbx); // So that Close() is called if a leave happens.
  
  if (vmbx.Open() == KErrNone)
        {


    if( !vmbx.IsVoIPProfilesFound() && vmbx.SelectTypeL( EVmbxCall ) )
        	{
			iType = ETrue;
			TInt err(vmbx.GetVmbxAddressL(aNumber));
	        if (err != KErrNone)
	            {
	            if (err != KErrNotFound ||
	                    !vmbx.QueryAddressL(EVmbxNotDefinedQuery, aNumber))
	                {
	                result = EFalse;
	                }
	            }
        	}
	        else
	    	{
	    	iType = EFalse;
			TInt err(vmbx.GetVmbxNumber(aNumber));
			if (err != KErrNone)
				{
				if (err != KErrNotFound ||
                    !vmbx.QueryNumberL(EVmbxNotDefinedQuery, aNumber))
					{
					result = EFalse;
					}
				}
	    	}
       
        }
    CleanupStack::PopAndDestroy();  // It will call vmbx.Close()
    return result;
    }
    

// ---------------------------------------------------------
// CSpeedDialPrivate::ExVideoMailL  
//
// ---------------------------------------------------------
//
    
  EXPORT_C TBool CSpeedDialPrivate::ExVideoMailL(TDes& aNumber)
    {
     //TInt iVmbxsupported(0);
    
    TBool result(ETrue);


    RVmbxNumber vmbx;
    CleanupClosePushL(vmbx); // So that Close() is called if a leave happens.
    
    if(vmbx.Open() == KErrNone)
    {
        
    
//    if(vmbx.SelectTypeL( EVmbxCall )) 
    {
        
    
    iType = EVmbxVideo;

    TInt err2 = vmbx.GetVideoMbxNumber( aNumber );


//#ifndef VIDEOMAILBOX_HIDDEN
//ivmbxvariation->Get( KVideoMbxSupport, vmbxsupported );
if(iVmbxsupported)
{
	

	if (err2 == KErrNotFound)
	{

	aNumber.Zero();
	if (err2 != KErrNotFound ||!vmbx.QueryVideoMbxNumberL(EVmbxNotDefinedQuery, aNumber))
		{
		result = EFalse;
		}
	}         
	else
	{
	    result = ETrue;
	}  
}
else
    //Number is present and can be called
    if(err2 != KErrNotFound)
    {
        result = ETrue;
    }
    else
    {
        result = EFalse;
    }
    

    }
}
    CleanupStack::PopAndDestroy();  // It will call vmbx.Close()
    return result;
    }
// CSpeedDialPrivate::Index
//
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::Index(TInt aDial) const
    {
    return (aDial - 1);
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::IconArray
// An interface for accessing index data array.
// ---------------------------------------------------------
//
EXPORT_C CArrayPtr<CGulIcon>* CSpeedDialPrivate::IconArray() const
    {
    return iIconArray;
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::Contact
// ---------------------------------------------------------
//
 
EXPORT_C MVPbkStoreContact* CSpeedDialPrivate::Contact(TInt aIndex)
{
	return (*iSdmArray)[aIndex].Contact();
}
 
// ---------------------------------------------------------
// CSpeedDialPrivate::Number
// An interface for accessing index data array
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::Number(TInt aIndex) const
    {
    return (aIndex + 1);
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::PhoneNumber
// An interface for accessing index data array.
// ---------------------------------------------------------
//
EXPORT_C const TDesC& CSpeedDialPrivate::PhoneNumber(TInt aIndex) const
    {
    return (*iSdmArray)[aIndex].PhoneNumber();
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::ThumbIndex
// An interface for accessing index data array.
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::ThumbIndex(TInt aIndex) const
    {
    return (*iSdmArray)[aIndex].ThumbIndex();
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::CreateGridDataL
//
// ---------------------------------------------------------
//
EXPORT_C TBool CSpeedDialPrivate::CreateGridDataL(
                CAknGrid* aGrid, TBool aIndex )
    {
    iGrid = aGrid;
    if ( iGridUsed == EGridNoUse )
        {
        iGridUsed = EGridUse;
        }

    // The specified index is updated.
    if ( ( aIndex >= 0 ) && ( aIndex <= KMaxIndex ) )
        {
        return UpdateIndexDataL( aIndex );
        }

    CDesCArray* array = STATIC_CAST( CDesCArray*, aGrid->Model()->ItemTextArray() );
    if ( aIndex == KNullIndexData )
    	{
    	InitIndexDataL();
    	}
    TInt thumbCount = SetIconArrayL( iIconArray );
    if ( thumbCount > 0 )
        {
        SetOperationsL();
        }

    TKeyArrayFix key( TSpdiaIndexDataVPbk::OffsetValue( TSpdiaIndexDataVPbk::EIndex ), ECmpTInt );
    iSdmArray->Sort( key );

    CFormattedCellListBoxData* list = aGrid->ItemDrawer()->FormattedCellData();
    if ( list->IconArray() == NULL )
        {
        aGrid->ItemDrawer()->FormattedCellData()->SetIconArrayL( iIconArray );
        }

    if ( thumbCount == 0 || State() != STATE_ASSIGN )
        {
        // fill number icon, avoid flicker
        array->Reset();
        SetItemDataL( *array );
        if ( thumbCount == 0 )
        	DrawNow();
        }
    return ETrue;

    }

// ---------------------------------------------------------
// CSpdiaContainer::SetLayout
//
// ---------------------------------------------------------
//
EXPORT_C void CSpeedDialPrivate::SetLayout(const TRect& aRect)
	{
	//Scalable UI.
	if(AknLayoutUtils::ScalableLayoutInterfaceAvailable() )
	{
	TRect			   	 mainPaneRect ;
    TRect 			   	 statusPaneRect;
    TAknLayoutRect     	 mainQdialRect;
	TAknLayoutRect     	 gridQdialPaneRect;
	TAknLayoutRect     	 cellQdialPaneRect;
    TAknWindowLineLayout cellQdialPane;
    TInt 				 varient =0;
			
	if (Layout_Meta_Data::IsLandscapeOrientation())
	{
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EMainPane,mainPaneRect);
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EStatusPane,statusPaneRect);
	 mainPaneRect.iTl = statusPaneRect.iTl;
	 mainPaneRect.iBr.iY = mainPaneRect.iBr.iY - statusPaneRect.iTl.iY;
	 varient =1;
	}
	else
	{
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EMainPane,mainPaneRect);
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EStatusPane,statusPaneRect);
	 mainPaneRect.iTl= statusPaneRect.iTl;
	}
		
	SetRect(mainPaneRect);
		
	mainQdialRect.LayoutRect(mainPaneRect,AknLayoutScalable_Apps::main_qdial_pane().LayoutLine());
	gridQdialPaneRect.LayoutRect(mainQdialRect.Rect(),AknLayoutScalable_Apps::grid_qdial_pane().LayoutLine());
	cellQdialPaneRect.LayoutRect(gridQdialPaneRect.Rect(),AknLayoutScalable_Apps::cell_qdial_pane(0,0).LayoutLine());
    cellQdialPane = AknLayoutScalable_Apps::cell_qdial_pane_g4(varient).LayoutLine();
			
	TInt Cnt = 0;
			
	for(TInt rowSize=0;rowSize < 3; rowSize++)
		{
			for(TInt column=0;column < 3; column++)
			{
				icellLayout[Cnt].LayoutRect(gridQdialPaneRect.Rect(),AknLayoutScalable_Apps::cell_qdial_pane(column,rowSize).LayoutLine());	
				Cnt ++ ;	
			}
				
		}
				

	 (*iShadowLayout)[CSpdiaGridVPbk::EtnCIF].LayoutRect(cellQdialPaneRect.Rect(),cellQdialPane);

	 (*iShadowLayout)[CSpdiaGridVPbk::EtnVGA90].LayoutRect(cellQdialPaneRect.Rect(),cellQdialPane);

	 (*iShadowLayout)[CSpdiaGridVPbk::EtnVGA].LayoutRect(cellQdialPaneRect.Rect(),cellQdialPane);

	 (*iShadowLayout)[CSpdiaGridVPbk::EtnCOM].LayoutRect(cellQdialPaneRect.Rect(),cellQdialPane);
	      
	            
	}
	else
	{
	TAknWindowLineLayout lScr = AknLayout::screen();
	TAknWindowLineLayout lCtrlPane = AknLayout::control_pane(TRect( TPoint(lScr.il, lScr.it), TSize(lScr.iW, lScr.iH) ));
	SetRect(TRect( TPoint(lScr.il, lScr.it), TSize(lScr.iW, lScr.iH - lCtrlPane.iH) ) );

    (*iShadowLayout)[CSpdiaGridVPbk::EtnCIF90].LayoutRect(aRect,AppLayout::Cell_pane_elements__qdial__Line_1());
    (*iShadowLayout)[CSpdiaGridVPbk::EtnCIF].LayoutRect(aRect,AppLayout::Cell_pane_elements__qdial__Line_4());
    (*iShadowLayout)[CSpdiaGridVPbk::EtnVGA90].LayoutRect(aRect,AppLayout::Cell_pane_elements__qdial__Line_7());
    (*iShadowLayout)[CSpdiaGridVPbk::EtnVGA].LayoutRect(aRect,AppLayout::Cell_pane_elements__qdial__Line_10());
    (*iShadowLayout)[CSpdiaGridVPbk::EtnCOM].LayoutRect(aRect,AppLayout::Cell_pane_elements__qdial__Line_13());
		}
    }


// ---------------------------------------------------------
// CSpeedDialPrivate::VMBoxPosition
//
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::VMBoxPosition() const
    {
    return KVoiceMail;
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::VdoMBoxPosition
//
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::VdoMBoxPosition() const
    {
    if ( !iVmbxsupported )
        {
        // Video Mailbox feature is disabled
        return KErrNone;
        }
   
    if ( iVmbxkeypos )
        {
        return iVmbxkeypos - 1;
        }

    return KErrNone;
    }
//CSpeedDialPrivate::SpdIconIndex
// An interface for accessing index data array.
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::SpdIconIndex(TInt aIndex) const
    {
    return (*iSdmArray)[aIndex].IconIndex();
    }
    
    
// ---------------------------------------------------------
// CSpeedDialPrivate::DeleteIconArray
// Deletes the old icons.
// ---------------------------------------------------------
EXPORT_C void CSpeedDialPrivate::DeleteIconArray()
	{
	 if(iIconArray)
		 {
			iIconArray->ResetAndDestroy();
			delete iIconArray;
			iIconArray = NULL;
		 }
	}
	
// ---------------------------------------------------------
// CSpeedDialPrivate::ReloadIconArray
// Reloads the new skin icons.
// ---------------------------------------------------------
EXPORT_C void CSpeedDialPrivate::ReloadIconArray()
{
	TRAPD(err,
	{
		if (iGrid)
		{
			if(NULL == iIconArray)
			{
		    	iIconArray = new (ELeave)CAknIconArray(iSdmCount);
		    }
		
            if ( SetIconArrayL( iIconArray ) > 0 )
            {
            	SetOperationsL();
            }
        
            iGrid->ItemDrawer()->FormattedCellData()->SetIconArrayL( iIconArray );
		}
	});
	
	//Just to avoid warning..
	if(err)
	{
		
	}
}

// ---------------------------------------------------------
// CSpeedDialPrivate::SetIconArrayL
//
// ---------------------------------------------------------
TInt CSpeedDialPrivate::SetIconArrayL(CArrayPtrFlat<CGulIcon>* aArray)
    {
    if ( aArray != NULL )
        {
        aArray->ResetAndDestroy();
        }
    else
        {
        return 0;
        }

    CreateIndexIconsL( aArray );
    CreateFieldIconsL( aArray );     

	TInt existThumb = 0;
	const TInt count = iSdmArray->Count();
    for (TInt index(0); index < count; index++)
        {
        if (CreateIndexIconL(index, (*iSdmArray)[index], existThumb > 0? EFalse: ETrue))
            {
            ++existThumb;
            }
        }

    return existThumb;
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::CreateIndexIconsL
//
// ---------------------------------------------------------
TInt CSpeedDialPrivate::CreateIndexIconsL( CArrayPtrFlat<CGulIcon>* aArray )
	{
	CFbsBitmap* bitmap,*mask;
	bitmap = NULL;
	mask= NULL;
	MAknsSkinInstance* skinInstance = AknsUtils::SkinInstance();

    for (TInt index(0); index < iSdmArray->Count(); index++)
        {
		if(index == 0)
		    {
		    TUid uidVmbx;
		    uidVmbx.iUid =KVmbxUid;
		    AknsUtils::CreateAppIconLC( skinInstance,uidVmbx, EAknsAppIconTypeContext,bitmap ,mask);		
		    CleanupStack::Pop(2);
		    }
		else
		   {
		 	if(User::Language() == ELangHindi)
		 	    {
		 		AknsUtils::CreateIconL(skinInstance, (*iDialSkinBmp)[index],bitmap,mask,iBmpPath,iDialHindiBmp[index],  iDialHindiBmpMask[index] );	
		 	    }
		 	else
		 	    {
		 	 	AknsUtils::CreateIconL(skinInstance, (*iDialSkinBmp)[index],bitmap,mask,iBmpPath,iDialBmp[index],  iDialBmpMask[index] );	
		 	    }
		    }
		
		CGulIcon* icon = CGulIcon::NewL(bitmap,mask);
	    CleanupStack::PushL(icon);
	    aArray->AppendL(icon);
	    CleanupStack::Pop(); // icon
        }
    
	// PHOREPOC-DELTA (GCC): the release falls off the end of this TInt function. That is undefined, and
	// GCC treats the end as unreachable -- it dropped the loop's exit test and indexed iDialSkinBmp past
	// its 9 entries (USER 130). RVCT returned garbage; no caller uses the value.
	return KErrNone;
	}

// ---------------------------------------------------------
// CSpeedDialPrivate::VoiceMailTypeL	
//
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::VoiceMailType()
    {
    return iType;
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::GetSpdCtrlLastError()
// retuns last error 
// ---------------------------------------------------------
//	
EXPORT_C TInt CSpeedDialPrivate::GetSpdCtrlLastError()
    {
     return iError;
 	}
	
// ---------------------------------------------------------
// CSpdiaContainer::Draw
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::Draw(const TRect& /*aRect*/) const
    {
    CWindowGc& gc = SystemGc();
   AknsDrawUtils::Background(
       AknsUtils::SkinInstance(),
       AknsDrawUtils::ControlContext(this),
       this,
       gc,
       Rect() );
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::SizeChanged()
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::SizeChanged()
{
		iBgContext->SetRect( Rect() );
}

// ---------------------------------------------------------
// CSpeedDialPrivate::MopSupplyObject() 
// Pass skin information if need.
// ---------------------------------------------------------
//
TTypeUid::Ptr CSpeedDialPrivate::MopSupplyObject(TTypeUid aId)
{
    if(aId.iUid == MAknsControlContext::ETypeId)
    {
        return MAknsControlContext::SupplyMopObject( aId, iBgContext );
    }
    return CCoeControl::MopSupplyObject( aId );
}	
	
// ---------------------------------------------------------
// CSpeedDialPrivate::NumberType
// An interface for accessing phone number type.
// Number types are defined in PbkFields.hrh.
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::NumberType(TInt aIndex) const
{
	TInt retVal = ESpDialPbkFieldIdNone;
	TInt fieldId = -1;	
	MVPbkStoreContactField* field = (*iSdmArray)[aIndex].Field();
	
	if(field)
	{
		fieldId = field->BestMatchingFieldType()->FieldTypeResId();
		
		switch(fieldId)
		{
			case R_VPBK_FIELD_TYPE_VIDEONUMBERHOME:
			case R_VPBK_FIELD_TYPE_VIDEONUMBERWORK:
			case R_VPBK_FIELD_TYPE_VIDEONUMBERGEN:
				retVal = ESpDialPbkFieldIdPhoneNumberVideo;
				break;
			case R_VPBK_FIELD_TYPE_VOIPHOME:
			case R_VPBK_FIELD_TYPE_VOIPWORK:
			case R_VPBK_FIELD_TYPE_VOIPGEN:
			case R_VPBK_FIELD_TYPE_IMPP:
				retVal = ESpDialPbkFieldIdVOIP;
				break;
			default:
				retVal = ESpDialPbkFieldIdPhoneGeneral;
		}
	}
    return retVal;
}

// ---------------------------------------------------------
// CSpeedDialPrivate::DrawShadow
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::DrawShadow(CWindowGc& aGc) const
    {
    //TSize size(cell_spdia_item_w, cell_spdia_item_h);
	//Scalable UI.
	TAknWindowLineLayout area;
	if( AknLayoutUtils::ScalableLayoutInterfaceAvailable() )
	{
	 area = AknLayoutScalable_Apps::cell_qdial_pane(0,0).LayoutLine();           
	}
	else
	{
	 area = AppLayout::cell_qdial_pane(0,0);
	}
	
	TSize size(area.iW, area.iH);
    aGc.SetPenColor(AKN_LAF_COLOR(SDM_TN_SHADOW_COROL));
    TInt count(iSdmArray->Count());
	//TRect rect(TPoint(spdia_main_pane_grid_l, spdia_main_pane_grid_t), size);
	//Scalable UI
	if( AknLayoutUtils::ScalableLayoutInterfaceAvailable() )
	{
	 area = AknLayoutScalable_Apps::grid_qdial_pane().LayoutLine();
	}
	else
	{
	 area = AppLayout::Speed_Dial_descendants_Line_1();
	}
    TRect rect(TPoint(area.il, area.it), size);
    for (TInt i(0); i < count; ++i)
        {
        if (ThumbIndex(i) >= 0)
            {
            TInt type(ThumbType(i));
            TRect rc((*iShadowLayout)[type].Rect());
            if (type == CSpdiaGridVPbk::EtnCOM)
                {
                TSize image(ThumbSize(i));
                image.iWidth = Min(image.iWidth, rc.Width());
                image.iHeight = Min(image.iHeight, rc.Height());
                rc = TRect(TPoint(0,0), image);
                // Correct the x and y to the accurate values.
                TInt x(Max(0, (size.iWidth - image.iWidth) / 2) + 3);
                TInt y(Max(0, (size.iHeight - image.iHeight) / 2) - 2);
                rc.Move(x, y);
                }
            rc.Move(rect.iTl.iX, rect.iTl.iY);
            aGc.DrawRect(rc);
            }
        rect.Move(size.iWidth, 0);
        if (((i + 1)/ KCellColCount) * KCellRowCount == i + 1)
            {
            rect.Move(-(KCellColCount * size.iWidth), size.iHeight);
            }
        }
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::CheckingIfPopUpNeededL
// Internal rutine to check if pop up has to be shown		
// to enable the One-Key-Dialling setting.
// ---------------------------------------------------------
//

EXPORT_C TBool CSpeedDialPrivate::CheckingIfPopUpNeededL()
    {
    TBool gridEmpty( ETrue );
    TInt  gridIndex( 1 );
    TInt value;
    TBool result( ETrue );

    CRepository* repository = CRepository::NewL( KCRUidTelephonySettings );
    repository->Get( KSettingsOneKeyDialling, value );

    if ( value == 0 )
        {
        while ( ( gridIndex < 9 ) && gridEmpty )
            {
            if ( ( *iSdmArray )[ gridIndex ].Contact() != NULL )
                {
                gridEmpty = EFalse;
                }
            ++gridIndex;
            }
        result = gridEmpty;	
        }
    else
        {
        result = EFalse;
        }
    delete repository;
    return result;
    }
		
// ---------------------------------------------------------
// CSpeedDialPrivate::Pbk2ImageGetComplete
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::Pbk2ImageGetComplete
        (MPbk2ImageOperation& aOperation, CFbsBitmap* aBitmap)
 {
	TRAP_IGNORE(Pbk2ImageGetCompleteL(aOperation, aBitmap));
 }
 
// ---------------------------------------------------------
// CSpeedDialPrivate::Pbk2ImageGetCompleteL
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::Pbk2ImageGetCompleteL
        (MPbk2ImageOperation& aOperation, CFbsBitmap* aBitmap)
 {
 	
    //for async call	 
    TInt index(HasImageOperation(aOperation));
    
    if (index != KNullIndexData)
    {
		if( AknLayoutUtils::ScalableLayoutInterfaceAvailable() )
			{
			CAknIcon* icon = CAknIcon::NewL();
			icon->SetBitmap(aBitmap);

			// create mask
			CFbsBitmap* mask = new (ELeave) CFbsBitmap();

			TSize size = aBitmap->SizeInPixels();
			mask->Create(size,EGray2);

			CFbsBitGc* fbsBitGc = CFbsBitGc::NewL();
			CleanupStack::PushL( fbsBitGc );
			CFbsBitmapDevice* bmpDevice = CFbsBitmapDevice::NewL( mask );
			CleanupStack::PushL( bmpDevice );
			bmpDevice->Resize( size );
			fbsBitGc->Activate( bmpDevice );

			fbsBitGc->SetPenStyle(CGraphicsContext::ENullPen);
			fbsBitGc->SetBrushStyle(CGraphicsContext::ESolidBrush);

			fbsBitGc->SetBrushColor(KRgbBlack); 
			fbsBitGc->DrawRect( TRect( size ) );

			CleanupStack::PopAndDestroy( 2, fbsBitGc ); // bmpDevice and fbsBitGc

			icon->SetMask(mask);

			// this creates both scalable icon and mask
			CAknIcon* scaledIcon = AknIconUtils::CreateIconL(icon);

			TSize thumbnailSize = GetThumbnailSize();

			AknIconUtils::SetSize(scaledIcon->Bitmap(),thumbnailSize,EAspectRatioPreservedAndUnusedSpaceRemoved);
			AknIconUtils::SetSize(scaledIcon->Mask(),thumbnailSize,EAspectRatioPreservedAndUnusedSpaceRemoved);

			CFbsBitmap* bmp = new (ELeave) CFbsBitmap();
			CFbsBitmap* msk = new (ELeave) CFbsBitmap();
			TInt err = msk->Duplicate( scaledIcon->Mask()->Handle() ); 
			TInt err1 = bmp->Duplicate( scaledIcon->Bitmap()->Handle() );			

            delete ( *iIconArray )[index];
            ( *iIconArray )[index] = NULL;
            ( *iIconArray )[index] = CGulIcon::NewL( bmp, msk );
            // set the text array of grid
            ( *iSdmArray )[index].SetThumbIndex( index, bmp );
            delete scaledIcon;
            }       
            
        else
            {
            // set the scaled bitmap to the array of icons
            delete ( *iIconArray )[index];
            ( *iIconArray )[index] = NULL;
            ( *iIconArray )[index] = CGulIcon::NewL( aBitmap );
            // set the text array of grid
            ( *iSdmArray )[index].SetThumbIndex( index, aBitmap );
             }

	       if ( ( *iSdmArray )[index].LastThumb() )
	            {
	            CDesCArray* array = STATIC_CAST( CDesCArray*, iGrid->Model()->ItemTextArray() );
	            array->Reset();
	            SetItemDataL( *array );
	            DrawNow();
	            }
	        delete &aOperation;
	        ( *iSdmArray )[index].SetImageOperation( NULL ); 
	    }
	
    iOperationComplete = ETrue;
    Release();
	}

	
// ---------------------------------------------------------
// CSpeedDialPrivate::Pbk2ImageGetFailed
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::Pbk2ImageGetFailed
        ( MPbk2ImageOperation& aOperation,
        TInt aError )
    {
    iError = aError;
    TInt index( HasImageOperation( aOperation ) );
    if ( index != KNullIndexData )
        {
        delete &aOperation;
        ( *iSdmArray )[index].SetImageOperation( NULL );
        if ( ( *iSdmArray )[index].LastThumb() )
            {
            CDesCArray* array = STATIC_CAST( CDesCArray*, iGrid->Model()->ItemTextArray() );
            array->Reset();
            TRAP_IGNORE( SetItemDataL( *array ) );
            DrawNow();
            }
        }
    iOperationComplete = ETrue;
    Release();
    }


// ---------------------------------------------------------
// CSpeedDialPrivate::HasImageOperation
//
// ---------------------------------------------------------
//
TInt CSpeedDialPrivate::HasImageOperation(MPbk2ImageOperation& aOperation)
{
	TInt index = KNullIndexData;
	for(TInt count =0; count < iSdmArray->Count() && index == KNullIndexData; ++count)
	{
	    if ((*iSdmArray)[count].ImageOperation() == &aOperation)
        {
             index = count;
        }		
	}
	return index;
}

// ---------------------------------------------------------
// CSpeedDialPrivate::SetDetails
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::SetDetails(TInt aIndex)
{
	MVPbkStoreContactField* field = (*iSdmArray)[aIndex].Field();
	
	if(field != NULL)
	{
    HBufC* number = NULL;
    TRAP_IGNORE(
            {
            TVPbkFieldStorageType dataType = field->FieldData().DataType();
            
            switch( dataType )
                {
                case EVPbkFieldStorageTypeText:
                    {
                    const MVPbkContactFieldTextData& data = MVPbkContactFieldTextData::Cast( field->FieldData() );
                    number = data.Text().AllocLC();
                    break;
                    }
                case EVPbkFieldStorageTypeUri:
                    {
                    const MVPbkContactFieldUriData& data = MVPbkContactFieldUriData::Cast( field->FieldData() );
                    number = data.Text().AllocLC();
                    break;
                    }
                }
        
            if (number != NULL)
				{
				//store the phone numbers in a array and cast those to TPhCltTelephoneNumber.
            	(*iSdmArray)[aIndex].SetPhoneNumber(*number);		           
            	CleanupStack::PopAndDestroy( number );
				}
			
			//Push
            const MVPbkFieldType* fieldType = field->BestMatchingFieldType();
            TInt iconindex = FindIconIndex(fieldType->FieldTypeResId(), field );
            (*iSdmArray)[aIndex].SetIconIndex(iconindex);
    
            //No need to fetch thumbnail now...will be updated while grid is
            // drawn
            (*iSdmArray)[aIndex].SetThumbIndex(-1,NULL);	
            }
        );	
	}
	    
}
// ---------------------------------------------------------
// CSpeedDialPrivate::AttributeOperationComplete
//
// ---------------------------------------------------------
//

void CSpeedDialPrivate::AttributeOperationComplete( MVPbkContactOperationBase& aOperation)
{

    TInt index = HasOperation(&aOperation);
     
     //for async call
    if (index != KNullIndexData)
    {
        TSpdiaIndexDataVPbk::TOperation operation = (*iSdmArray)[index].Operation();    
        //MVPbkStoreContactField* field = (*iSdmArray)[index].Field();
        
        switch(operation)
        {
            case TSpdiaIndexDataVPbk::OPERATION_ASSIGN:
            
                TRAPD(err,
                {
                                
                HBufC* prompt = StringLoader::LoadLC(R_SPDIA_TEXT_KEY_ASSIGNED, index+1, iCoeEnv);
                CAknConfirmationNote* note = new (ELeave)CAknConfirmationNote();
                note->ExecuteLD(*prompt);
                
                CleanupStack::PopAndDestroy(prompt);
                });
                if(err == KErrNone)
                {
                    SetDetails(index);
                }
            
            break;
            
            case TSpdiaIndexDataVPbk::OPERATION_REMOVE:
                (*iSdmArray)[index].Reset();                
            
            break;
            
            case TSpdiaIndexDataVPbk::OPERATION_NONE:
            
            //Error Handling
            
            break;
                    
            
        }
        
        iOperationComplete = ETrue;
        Release();
            
    }
    
  //  CleanupStack::PopAndDestroy();     
}

// ---------------------------------------------------------
// CSpeedDialPrivate::AttributeOperationFailed
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::AttributeOperationFailed(MVPbkContactOperationBase& aOperation, TInt aError) 
{
	
	TInt index(HasOperation(&aOperation));
	
	if (index != KNullIndexData)
	{
		TSpdiaIndexDataVPbk::TOperation operation = (*iSdmArray)[index].Operation();	
		
		switch(operation)
		{
			case TSpdiaIndexDataVPbk::OPERATION_ASSIGN:
				(*iSdmArray)[index].Reset();
			break;
			
			case TSpdiaIndexDataVPbk::OPERATION_REMOVE:
				(*iSdmArray)[index].Reset();
			break;
			
			default:
			 	delete &aOperation;
		}
		iError = aError;
		iOperationComplete = ETrue;
		Release();
		
	}
			

}


// ---------------------------------------------------------
// CSpeedDialPrivate::HasOperation
//
// ---------------------------------------------------------
//
TInt CSpeedDialPrivate::HasOperation(MVPbkContactOperationBase* aContactOpertion)
{
	TInt i=0, index = KNullIndexData;		
	for(i=0;i<iSdmArray->Count() && index == KNullIndexData;++i)
	{
		if ((*iSdmArray)[i].HasOperation(aContactOpertion))
		{
			index =i;
		}
	}
	
	return index;
}


// ---------------------------------------------------------
// CSpeedDialPrivate::EMail
//
// ---------------------------------------------------------
//
EXPORT_C HBufC* CSpeedDialPrivate::EMail( MVPbkStoreContact* aContact )
    {  

        TUint fetchFlags = 0;
        iMail = NULL;
        iFetchmail = ETrue;
        // Set service data
        TAiwSingleItemSelectionDataV3 data;

        data.SetFlags(fetchFlags);
        data.SetCommAddressSelectType(EAiwCommEmailEditor);
        TRAP_IGNORE(
                {
        CAiwGenericParamList& inParamList = iServiceHandler->InParamListL();
        inParamList.AppendL(TAiwGenericParam(EGenericParamContactSelectionData,
          TAiwVariant(TAiwSingleItemSelectionDataV3Pckg(data))));

        MVPbkContactLinkArray* linkArray = NULL;
        CVPbkContactLinkArray* array = CVPbkContactLinkArray::NewLC();
        MVPbkContactLink* ContactLink = aContact->CreateLinkLC();
        CleanupStack::Pop();
        array->AppendL(ContactLink);
        linkArray = array;
      
        HBufC8* PackLink = linkArray->PackLC();
        inParamList.AppendL(TAiwGenericParam(EGenericParamContactLinkArray,
          TAiwVariant(*PackLink)));
        CleanupStack::PopAndDestroy();//PackLink
        CleanupStack::PopAndDestroy();//linkArray
        CAiwGenericParamList& outParamList = iServiceHandler->OutParamListL();
        iServiceHandler->ExecuteServiceCmdL(KAiwCmdSelect, inParamList,
                outParamList, 0, this);
                }
                    );
        iFetchmailComplete = EFalse;
        // Wait till the contact is selected
        while ( !iFetchmailComplete )
        {
        WaitFetchMail();
        }
        iFetchmail = EFalse;
        return iMail;
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::SetSpeedDialIndexL
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::SetSpeedDialIndexL(MVPbkStoreContact* aContact)
{
	TInt count =0;
	TInt index = KNullIndexData;
	
	//	Do a linear search to find which is the speeddial index 
	// 	stored in this field.
	for(count =0; count<KArraySize && index ==KNullIndexData; ++count)
	{
		// Compare with every speeddial attribute
		iSpeedDial->SetIndex(count+1);
		
		//If the index is not assigned already..
		if((*iSdmArray)[count].Contact() == NULL)
		{
			MVPbkStoreContactFieldCollection* fieldCollection = iAttributeManager->FindFieldsWithAttributeLC(
                						*(iSpeedDial), *aContact); 
			if(fieldCollection && (fieldCollection->FieldCount() > 0))
			{
				index = count;
				
				MVPbkStoreContactField& field = fieldCollection->FieldAt(0);
				
				(*iSdmArray)[index].SetContactLink(field.CreateLinkLC());
				(*iSdmArray)[index].SetContact(aContact);
				CleanupStack::Pop();
				
	            TVPbkFieldStorageType dataType = field.FieldData().DataType();
	            HBufC* number = NULL;
	            switch( dataType )
                {
	                case EVPbkFieldStorageTypeText:
                    {
	                    const MVPbkContactFieldTextData& data = MVPbkContactFieldTextData::Cast( field.FieldData() );
	                    number = data.Text().AllocLC();
	                    break;
                    }
	                case EVPbkFieldStorageTypeUri:
                    {
	                    const MVPbkContactFieldUriData& data = MVPbkContactFieldUriData::Cast( field.FieldData() );
	                    number = data.Text().AllocLC();
	                    break;
                    }
                }
				
			    if ( number != NULL )
                {
					TPhCltTelephoneNumber phonenumber(*number);
                    phonenumber.Trim();
					CleanupStack::PopAndDestroy( number );
					//store the phone numbers in a array and cast those to TPhCltTelephoneNumber.
                    if(phonenumber.Length() > 0)
                    {
                        (*iSdmArray)[index].SetPhoneNumber(phonenumber);
                    }
                }
                else
                {
                    //Remove SpeedDial
                }
			}
			
			CleanupStack::PopAndDestroy(); // Pop and Destroy FieldCollection
			
		}
	}
	//Delete if we are not using that contact..
	if(KNullIndexData == index)
	{
		delete aContact;
	}
	
}
	
// ---------------------------------------------------------
// CSpeedDialPrivate::VPbkSingleContactOperationComplete()
//
// ---------------------------------------------------------
void CSpeedDialPrivate::VPbkSingleContactOperationComplete( MVPbkContactOperationBase& aOperation, 
													MVPbkStoreContact* aContact)
{

 	if (iState == STATE_INITIAL)
 	{
 		TRAP_IGNORE(SetSpeedDialIndexL(aContact));
 		iRetrieveComplete = ETrue;
		Release();
 	}
 	else if(iState == STATE_CONTACTCHANGE)
 	{
 		iContact = aContact;
 		iRetrieveComplete = ETrue;
		Release();
 	}
 	else
 	{
		
		TInt index = HasOperation(&aOperation);
		
		if (index != KNullIndexData)
		{
			TSpdiaIndexDataVPbk::TOperation operation = (*iSdmArray)[index].Operation();
			if(TSpdiaIndexDataVPbk::OPERATION_ASSIGN == operation)
			{	
				
				// Store the contact and field in corresponding index
				(*iSdmArray)[index].SetContact(aContact);
				(*iSdmArray)[index].SetContactLink(iContactLink);
				iRetrieveComplete = ETrue;
				Release();
						
			}
			else if(TSpdiaIndexDataVPbk::OPERATION_ASSIGN_CONTACT == operation)
			{
          iSpeedDial->SetIndex(index + 1);
          TRAP_IGNORE(
              {
              MVPbkStoreContactFieldCollection* fieldCollection = iAttributeManager->FindFieldsWithAttributeLC( *( iSpeedDial ), *aContact ); 
              if ( fieldCollection && ( fieldCollection->FieldCount() > 0 ) )
                  {
                  // update speeddial information
                  MVPbkStoreContactField& field = fieldCollection->FieldAt( 0 );
                  ( *iSdmArray )[ index ].SetContactLink( field.CreateLinkLC() );
                  ( *iSdmArray )[ index ].SetContact( aContact );
                  CleanupStack::Pop();
                  SetDetails( index );
                  }

          		CleanupStack::PopAndDestroy(); // Pop and Destroy FieldCollection
              }
          )				
				  iRetrieveComplete = ETrue;
				  Release();
			}
		 	//delete &aOperation;
		}
 	}
}

// ---------------------------------------------------------
// CSpeedDialPrivate::VPbkSingleContactOperationFailed()
//
// ---------------------------------------------------------
void CSpeedDialPrivate::VPbkSingleContactOperationFailed(
                 MVPbkContactOperationBase& /*aOperation*/, 
                 TInt aError)
{
	iError = aError;
	iRetrieveComplete = ETrue;
	Release();

}
                 
// ---------------------------------------------------------
// CSpeedDialPrivate::ShowAssignL()
//
// ---------------------------------------------------------
TBool CSpeedDialPrivate::ShowAssignL( MVPbkContactLink& aContactLink, TInt& aIndex )
{
	TBool aReturn (EFalse);
	
    TRAPD(err,iGridDlg = CSpdiaGridDlgVPbk::NewL(aIndex, *this));
    
    // Execute the CPbkFetchEntryDlg dialog
    TInt result(ETrue);
    
	if(err == KErrNone)
	{
	    TRAPD(error, result = iGridDlg->ExecuteLD(R_SPDIA_GRID_DIALOG));
    	iGridDlg = NULL; 
    	iIconArray = NULL;	
    	 // Check errors
    	User::LeaveIfError(error);
	}

  
   
   	TBool popUp = ETrue;
   	
    if (iBooleanVarForAddingDialogVariation)
	{
		if(!CheckingIfPopUpNeededL())
		  popUp = EFalse;
	}
   	if (popUp && result && !CheckSpaceBelowCriticalLevelL())
   	{
	//for contact link
  	TUint assignFlags = 0; 
    ExecuteSingleEntryAssignL(aIndex-1,AiwContactAssign:: TAiwContactAttributeAssignDataV1().SetFlags(assignFlags));//amit
    aReturn = ETrue;
    //return aReturn;
    // for contact link
		   	    	    
		if((*iSdmArray)[aIndex-1].Contact() != NULL)
		{
			result = RemoveSpeedDialL(aIndex-1);
		}
		
		//If removal was successful
		if(result != EFalse)
		{
		   	iRetrieveComplete = EFalse;
		   	if(iContactLink)
		   	{
		   		delete iContactLink;
		   		iContactLink= NULL;
		   	}
		 	iContactLink = aContactLink.CloneLC();
		 	CleanupStack::Pop();
		 	
	    MVPbkContactOperationBase* operation = iContactManager->RetrieveContactL( aContactLink, (*this ));
		   		      
		    if(operation)
		    {
				(*iSdmArray)[aIndex-1].SetOperation(operation,TSpdiaIndexDataVPbk::OPERATION_ASSIGN);
				
				while (!iRetrieveComplete)
			    {
			    	Wait();
			    }
			    //If retrieval is successful
				if(iError == KErrNone)
				{
					AssignSpeedDialL(aIndex-1);				
					aReturn = ETrue;
				}
				else
				{
					delete iContactLink;
				}
			   	
		    }
		}
	    
	    iContactLink = NULL;
   	}

	return aReturn;	   	
}

// ---------------------------------------------------------
// CSpeedDialPrivate::ShowAssignL()
//
// ---------------------------------------------------------
TBool CSpeedDialPrivate::ShowAssignL( TInt aIndex, MVPbkContactLink*& aContactLink  )
    {

    // Have to display the title of SpeedDial when called 
    // from SpeedDial application...May be that function can be moved to
    // speed dial application...

    TBool result = ETrue;
    HBufC* prompt = NULL;
    TBuf<KOperatorNameLength> operatorName;
    
    aIndex = aIndex-1;
    if (iBooleanVarForAddingDialogVariation )
        {
        if(iPopupDialog)
            {
            if(!CheckingIfPopUpNeededL())
            result = EFalse;
            }
        }

    if ( !CheckSpaceBelowCriticalLevelL() )
        {
        if ( result && (*iSdmArray)[aIndex].Contact() != NULL )
            {
            //if(iGridUsed == EGridNoUse)//because grid will be used if you want to replace or change
            if ( iGridUsed == EGridUse )
                {
                MakeVisible( ETrue );
                result = ETrue;
                MakeVisible( EFalse );
                }
            }

        if(result)
            {
            TUint assignFlags = 0;            
            //assignFlags |= (AiwContactAssign::EUseProviderTitle);//|AiwContactAssign::EDoNotOpenEditor); 
            CAknTitlePane* title = NULL;
            CEikStatusPane *sp = CEikonEnv::Static()->AppUiFactory()->StatusPane();
            title =  STATIC_CAST(CAknTitlePane*, sp->ControlL( TUid::Uid(EEikStatusPaneUidTitle) ) );
            prompt = StringLoader::LoadLC(R_QTN_SPDIA_TITLE, iCoeEnv);
            operatorName.Copy(*title->Text());
            title->SetTextL(*prompt);
            title->MakeVisible(ETrue);
            CleanupStack::PopAndDestroy(prompt); 
            
            ExecuteSingleEntryAssignL(aIndex,AiwContactAssign:: TAiwContactAttributeAssignDataV1().SetFlags(assignFlags));

            if ( iCancelFlag )
                {
                iCancelFlag = EFalse;
                return result;
                }

            if ( iError == KErrNone && iContactLink != NULL )
                {
                (*iSdmArray)[aIndex].Reset();
                //Retrieve Contact and assign fields to the index..
                iRetrieveComplete = EFalse;
                MVPbkContactOperationBase* operation = iContactManager->RetrieveContactL( *iContactLink, (*this ));

                if(operation)
                    {
                    (*iSdmArray)[aIndex].SetOperation(operation,TSpdiaIndexDataVPbk::OPERATION_ASSIGN_CONTACT);

                    while (!iRetrieveComplete)
                        {
                        Wait();
                        }
                    if ( iError == KErrNone )
                        {
                        result = ETrue;	
                        }
                    else
                        {
                        //We are taking a copy of contact links..
                        //If we can't store it, delete the link..
                        delete iContactLink;
                        iContactLink = NULL;
                        result = EFalse;
                        }
                    }
                }

            else if( iError == KErrInUse )
                {
                ShowNoteL( R_QTN_SPDIA_CONTACT_IN_USE );
                }

            if(iContactLink != NULL)
                {
                aContactLink = iContactLink->CloneLC();
                CleanupStack::Pop();	    		
                delete iContactLink;
                }
            else
                {
                aContactLink = CONST_CAST(MVPbkContactLink*, iContactLink);
                }
            iContactLink = NULL;

            title->SetTextL( operatorName );
            } 
        }

        return result;

    }

// ---------------------------------------------------------
// CSpeedDialPrivate::ShowRemoveL
//
// ---------------------------------------------------------
//
TBool CSpeedDialPrivate::ShowRemoveL( TInt aIndex )
{

	TBool aReturn = EFalse;
		
    if ( (!CheckSpaceBelowCriticalLevelL() )&&( (*iSdmArray)[aIndex].Contact()!= NULL ))
	{
		iState = STATE_REMOVE;
		if((*iSdmArray)[aIndex].Field() != NULL)
		{
			iOperationComplete = EFalse;
			iSpeedDial->SetIndex(aIndex+1);
			(*iSdmArray)[aIndex].Contact()->LockL(*(this));

			while(!iOperationComplete)
			{
				Wait();
			}

			if(iError == KErrNone)
			{
				
				MVPbkStoreContactField* field = (*iSdmArray)[aIndex].Field();
				TPtrC label = field->FieldLabel();
				iOperationComplete = EFalse;
				MVPbkContactOperationBase* operation = NULL; 
				operation = iAttributeManager->RemoveFieldAttributeL(
														*field, *iSpeedDial, (*this));
				if(operation)
				{
					(*iSdmArray)[aIndex].SetOperation(operation,TSpdiaIndexDataVPbk::OPERATION_REMOVE);
				
					while(!iOperationComplete)
					{
						Wait();
					}

				}
				
				//If there are no errors
				if(iError == KErrNone)
				{
					aReturn = ETrue;
				}
			}
            else if( iError == KErrInUse )
                {
                // Prompt information note when contact is in use
                ShowNoteL( R_QTN_SPDIA_CONTACT_IN_USE );
                }
		}
	}
	
	return aReturn;
}

// ---------------------------------------------------------
// CSpeedDialPrivate::ShowAssign
// Function called when assigning speed dial from phone book
// application
// ---------------------------------------------------------
//
EXPORT_C TInt CSpeedDialPrivate::ShowAssign( MVPbkContactLink& aContactLink, TInt& aIndex )
   	{
   	iError = KErrNone;
   	iCurrentIndex = aIndex - 1;

   	TRAPD( err, ShowAssignL( aContactLink, aIndex ) );

   	if ( err )
   	   	{
   	   	iError = err;
   	   	}

   	iCurrentIndex = -1;
   	return iError;	
   	}
	
// ---------------------------------------------------------
// CSpdiaContainer::ShowAssignL
// Function called when assigning speed dial from SpeedDial and Telephony
// application
// ---------------------------------------------------------
//
	
EXPORT_C TInt CSpeedDialPrivate::ShowAssign( TInt aIndex, MVPbkContactLink*& aContactLink  )
   	{
   	iError = KErrNone;
   	if ( !iShowAssignCalled )
   	   	{
   	   	iShowAssignCalled = ETrue;
   	   	iCurrentIndex = aIndex - 1;

   	   	TRAPD( err, ShowAssignL( aIndex, aContactLink ) );

   	   	if ( err )
   	   	   	{
   	   	   	iError = err;
   	   	   	}
   	   	iCurrentIndex = -1;
   	   	iShowAssignCalled = EFalse;
   	   	} 	

   	return iError;
   	}

EXPORT_C TInt CSpeedDialPrivate::ShowRemove( TInt aIndex )
   	{
   	iError = KErrNone;

   	iCurrentIndex = aIndex;
   	TInt reValue = ETrue;
   	TRAP_IGNORE( reValue = ShowRemoveL( aIndex ) );

   	iCurrentIndex = -1;
   	return reValue;
   	}

    
// --------------------------------------------------------------------------
// CSpeedDialPrivate::StoreReady
//
// --------------------------------------------------------------------------
//
void CSpeedDialPrivate::StoreReady(MVPbkContactStore& /*aContactStore*/)
    {
    	iOperationComplete = ETrue;
    	Release();
    }
    
// --------------------------------------------------------------------------
// CSpeedDialPrivate::StoreUnavailable
//
// --------------------------------------------------------------------------
//
void CSpeedDialPrivate::StoreUnavailable
        (MVPbkContactStore& /*aContactStore*/, TInt /*aReason*/)
    {
    	iOperationComplete = ETrue;
    	Release();

    	//iError = aReason;
    }

// --------------------------------------------------------------------------
// CSpeedDialPrivate::SetState
//
// --------------------------------------------------------------------------
//    
EXPORT_C void CSpeedDialPrivate::SetState(eSpeedDialState aState)
{
	iState = aState;
}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::State
//
// --------------------------------------------------------------------------
// 
EXPORT_C  CSpeedDialPrivate::eSpeedDialState CSpeedDialPrivate::State()
{
	return iState;
}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::GridStatus
//
// --------------------------------------------------------------------------
// 
EXPORT_C  CSpeedDialPrivate::TGridUse CSpeedDialPrivate::GridStatus()
{
	return iGridUsed;
}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::SetGridStatus
//
// --------------------------------------------------------------------------
// 
EXPORT_C  void CSpeedDialPrivate::SetGridStatus(TGridUse aStatus)
{
	iGridUsed = aStatus;
}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::HandleStoreEventL
//
// --------------------------------------------------------------------------
//
void CSpeedDialPrivate::HandleStoreEventL(
        MVPbkContactStore& aContactStore,
        TVPbkContactStoreEvent aStoreEvent)
	{
	
	MVPbkContactStore* storeInList = iContactManager->ContactStoresL().Find(aContactStore.StoreProperties().Uri());
	
	if (storeInList !=NULL)
	{
	switch(aStoreEvent.iEventType)
		{
		
		case TVPbkContactStoreEvent::EContactChanged:
		case TVPbkContactStoreEvent::EContactDeleted:
		
		if ( iState == STATE_IDLE /*&& iUpdateFlag == EFalse*/ )
			{
			
			TInt findResult = 0;
			findResult = FindInSpeedDialContacts(aStoreEvent.iContactLink);
			
			if(findResult != EFalse)
				{
				if (iGridUsed != EGridUse)
					{
					if ( iRemoveConfirmQueryDialog 
						&& aStoreEvent.iEventType == TVPbkContactStoreEvent:: EContactChanged
						&& iCurrentIndex == findResult )
						{
						delete iRemoveConfirmQueryDialog;
						iRemoveConfirmQueryDialog = NULL;
						}
					iUpdateFlag = ETrue;
					// if CSpeedDialPrivate object is constructed by dialer, the value of iGrid is NULL
					if ( iGrid )
						{
						CreateDataL( *iGrid  );
						}
					// If the view number note popped up and the customer delete the image of contact
					// Set the update flag.
					if ( iRefreshObserver && iCurrentIndex == findResult )
						{
						iUpdateFlag = ETrue;
						}
					}
				}
			else
				{
				if(CheckContactAssignedFromPhoneBookL(aStoreEvent.iContactLink))
					{
					if (iGridUsed != EGridUse)
						{
						iUpdateFlag = ETrue;
						// if CSpeedDialPrivate object is constructed by dialer, the value of iGrid is NULL
						if ( iGrid )
							{
							CreateDataL( *iGrid  );
							}
						}
					}
				
				}
			}	
		break;
		
		default:
		break;
		}
	
	}
	
	}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::SetUpdateFlag
//
// --------------------------------------------------------------------------
//
EXPORT_C void CSpeedDialPrivate::SetUpdateFlag(TBool aFlag)
{
	iUpdateFlag = aFlag;
}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::UpdateFlag
//
// --------------------------------------------------------------------------
//

EXPORT_C TBool CSpeedDialPrivate::UpdateFlag()
{
	return iUpdateFlag;
}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::FindInSpeedDialContacts
//
// --------------------------------------------------------------------------
//
    
TInt CSpeedDialPrivate::FindInSpeedDialContacts(MVPbkContactLink* aContactLink)    
    {
    // TBool found = EFalse;
    TInt found = 0;
    MVPbkStoreContact* contact = NULL;
    for ( TInt count = 0; count < iSdmArray->Count() && found != iCurrentIndex; ++count )
        {
        contact = ( *iSdmArray )[count].Contact();
        
        if ( contact != NULL && aContactLink->RefersTo( *contact ) )
            {
            found = iSdmArray->Count();
            // if the changed contact is the one which is focused in Speeddial,
            // then the note can be removed
            if ( count == iCurrentIndex )
                {
                found = iCurrentIndex;
                }
            }
        }
    return found;
    }

// --------------------------------------------------------------------------
// CSpeedDialPrivate::CheckContactAssignedFromPhoneBookL
//
// --------------------------------------------------------------------------
//
TBool CSpeedDialPrivate::CheckContactAssignedFromPhoneBookL(MVPbkContactLink* aContactLink)
{
	TBool aReturn = EFalse;
	iRetrieveComplete = EFalse;
	MVPbkContactOperationBase* operation = iContactManager->RetrieveContactL( *aContactLink,
											(*this ));
	//Save the old state																
	eSpeedDialState oldState = iState;
	
	iState = STATE_CONTACTCHANGE;
	if(operation != NULL)
	{
		while(!iRetrieveComplete)
		{
			Wait();
		}
		
		delete operation;
		if(iContact != NULL)
		{
			aReturn = iAttributeManager->HasContactAttributeL(
										CVPbkSpeedDialAttribute::Uid(),*iContact);
			delete iContact;
			iContact = NULL;
		}	
	}

	//Restore the old state
	iState  = oldState;
	return aReturn;
	
}


// --------------------------------------------------------------------------
// CSpeedDialPrivate::ExecuteSingleEntryAssignL
//
// --------------------------------------------------------------------------
//
void CSpeedDialPrivate::ExecuteSingleEntryAssignL(TInt aIndex,
        AiwContactAssign::TAiwContactAttributeAssignDataV1 aData)
    { 
    iState = STATE_ASSIGN;
        
    CAiwGenericParamList& inParamList = iServiceHandler->InParamListL(); 
    
    const TAiwVariant speedDialVariant( aIndex + 1 );
    /*
    TGenericParamId genericParam =
        TGenericParamId( EGenericParamSpeedDialIndex );*/
	//For the time being..phone book ui lib is compiled with this value...     
    TGenericParamId genericParam =
        TGenericParamId( EGenericParamSpeedDialIndex );
  
    inParamList.AppendL(TAiwGenericParam(
                    genericParam,
                    speedDialVariant
                    ) );  
  
    inParamList.AppendL( TAiwGenericParam( EGenericParamContactAssignData,
            TAiwVariant( AiwContactAssign::TAiwContactAttributeAssignDataV1Pckg( aData ) ) ) );
                                     

	iOperationComplete = EFalse;
	iContactLink = NULL;
    iServiceHandler->ExecuteServiceCmdL(
            KAiwCmdAssign, 
            inParamList, 
            iServiceHandler->OutParamListL(),
            0,
            this );                
    
    // Wait till the contact is selected
    while ( !iOperationComplete )
        {
        Wait();
        }
    
    // CleanupStack::PopAndDestroy();
    //iServiceHandler = NULL;
    }

// --------------------------------------------------------------------------
// CSpeedDialPrivate::ExecuteSingleEntryFetchL
//
// --------------------------------------------------------------------------
//
void CSpeedDialPrivate::ExecuteSingleEntryFetchL(TInt aIndex,
        TAiwSingleEntrySelectionDataV2 aData,
        const TDesC& aUri1)
{ 
    CAiwServiceHandler* serviceHandler = CAiwServiceHandler::NewL();
    
    serviceHandler->AttachL(R_SPDIA_SINGLE_ENTRY_FETCH_INTEREST);
    
    CleanupStack::PushL(serviceHandler);  
        
    CAiwGenericParamList& inParamList = serviceHandler->InParamListL();    
    inParamList.AppendL(
        TAiwGenericParam(
            EGenericParamContactSelectionData,
            TAiwVariant(TAiwSingleEntrySelectionDataV2Pckg(aData))));

    CVPbkContactStoreUriArray* uriArray = CVPbkContactStoreUriArray::NewL();
    CleanupStack::PushL(uriArray);
    if (aUri1.Length() > 0) 
	    {
	    uriArray->AppendL(aUri1);
			}  

    HBufC8* packedUris = uriArray->PackLC();
    if (uriArray->Count() > 0)
        {
        inParamList.AppendL(
        TAiwGenericParam(
                EGenericParamContactStoreUriArray,
                TAiwVariant(*packedUris)));
        }

	iOperationComplete = EFalse;
	iContactLink = NULL;
    serviceHandler->ExecuteServiceCmdL(
            KAiwCmdSelect, 
            inParamList, 
            serviceHandler->OutParamListL(),
            0,
            this);                
    CleanupStack::PopAndDestroy(2); // packedUris, uriArray, service handler
    
    //Wait till the contact is selected
    while (!iOperationComplete)
    {
    	Wait();
    }
    
    CleanupStack::PopAndDestroy(serviceHandler);
    
    if(iContactLink && !CheckSpaceBelowCriticalLevelL())
    {
    	iRetrieveComplete = EFalse;
    	
    	MVPbkContactOperationBase* operation = iContactManager->RetrieveContactL( *iContactLink, (*this ));
	      
	    if(operation)
	    {
			(*iSdmArray)[aIndex].SetOperation(operation,TSpdiaIndexDataVPbk::OPERATION_ASSIGN);
			
			while (!iRetrieveComplete)
		    {
		    	Wait();
		    }
		    
		    ShowRemove(aIndex);
			if(iError == 0)
			{
				
			}   	
	    }
	    
    }
}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::HandleNotifyL
//
// --------------------------------------------------------------------------
//
TInt CSpeedDialPrivate::HandleNotifyL(
        TInt aCmdId,
        TInt aEventId,
        CAiwGenericParamList& aEventParamList,
        const CAiwGenericParamList& /*aInParamList*/)
{
    TInt result = 0;
    iContactLink = NULL;
    
      if ((aCmdId == KAiwCmdSelect) || (aCmdId == KAiwCmdAssign))
        {
        if (aEventId == KAiwEventCompleted)
            {
            TInt index = 0;
            const TAiwGenericParam* param =
                aEventParamList.FindFirst(index, EGenericParamContactLinkArray);
            if ( !iFetchmail && param)
                {
                TPtrC8 contactLinks = param->Value().AsData();
                CVPbkContactLinkArray* links = CVPbkContactLinkArray::NewLC(
                        contactLinks,iContactManager->ContactStoresL());
	            
	            if(links->Count() > 0)
	           	{
	           	    if(iContactLink)
		         	{
		   	        	delete iContactLink;
		   		        iContactLink= NULL;
		   	         }
		  
	           		iContactLink = links->At(0).CloneLC();
	           		CleanupStack::Pop();
	           	}
	           	
                CleanupStack::PopAndDestroy(links);
                }
                
            index = 1;
            const TAiwGenericParam* paramMail =
                aEventParamList.FindFirst(index, EGenericParamContactFieldData);   
            if ( iFetchmail && paramMail )
            	{  
                TInt length = paramMail->Value().AsDes().Length();
                iMail = HBufC::NewL(length);
                iMail->Des().Append(paramMail->Value().AsDes());
            	}            

            if( iFetchmail )
                {
                iFetchmailComplete = ETrue;
                ReleaseFetchMail();
                }
            else
                {
                iOperationComplete = ETrue;
                Release();
                }  

            }
        else if (aEventId == KAiwEventCanceled)
            {
            	iError = KErrCancel;    

                if( iFetchmail )
                    {
                    iFetchmailComplete = ETrue;
                    ReleaseFetchMail();
                    }
                else
                    {
                    iOperationComplete = ETrue;
                    Release();
	                }
            }
         else if (aEventId == KAiwEventError)
         	{
         	   	iError = KErrInUse;    
	           	iOperationComplete = ETrue;
	    		Release();
         	}
         else if(aEventId == KAiwEventQueryExit)	
         	{
                iError = KLeaveExit;    
	            iOperationComplete = ETrue;
	    		Release();
                result = 1;
           	}
       }

    return result;
 }

// --------------------------------------------------------------------------
// CSpeedDialPrivate::ContactOperationCompleted
//
// --------------------------------------------------------------------------
//
void CSpeedDialPrivate::ContactOperationCompleted(TContactOpResult /*aResult*/)
{
	iOperationComplete = ETrue;
	Release();

}

// --------------------------------------------------------------------------
// CSpeedDialPrivate::ContactOperationFailed
//
// --------------------------------------------------------------------------
//
void CSpeedDialPrivate::ContactOperationFailed(TContactOp /*aOpCode*/, TInt aErrorCode, 
        							TBool /*aErrorNotified*/)
{
	iError = aErrorCode;
	iOperationComplete = ETrue;
	Release();
}

// ---------------------------------------------------------
// CSpeedDialPrivate::InitIndexDataL
// The SDM control data is initialized.
// ---------------------------------------------------------
//
void CSpeedDialPrivate::InitIndexDataL()
{
    // The dialog of grid is not closed.
    if (iIconArray != NULL)
    {
        iError = KErrNone;
        ResetArray();
        //State set to identify the source of call during callback.
        
        //Save the old state
        eSpeedDialState oldState = iState;
        
        iState = STATE_INITIAL;
        iOperationComplete = EFalse;

		/*Delete already fetched contacts if any */
		if(iContactLinkArray != NULL)
		{
			delete iContactLinkArray;
			iContactLinkArray = NULL;
		}
        //Fetch all contacts for which speed dial attribute is set.
        iFetchAll = EFalse;
        iSpeedDial->SetIndex(CVPbkSpeedDialAttribute::KSpeedDialIndexNotDefined);
       // iSpeedDial->SetIndex(2);
       	MVPbkContactOperationBase* findOperation = iAttributeManager->ListContactsL(*iSpeedDial,(*this));
       
        while(findOperation != NULL && !iFetchAll)
        {
        	//Wait till every contactlink is fetched and set.
        	Wait();
        }
        //Delete the handle
        delete findOperation;
        
        MVPbkContactOperationBase* retrieveOperation = NULL;
        for(TInt count =0; iContactLinkArray && count < iContactLinkArray->Count();++count)
        {
        	iRetrieveComplete = EFalse;     
        	iContactLink = &(iContactLinkArray->At(count));
	 		retrieveOperation = iContactManager->RetrieveContactL(iContactLinkArray->At(count),(*this ));
			while(retrieveOperation != NULL && !iRetrieveComplete)
			{
				//Wait till the contact is fetched and its speed dial 
				//index is initialized.
				Wait();
			}
			//Delete the handle received
			delete retrieveOperation;

        }
        // Undo this reset 
        // Just for debugging...
        if(iError != KErrNone)
        {
        	iError = KErrNone;
        }
		//Reset the state 
		iState = oldState;
    
    }
}

// ---------------------------------------------------------
// CSpeedDialPrivate::FindCompleteL
// .
// ---------------------------------------------------------
//
void CSpeedDialPrivate::FindCompleteL(MVPbkContactLinkArray* aResults)
{
	
	iContactLinkArray = aResults;
	iFetchAll = ETrue;
	Release();
	
	
}

// ---------------------------------------------------------
// CSpeedDialPrivate::FindFailed
// 
// ---------------------------------------------------------
//
void CSpeedDialPrivate::FindFailed(TInt aError)
{
	iError = aError;
	iFetchAll = ETrue;
	Release();
}
// ---------------------------------------------------------
// CSpdiaContainer::CreateItemDescriptorL
//
// ---------------------------------------------------------
//
HBufC* CSpeedDialPrivate::CreateItemDescriptorLC(const TSpdiaIndexDataVPbk& aSdmData)
    {
    HBufC* des = HBufC::NewLC(KApaMaxAppCaption);
    TPtr ptr(des->Des());

    // thumb type
    TInt type(CSpdiaGridVPbk::EText);
    if (aSdmData.ThumbIndex() >= 0)
        {
        type = ConvThumbType(aSdmData.ThumbSize());
        }

    // Text
    if ( type == CSpdiaGridVPbk::EText )
        {
        // number type icon is missing
        TBuf<KPhCltNameBufferLength> iconInfo;
        iconInfo.Append( KDesTab );       // 1
        if ( aSdmData.IconIndex() >= 0 )
            {
            iconInfo.AppendNum( aSdmData.IconIndex() );
            }
        AppendTextL( aSdmData, ptr ); //  2,3,4
        ptr.Insert( 0, iconInfo );
        }
    else
        {
        // icon + text + text + text
        ptr.Append(KDesTab2);       // 1, 2
        ptr.Append(KDesTab2);       // 2
        }

    // thumbnail 5-14
    for (TInt i(CSpdiaGridVPbk::EtnCIF90); i <= CSpdiaGridVPbk::EtnCOM; ++i)
        {
        AppendThumbnail(aSdmData, ptr, (type == i? ETrue: EFalse));
        }

    // number
    ptr.Append(KDesTab);        // 15
    // check
    ptr.Append(KDesTab);        // 16

    return des;
    }


// ---------------------------------------------------------
// CSpeedDialPrivate::CreateDataL
// Set grid used flag and calls CreateDataL.
// ---------------------------------------------------------
//
TBool CSpeedDialPrivate::CreateDataL(const CAknGrid& aGrid)
    {
    TGridUse  oldStatus;
    
    TBool result;
    
    oldStatus = iGridUsed;
    
    iGridUsed = EGridUseAndUpdate;

    result = CreateGridDataL(CONST_CAST(CAknGrid*, &aGrid), KNullIndexData);
    
    iGridUsed = oldStatus;
    
    return result;
    }
    
    	
// ---------------------------------------------------------
// CSpeedDialPrivate::AppendTextL
// output Text1\tText2\tText3\t
// ---------------------------------------------------------
//
void CSpeedDialPrivate::AppendTextL(const TSpdiaIndexDataVPbk& aSdmData, TPtr& aText)
    {
    if ( aSdmData.Contact() == NULL )
        {
        aText.Append( KDesTab2 );
        aText.Append( KDesTab2 );
        }
    else
        {
        // Test application name formatting logic is that take last name and
        // first name and combine them using space as a separator. If either of
        // them exists use company name. If there are still no name use "Unnamed"
	    
        // Get field types from the master field type list
        const MVPbkFieldType* lastNameType = NULL; 
	      const MVPbkFieldType* firstNameType = NULL;           
	      const MVPbkFieldType* companyNameType =
                iContactManager->FieldTypes().Find( R_VPBK_FIELD_TYPE_COMPANYNAME );
	    
        TInt language = User::Language();
	    
        CDesCArrayFlat* bufs = new( ELeave ) CDesCArrayFlat( 2 );
        CleanupStack::PushL( bufs );
	    
        // Non-graphical characters are replaced with space
        const TText KReplaceChar = ' ';
	        
        if ( ( language == ELangTaiwanChinese )
            || ( language == ELangHongKongChinese )
            || ( language == ELangPrcChinese ) )
     		    {
     		    lastNameType = iContactManager->FieldTypes().Find( R_VPBK_FIELD_TYPE_LASTNAME );
     		    firstNameType = iContactManager->FieldTypes().Find( R_VPBK_FIELD_TYPE_FIRSTNAME );            
     		    }
        else
            {
            // Change the display order as first name - last name.
            lastNameType = iContactManager->FieldTypes().Find( R_VPBK_FIELD_TYPE_FIRSTNAME );
            firstNameType = iContactManager->FieldTypes().Find( R_VPBK_FIELD_TYPE_LASTNAME );      
            }
            
        // Contact should have only one last name field
        CVPbkBaseContactFieldTypeIterator* itr = 
            CVPbkBaseContactFieldTypeIterator::NewLC( *lastNameType, aSdmData.Contact()->Fields() );
        while ( itr->HasNext() )
            {
	          const MVPbkBaseContactField* field = itr->Next();
	          // last name field is text data
	          const MVPbkContactFieldTextData& data = 
	              MVPbkContactFieldTextData::Cast( field->FieldData() );
	          HBufC* lastName = data.Text().AllocLC();
	        
	          TPtr lName(NULL, 0);
	          lName.Set( lastName->Des() );
         	  ReplaceNonGraphicCharacters( lName, KReplaceChar );
         	
         	  const TInt 	len = lName.Length();
         	  if ( 0 < len )
                {
                bufs->AppendL( lName);
                }
	        
            CleanupStack::PopAndDestroy( lastName );
            }
        CleanupStack::PopAndDestroy( itr );
	    
	    // Contact should have only one first name field
	    itr = CVPbkBaseContactFieldTypeIterator::NewLC( *firstNameType, 
	        aSdmData.Contact()->Fields() );
	    while ( itr->HasNext() )
	        {
	        const MVPbkBaseContactField* field = itr->Next();
	        // last name field is text data
	        const MVPbkContactFieldTextData& data = 
	            MVPbkContactFieldTextData::Cast( field->FieldData() );
	        
	        HBufC* firstName = data.Text().AllocLC();

	        TPtr fName(NULL, 0);
	        fName.Set( firstName->Des() );
         	ReplaceNonGraphicCharacters( fName, KReplaceChar );
         	
         	const TInt 	len = fName.Length();
         	if (0 < len)
         	{
         		bufs->AppendL( fName);
        	}
	        
			CleanupStack::PopAndDestroy( firstName );
	        }
	    CleanupStack::PopAndDestroy( itr );
	    
	    // No last and first name try company name
	    if ( bufs->Count() == 0 )
	        {
	        // Contact should have only one company name field
	        itr = CVPbkBaseContactFieldTypeIterator::NewLC( *companyNameType, 
	            aSdmData.Contact()->Fields() );
	        while ( itr->HasNext() )
	            {
	            const MVPbkBaseContactField* field = itr->Next();
	            // last name field is text data
	            const MVPbkContactFieldTextData& data = 
	                MVPbkContactFieldTextData::Cast( field->FieldData() );
	            HBufC* companyName = data.Text().AllocLC();
	            bufs->AppendL( *companyName );
	            CleanupStack::PopAndDestroy( companyName );
	            }
	        CleanupStack::PopAndDestroy( itr );
	        }
	    
	  	aText.Append(KDesTab);
	    // If still no name use phone number
	    if ( bufs->Count() == 0 )
	        {
            	aText.Append(aSdmData.PhoneNumber());
            	aText.Append(KDesTab); 
	        }
	    else
	    {
		    for ( TInt i = 0; i < bufs->Count(); ++i )
		    {
		        aText.Append( (*bufs)[i] );
		        aText.Append(KDesTab);
		    }
	    	
	    }
	    
		aText.Append(KDesTab);
	    CleanupStack::PopAndDestroy( bufs );	
    }
    
	AknTextUtils::DisplayTextLanguageSpecificNumberConversion (aText);
}
   

// ---------------------------------------------------------
// CSpdiaContainer::SetOperationsL
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::SetOperationsL()
    {
    for (TInt index(iSdmArray->Count() -1); index > 0; --index)
        {
        TSpdiaIndexDataVPbk& data = (*iSdmArray)[index];
        
        if (data.ThumbIndex() >=0)
            {
            iOperationComplete = EFalse;
            data.SetImageOperation(
                    iImageManager->GetImageAsyncL(NULL,*(data.Contact()),*iThumbNailFieldType,(*this) ) );
            while ( !iOperationComplete )
                {
                Wait();
                }
            }
        }
    }

// ---------------------------------------------------------
// CSpdiaContainer::UpdateIndexDataL
//
// ---------------------------------------------------------
//

TBool CSpeedDialPrivate::UpdateIndexDataL(TInt aIndex)
    {
	CFbsBitmap* bitmap,*mask;
	bitmap = NULL;
	mask= NULL;
	MAknsSkinInstance* skinInstance = AknsUtils::SkinInstance();
	

    TSpdiaIndexDataVPbk& data = (*iSdmArray)[aIndex];
    delete (*iIconArray)[aIndex];
    (*iIconArray)[aIndex] = NULL;
    TFileName bmpPath(iBmpPath);

	if(	User::Language() == ELangHindi )
	{
		AknsUtils::CreateIconL(skinInstance, (*iDialSkinBmp)[aIndex],bitmap,mask,bmpPath,iDialHindiBmp[aIndex],  iDialHindiBmpMask[aIndex] );
	}
	else
	{
		AknsUtils::CreateIconL(skinInstance, (*iDialSkinBmp)[aIndex],bitmap,mask,bmpPath,iDialBmp[aIndex],  iDialBmpMask[aIndex] );
	}
	(*iIconArray)[aIndex] = CGulIcon::NewL(bitmap,mask);
	
	
	
	MVPbkStoreContact* aContact = data.Contact();

    data.ResetThumbIndex();
    if (CreateIndexIconL(aIndex, data, ETrue))
        {
        
    	if (iImageManager)
    	{
    		data.SetImageOperation( iImageManager->GetImageAsyncL(NULL,*aContact,*iThumbNailFieldType,(*this) ) );
    	}
        }
    else
        {
        ChangeIndexDataL(aIndex, data);
        iGrid->DrawNow();
        }
    return ETrue;
    }
   

// ---------------------------------------------------------
// CSpdiaContainer::CreateIndexIconL
//
// ---------------------------------------------------------
//
TBool CSpeedDialPrivate::CreateIndexIconL(TInt aIndex,
                 TSpdiaIndexDataVPbk& aSdmData, TBool aSetUpdateThumb)
    {
    TBool existThumb(EFalse);

  	if (aSdmData.Contact() != NULL)
    {
	    TInt fieldId = -1;	   
	    
	    MVPbkStoreContactField* field = (*iSdmArray)[aIndex].Field();	
		if(field)
		{
			fieldId = field->BestMatchingFieldType()->FieldTypeResId();
		}
	    aSdmData.SetIconIndex(FindIconIndex(fieldId, field ));     
	   
		// check for the functionality, why he is checking has thumbnail before adding
		
		if (iImageManager->HasImage(*aSdmData.Contact(),*iThumbNailFieldType))
		{
			existThumb = ETrue;
	        if (aSetUpdateThumb)
	        {
	            aSdmData.SetLastThumb(ETrue);
	        }
	        aSdmData.SetThumbIndex(aIndex, NULL);
	    }
	    else
	    {
	    	aSdmData.SetThumbIndex(-1, NULL);
	    }
	    
    }
         
    return existThumb;
    }

// ---------------------------------------------------------
// CSpdiaContainer::ChangeIndexDataL
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::ChangeIndexDataL(
    TInt aIndex,
    const TSpdiaIndexDataVPbk& aSdmData )
    {
    CDesCArray* array = STATIC_CAST(CDesCArray*,
                        iGrid->Model()->ItemTextArray());
    HBufC* des = ItemDescriptorLC(aIndex, aSdmData);
    array->Delete(aIndex);
    array->InsertL(aIndex, *des);
    CleanupStack::PopAndDestroy(); // des
    }
    
    
// ---------------------------------------------------------
// CSpeedDialPrivate::FindIconIndex
//
// ---------------------------------------------------------
//

TInt CSpeedDialPrivate::FindIconIndex( TInt aId, MVPbkStoreContactField* aField ) //const
    {
    TInt index(KNullIndexData);
    
	// If impp field, find xSP icon info from helper
    if ( aField && ( aId == R_VPBK_FIELD_TYPE_IMPP ) )
    	{
    	TPtrC scheme = MVPbkContactFieldUriData::Cast( aField->FieldData()).Scheme();
    	           	
    	const TInt count = ixspIconInfoArray.Count(); 
    	for ( TInt i=0; i < count; i++ )
    		{
    		if ( !scheme.CompareF( ixspIconInfoArray[i].LableText() ) )
    			{
    			index = ixspIconInfoArray[i].IconIndex();
    			break;
    			}
    		}
    	}
    // If not impp field, calculate icon index as below
    else
    	{
        for (TInt n(0); index == KNullIndexData && n < (sizeof KFieldIds/sizeof KFieldIds[0]); ++n)
            {
            if ((TUint)aId == KFieldIds[n] )
               {
               index = n + iSdmCount;
               }
            }
    	}
    return index;
    }
    

// ---------------------------------------------------------
// CSpdiaContainer::ItemDescriptorL
//
// ---------------------------------------------------------
//
HBufC* CSpeedDialPrivate::ItemDescriptorLC(TInt aIndex,
                            const TSpdiaIndexDataVPbk& aSdmData)
    {
    HBufC* des = NULL;
    if (aSdmData.Contact() != NULL)
        {
        des = CreateItemDescriptorLC(aSdmData);
        }
    else
        {
        des = HBufC::NewLC(KNullCell.iTypeLength);
        des->Des().Format(KNullCell, aIndex);
        }
    return des;
    }
    
// ---------------------------------------------------------
// CSpdiaContainer::SetItemDataL
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::SetItemDataL(CDesCArray& aArray)
    {
    for (TInt loop(0); loop < iSdmCount; loop++)
        {
        TSpdiaIndexDataVPbk& data = (*iSdmArray)[loop];

        aArray.AppendL(ItemDescriptorLC(loop, data)->Des());
        CleanupStack::PopAndDestroy(); // des
        }
    }
    
TBool CSpeedDialPrivate::CheckSpaceBelowCriticalLevelL()
    {
    TBool retcode(EFalse);

    if (SysUtil::FFSSpaceBelowCriticalLevelL(&(iCoeEnv->FsSession())))
        {
        CErrorUI* errorUi = CErrorUI::NewLC();
        errorUi->ShowGlobalErrorNoteL(KErrDiskFull);
        CleanupStack::PopAndDestroy(); // errorUi
        retcode = ETrue;
        }
    return retcode;
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::GetThumbnailSize()
// Gets actual size of thumbnial to be drawn
// ---------------------------------------------------------
//
TSize CSpeedDialPrivate::GetThumbnailSize()
{
	TRect mainPaneRect ;
    TRect statusPaneRect;
    TInt  varient =0;

	if (Layout_Meta_Data::IsLandscapeOrientation())
	{
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EMainPane,mainPaneRect);
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EStatusPane,statusPaneRect);
	 mainPaneRect.iTl = statusPaneRect.iTl;
	 mainPaneRect.iBr.iY = mainPaneRect.iBr.iY - statusPaneRect.iTl.iY;
	 varient =1;
	}
	else
	{
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EMainPane,mainPaneRect);
	 AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EStatusPane,statusPaneRect);
	 mainPaneRect.iTl= statusPaneRect.iTl;
	}
		
	TAknLayoutRect mainGridPaneRect;
	TAknLayoutRect gridPaneRect;
	TAknLayoutRect cellPaneRect;
	TAknLayoutRect thumbPaneRect;
	
	mainGridPaneRect.LayoutRect(mainPaneRect,AknLayoutScalable_Apps::main_qdial_pane());//main grid pane
	gridPaneRect.LayoutRect(mainGridPaneRect.Rect(),AknLayoutScalable_Apps::grid_qdial_pane());//main grid pane
	cellPaneRect.LayoutRect(gridPaneRect.Rect(),AknLayoutScalable_Apps::cell_qdial_pane(0,0));
	thumbPaneRect.LayoutRect(cellPaneRect.Rect(),AknLayoutScalable_Apps::thumbnail_qdial_pane(varient));

	return thumbPaneRect.Rect().Size();

}
	

// ---------------------------------------------------------
// CSpeedDialPrivate::ThumbType
// An interface for accessing index data array.
// ---------------------------------------------------------
//
TInt CSpeedDialPrivate::ThumbType(TInt aIndex) const
    {
    return ConvThumbType((*iSdmArray)[aIndex].ThumbSize());
    }
    
// ---------------------------------------------------------
// CSpeedDialPrivate::ThumbSize
// An interface for accessing index data array.
// ---------------------------------------------------------
//
TSize CSpeedDialPrivate::ThumbSize(TInt aIndex) const
    {
    return (*iSdmArray)[aIndex].ThumbSize();
    }
    
// ---------------------------------------------------------
// CSpdiaContainer::ConvThumbType
// Returns
//      EtnVGA  : 640 x 480 40 x 30 VGA
//      EtnVGA90: 480 x 640 30 x 40 VGA turned
//      EtnCIF  : 352 x 288 44 x 36 CIF
//      EtnCIF90: 288 x 352 36 x 44 CIF turned
//      EtnCOM  : othre
// ---------------------------------------------------------
//
TInt CSpeedDialPrivate::ConvThumbType(const TSize& aSize) const
    {
	// Scalable UI
	TAknWindowLineLayout  vga;
	TAknWindowLineLayout  cif;
	if( AknLayoutUtils::ScalableLayoutInterfaceAvailable() )
	{
	 vga = AknLayoutScalable_Apps::popup_phob_thumbnail_window_g2().LayoutLine();
	 cif = AknLayoutScalable_Apps::popup_phob_thumbnail_window_g1().LayoutLine();//image

	}
	else
	{
	 vga = AppLayout::Thumbnail_pop_up_window_elements_Line_1();
	 cif = AppLayout::Thumbnail_pop_up_window_elements_Line_3();

	}
    TInt type(CSpdiaGridVPbk::EtnCOM);

    if (aSize.iWidth == cif.iH//SDM_TN_CIF_HEIGHT
            && aSize.iHeight == cif.iW)// SDM_TN_CIF_WIDTH)
        {
        type = CSpdiaGridVPbk::EtnCIF90;
        }
    else if (aSize.iWidth == cif.iW &&//SDM_TN_CIF_WIDTH &&
                    aSize.iHeight ==  cif.iH)//SDM_TN_CIF_HEIGHT)
        {
        type = CSpdiaGridVPbk::EtnCIF;
        }
    else if (aSize.iWidth == vga.iH &&//SDM_TN_VGA_HEIGHT &&
                    aSize.iHeight == vga.iW )//SDM_TN_VGA_WIDTH)
        {
        type = CSpdiaGridVPbk::EtnVGA90;
        }
    else if (aSize.iWidth == vga.iW &&//SDM_TN_VGA_WIDTH &&
                    aSize.iHeight == vga.iH)//SDM_TN_VGA_HEIGHT)
        {
        type = CSpdiaGridVPbk::EtnVGA;
        }

    return type;
    }

// ---------------------------------------------------------
// CSpdiaContainer::AppendThumbnail
// output \tThumbnail\ticon
// ---------------------------------------------------------
//
void CSpeedDialPrivate::AppendThumbnail(
    const TSpdiaIndexDataVPbk& aSdmData,
    TPtr& aText,
    TBool aFixedLocation )
    {
    if (!aFixedLocation)
        {
        aText.Append(KDesTab2);
        return;
        }

    aText.Append(KDesTab);
    if ( aSdmData.ThumbIndex() >= 0 && aSdmData.ThumbSize().iWidth > 0 )
        {
        aText.AppendNum(aSdmData.ThumbIndex());
        }

    aText.Append(KDesTab);
    if (aSdmData.IconIndex() >= 0)
        {
        aText.AppendNum(aSdmData.IconIndex());
        }
    }
	




// ---------------------------------------------------------
// CSpeedDialPrivate::AssignSpeedDial
//
// ---------------------------------------------------------
//

TBool CSpeedDialPrivate::AssignSpeedDialL(TInt aIndex)
{
	TBool aReturn (EFalse);
	
	MVPbkContactOperationBase* operation = NULL;

	iSpeedDial->SetIndex(aIndex+1);	
	
	iOperationComplete = EFalse;
	(*iSdmArray)[aIndex].Contact()->LockL(*this);
	
	while(!iOperationComplete)
	{
		Wait();
	}
	if(iError == KErrNone)
	{
		
		MVPbkStoreContactField* field = (*iSdmArray)[aIndex].Field();
		iOperationComplete = EFalse;
		
		if( field != NULL)
		{
			operation = iAttributeManager->SetFieldAttributeL( *field, *iSpeedDial, ( *this ) );
		}
	
		if(operation != NULL)
		{
			(*iSdmArray)[aIndex].SetOperation(operation,TSpdiaIndexDataVPbk::OPERATION_ASSIGN);
			while(!iOperationComplete)
			{
				Wait();
			}
		
		}
		if(iError == KErrNone)
		{
			aReturn = ETrue;
		}
	}

	return aReturn;		
}



// ---------------------------------------------------------
// CSpeedDialPrivate::RemoveSpeedDial
//
// ---------------------------------------------------------
//

TBool CSpeedDialPrivate::RemoveSpeedDialL(TInt aIndex)
{
	TBool aReturn (EFalse);
	
	if((*iSdmArray)[aIndex].Field() != NULL)
	{
		iError = KErrNone;
		
		iOperationComplete = EFalse;
		iSpeedDial->SetIndex(aIndex+1);
		(*iSdmArray)[aIndex].Contact()->LockL(*this);
			
		while(!iOperationComplete)
		{
				Wait();
		}
		
	if(iError == KErrNone)
		{
			iOperationComplete = EFalse;
			MVPbkContactOperationBase* operation = iAttributeManager->RemoveFieldAttributeL(
													*((*iSdmArray)[aIndex].Field()), *iSpeedDial, (*this));
			
			if(operation)
			{
				
				(*iSdmArray)[aIndex].SetOperation(operation,TSpdiaIndexDataVPbk::OPERATION_REMOVE);
			
				while(!iOperationComplete)
				{
					Wait();
				}

			}
			
			if(iError == KErrNone)
			{
				aReturn = ETrue;
			}
		}
	}

	return aReturn;
}

// ---------------------------------------------------------
// CSpeedDialPrivate::ContactTitleL
//
// ---------------------------------------------------------
//

EXPORT_C HBufC* CSpeedDialPrivate::ContactTitleL( MVPbkBaseContact* aContact, TBool aUnnamed )
{


	HBufC* title = NULL;
	
	if (aContact != NULL)
	{
		CPbk2SortOrderManager* sortOrderManager = CPbk2SortOrderManager::NewL( iContactManager->FieldTypes() );
       
	    MPbk2ContactNameFormatter* nameFormatter = Pbk2ContactNameFormatterFactory::CreateL(
	                        iContactManager->FieldTypes(), 
	                        *sortOrderManager );
        title = nameFormatter->GetContactTitleOrNullL( aContact->Fields(), KContactFormattingFlags );
	    
	    if (title == NULL && aUnnamed)
	    {
	    	title = nameFormatter->UnnamedText().AllocL();	
	    }
	    
	    delete sortOrderManager;
	    delete nameFormatter;
	}
  
    return title;
                        
}

// ---------------------------------------------------------
// CSpeedDialPrivate::Release
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::Release(  )
     {
     
    
     if( iWait->IsStarted() )
         {
         iWait->AsyncStop();
         }
     }
   
// ---------------------------------------------------------
// CSpeedDialPrivate::Wait
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::Wait()
{
   if( !(iWait->IsStarted()) ) 
    	iWait->Start();

}

// ---------------------------------------------------------
// CSpeedDialPrivate::WaitFetchMail
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::WaitFetchMail()
    {
    if ( !( iWaitFetchmail->IsStarted() ) )
        {
        iWaitFetchmail->Start();
        }
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::ReleaseFetchMail
//
// ---------------------------------------------------------
//
void CSpeedDialPrivate::ReleaseFetchMail()
    {
    if ( iWaitFetchmail->IsStarted() )
        {
        iWaitFetchmail->AsyncStop();
        }
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::Cancel
// To cancel the aiw event
// ---------------------------------------------------------
//
TBool CSpeedDialPrivate::Cancel()
{
	if(!iOperationComplete)
	{
		iOperationComplete = ETrue;
	}
	if(iQueryDialog)
	{
		delete iQueryDialog;
		iQueryDialog = NULL;
	}
    if ( iServiceHandler )
        {
        TRAP_IGNORE(
                {
                CAiwGenericParamList& inParamList =  iServiceHandler->InParamListL();
                CAiwGenericParamList& outParamList =  iServiceHandler->OutParamListL();
                iServiceHandler->ExecuteServiceCmdL(
                    KAiwCmdAssign, 
                    inParamList,
                    outParamList,
                    KAiwOptCancel, NULL );
                }
            )
        }
    
    Release();
    iCancelFlag = ETrue;
    return ETrue;
}

// ---------------------------------------------------------
// CSpeedDialPrivate::GetWait
// Monitor iWait
// ---------------------------------------------------------
//
EXPORT_C CActiveSchedulerWait* CSpeedDialPrivate::GetWait()
 {
  return iWait;	
 }

// ---------------------------------------------------------
// CSpeedDialPrivate::ReplaceNonGraphicCharacters
// To replace nongraphic characters (e.g.:- newline)
// ---------------------------------------------------------
//
void CSpeedDialPrivate::ReplaceNonGraphicCharacters( TDes& aText, TText aChar )
    {
    const TInt len = aText.Length();

    const TUint KPbkZWSCharacter = 0x200B;
    const TUint KPbkZWNJCharacter = 0x200C;

    // Zero Width Space character
    const TChar zwsChar( KPbkZWSCharacter );
    // Zero Width Non-Joiner character
    const TChar zwnjChar( KPbkZWNJCharacter );

    for ( TInt i=0; i < len; ++i )
        {
        if ( !TChar(aText[i]).IsGraph() &&
             ( (zwsChar != aText[i]) && (zwnjChar != aText[i]) ) )
            {
            // If non-graphic char is specified in ZWSCharacter,
            // it will not be replaced. Otherwise replace non-graphic
            // character with aChar.
             aText[i] = aChar;
	        }
        }

    aText.Trim();
}

// ---------------------------------------------------------
// CSpeedDialPrivate::SetRefreshObserver
// Set refresh observer.
// ---------------------------------------------------------
//
EXPORT_C  void CSpeedDialPrivate::SetRefreshObserver( MRefreshObserver* aObserver )
    {
    iRefreshObserver = aObserver;
    }

// ---------------------------------------------------------
// CSpeedDialPrivate::SetCurrentGridIndex
// set the currentindex
// ---------------------------------------------------------
//
EXPORT_C void CSpeedDialPrivate::SetCurrentGridIndex( TInt aCurrentGridIndex )
 	{
	iCurrentIndex = aCurrentGridIndex;
	}


// ---------------------------------------------------------
// CSpeedDialPrivate::SetGridDataAsNull
// Set iGrid and iIconArray as Null
// ---------------------------------------------------------
//
EXPORT_C void CSpeedDialPrivate::SetGridDataAsNull()
	{
	iGrid = NULL;
	iIconArray = NULL;
	}
// ---------------------------------------------------------
// CSpeedDialPrivate::SetCurrentGridIndex
// set the currentindex
// ---------------------------------------------------------
//
void CSpeedDialPrivate::ShowNoteL( TInt aResourceId )
    {
    HBufC* prompt = StringLoader::LoadLC( aResourceId );
    CAknInformationNote* dlg = new ( ELeave ) CAknInformationNote( EFalse );
    dlg->ExecuteLD( *prompt );
    CleanupStack::PopAndDestroy(); // prompt
    }
// End of the file

