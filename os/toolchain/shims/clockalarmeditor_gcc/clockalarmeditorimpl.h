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
*
* Description:   This is the header file for the CClockAlarmEditor class.
*
*/

#ifndef __CLOCK_ALARMEDITORIMPL_H__
#define __CLOCK_ALARMEDITORIMPL_H__

// System includes
#include <AknForm.h>
#include <eikedwob.h>
#include <eikcapc.h>
#include <asshdalarm.h>
#include <asclisession.h>

// User includes
#include "clkcommon.h"

// Forward declarations
class CAknView;
class MClkAlarmInfoObserver;

// Class declaration
/**
* @class CClockAlarmEditorImpl
* @brief The object of this class is used to accept the alarm information from the user.
* @dll clockalarmeditor.dll
*/
class CClockAlarmEditorImpl : public CAknForm,
                              public MEikEdwinObserver
	{
    public:             // Constructor and destructor
        
		/**
		* @brief 2-phased constructor.
		* @param aAlarmId The id of the alarm the client wishes to edit.
		* @return CClockAlarmEditor* Pointer to the constructed CClockAlarmEditor object.
		*/
		static CClockAlarmEditorImpl* NewL( TAlarmId& aAlarmId );
		
		/**
		* @brief Destructor
		*/
		~CClockAlarmEditorImpl();
		
	public:             // New functions
		
		/**
		* @brief Gets the previous alarm information from the document class and populates 
		*        the form before displaying it.
		* @param aAlarmInfo AlarmInfo structure.
		* @param aEditAlarm ETrue if the alarm is being reset.
		*/
        void LoadAlarmInformationL( SClkAlarmInfo& aAlarmInfo, TBool aEditAlarm );
		
		/**
		* @brief Calls the form's ExecuteLD to launch the editor.
		* @return TInt the newly set alarm ID, or 0 in case user has cancelled.
		*/
        TInt ExecuteLD();
        
    protected:          // Functions from base classes
        
		/**
		* @brief From CAknForm. Sets the initial current line in the form.
		*        This is implemented so that fields can removed dynamically.
		* @see CAknForm      
		*/
		void SetInitialCurrentLineL();
		
		/**
		* @brief From CAknForm. Saves the data in the form.
		* @return TBool ETrue if save successful.
		*/
		TBool SaveFormDataL();
		
		/**
		* @brief From CEikDialog. Checks if is ok to exit the form.
		* @param aButtonId The ID of the button that was activated.
		* @return ETrue to validate and exit the dialog,
		*         EFalse to keep the dialog active.
		* @see CEikDialog.
		*/
		TBool OkToExitL( TInt aButtonId );

		/**
		* @brief From CEikDialog. Pre-layout dialog initialisation.
		* @see CEikDialog.
		*/
		void PreLayoutDynInitL();
		
		/**
		* @brief From CEikDialog. Post-layout dialog initialisation.
		* @see CEikDialog.
		*/
		void PostLayoutDynInitL();
		
		/**
		* @brief From CEikDialog. Command handling.	
		* @param aCommandId The ID of the command that was activated.
		* @see CEikDialog.
		*/
		void ProcessCommandL( TInt aCommandId );
		
		/**
		* @brief From MEikMenuObserver. Dynamically initializes the menu items.
		* @param aResourceId The resource which triggered the action.
		* @param aMenuPane Pointer to the menupane of type CEikMenuPane.
		*/
		void DynInitMenuPaneL( TInt aResourceId, CEikMenuPane* aMenuPane );
        
    private:            // New functions
        
		/**
		* @brief Performs the 2nd phase of the 2-phase construction.
		*/
        void ConstructL();
        
		/**
		* @brief C++ default constructor.
		* @param aAlarmId The id of the alarm.
		*/
		CClockAlarmEditorImpl( TAlarmId& aAlarmId );
		
		/**
		* @brief Handles the occuracnce item command.
		*/
		void HandleOccuranceCmdL();
				
		/**
		* @brief Handles the alarm day item command.
		*/
		void HandleAlarmDayCmdL();
		
		/**
		* @brief Gets the index of the selected occurance item.
		*/
		void GetSelectedOccIndex();
                
		/**
		* @brief Gets the index of selected alarm day item.
		*/
		void GetSelectedAlmDayIndex();
				
		/**
		* @brief Creates the alarm day selection control dynamically.
		*/
        void CreateAlmDayCtrlL();
                
		/**
		* @brief Deletes the alarm day selection control if it exists.
		*/
        void DeleteAlmDayCtrlL();
                
		
        
		/**
		* @brief Sets the title text.
		* @param aNonDefault If EFalse then the default text will be displayed.
		*/
        void SetTitleL( TBool aNonDefault );
        
		/**
		* @brief Connects to the alarm server and sets the alarm.
		*/
        void SetAlarmL();
        
		/**
		* @brief Checks if the alarm time has gone past the current time and updates the alarm time accordingly.
		* @param aHomeTime The current time.
		* @param aTimeFromForm The time got from the form.
		*/
        void GetActualAlarmTime( const TTime& aHomeTime, TTime& aTimeFromForm );
        
		/**
		* @brief Tries to get the alarm info from alarm server corresponding 
		*        to aExistingAlarmId.
		* @param aExistingAlarmId The existing alarm Id.
		* @param aAlarmInfo Filled up with the alarm details if alarm exists.
		* @return KErrNone if alarm exists, KErrNotFound otherwise.
		*/
        TInt GetAlarmInformationL( TAlarmId aExistingAlarmId, SClkAlarmInfo& aAlarmInfo );
        
		/**
		* @brief Gets the value of alarm time previously set from cenrep.
		* @param aAlarmInfo Filled up with the default alarm info details.
		*/
        void GetPreviousAlarmTimeL( SClkAlarmInfo& aAlarmInfo );
        
        /**
        * @brief Sets the previous alarm time value in cenrep
        * @param aTime The previous alarm time value. 
        */
        void SetPreviousAlarmTimeL( TTime& aTime );
        
        /**
        * @brief Checks if DST changes will be applied in the next 24 hours.
        * @return TBool ETrue if remaining time note has to be displayed.
        */
        TBool CheckForDstChangesL();
        
        /**
        * @brief Displays the remaining time for the alarm to expire.
        */
        void DisplayRemainingTimeL();
        
    private:            // Functions from base classes.
        
		/**
		* @brief From CEikEdwin. Handles the Navigation on the editor control.
		* @param aEdwin The editor in context.
		* @param aEventType The type of event to be handled. 
		*/
        void HandleEdwinEventL( CEikEdwin* aEdwin, TEdwinEvent aEventType );
                
        /**
		* @brief From CCoeControl. Handles key events.
		* @param aKeyEvent The key event.
		* @param aEventCode The event code.
		* @return TKeyResponse The key response.
		*/
        TKeyResponse OfferKeyEventL( const TKeyEvent& aKeyEvent,
                                     TEventCode aType );
                    
	
        
		/**
		* @brief From CCoeControl. Takes necessary changes when control changes from one line to other.
		* @param aControlId The new control line that is focused.
		*/
        void LineChangedL( TInt aControlId );

		//Single click integration
        /**
        * @brief From CEikDialog. Handles state change events from popup fields
        * @param aControlId The control that sent event EEventStateChanged 
        */
        void HandleControlStateChangeL( TInt aControlId );
		/**
        * @brief CurrentValueTextLC from base class        
        */
        virtual HBufC* CurrentValueTextLC(){ return NULL; };   // PHOREPOC-DELTA (GCC): no return in a non-void
        // function is undefined; GCC compiles the body as unreachable (X4.64 return census)
        
    private:            // Data
        
		/**
		* @var iObserver
		* @brief The observer of the alarm information.
		*/
        MClkAlarmInfoObserver*          iObserver;
        
		/**
		* @var iViewId;
		* @brief The id of the view.
		*/
        TUid                            iViewId;
        
		/**
		* @var iAlarmForEditing
		* @brief ETrue if alarm is opened for editing.
		*/
        TBool                           iAlarmForEditing;
        
		/**
		* @var iAlarmInfo
		* @brief The alarm information.
		*/
        SClkAlarmInfo                   iAlarmInfo;
       
        
		/**
		* @var iAlarmId
		* @brief The shared alarm object.
		*/
        TAlarmId&						iAlarmId;
        
		/**
		* @var iOccuranceIndex
		* @brief This indicates the repeat type of the alarm.
		*/
        TInt                            iOccuranceIndex;
        
		/**
		* @var iDayIndex
		* @brief This has the index of the day selected by the user.
		*/
        TInt                            iDayIndex;
        
		/**
		* @var iSelectedControl
		* @brief The index of the control that is selected.
		*/
        TInt                            iSelectedControl;
        
		/**
		* @var iShowAlarmDayControl
		* @brief This indicates wheather the alarm day popup item should be displayed or not.
		*/
        TBool                           iShowAlarmDayControl;
        

		/**
		* @var iOffset
		* @brief The offset of the resource loaded in the memory.
		*/
		TInt							iOffset;
		
		/**
		* @var iAlarmSrvSes
		* @brief Object of alarm server session.
		*/
		RASCliSession                   iAlarmSrvSes;
		/**
		 * @var iWeekDaysList
		 * @brief This holds the list of weekdays.
		 */
		CDesCArrayFlat*                 iWeekDaysList;
		/**
		 * @var iAlmDayFieldTextArray
		 * @brief This indicates the avkon query value text array which
		 * is used to set the array in the popup field control.
		 */
		CAknQueryValueTextArray*        iAlmDayFieldTextArray;
		/**
		 * @var iAlmDayFieldTextValue
		 * @brief This allows to control the values of popup field control.
		 */
		CAknQueryValueText*             iAlmDayFieldTextValue;
	};

#endif 			// __CLOCK_ALARMEDITORIMPL_H__

// End of file
