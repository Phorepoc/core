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

#ifndef IMAGICAPP_H
#define IMAGICAPP_H

// INCLUDES
#include <aknapp.h>
#include "../group/UidList.txt"

// CONSTANTS
const TUid KUidImagic = { Imagic_UID3  }; //0xE1EF0018


// CLASS DECLARATION

/**
* CImagicApp application class.
* Provides factory to create concrete document object.
* 
*/
class CImagicApp : public CAknApplication
    {
    
    private:

        /**
        * From CApaApplication, creates CImagicDocument document object.
        * @return A pointer to the created document object.
        */
        CApaDocument* CreateDocumentL();
        
        /**
        * From CApaApplication, returns application's UID (KUidImagic).
        * @return The value of KUidImagic.
        */
        TUid AppDllUid() const;

        // PHOREPOC PATCH: see ImagicApp.cpp (a second launch must not end before its document name is known)
        void PreDocConstructL();
    };

#endif

// End of File

