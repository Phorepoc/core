/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phgskbd.h -- OURS (Phorepoc): the Settings (GS) entry for the keyboard framework: choose the default keyboard among the
// installed ones (phimreg), and phkbd's customisation (key height, colour scheme, key preview). An ECom plug-in of the
// General Settings application (interface 0x10207236), shown in the "General" list.
#ifndef PHGSKBD_H
#define PHGSKBD_H

#include <gsbaseview.h>
#include <gsbasecontainer.h>
#include <gsfwviewuids.h>
#include <gsplugininterface.h>
#include <eikclb.h>
#include <aknlists.h>
#include <ConeResLoader.h>

const TUid KPhGsKbdPluginUid = { 0xE7015821 };      // = the ECom implementation UID = the GS view id
_LIT( KPhGsKbdResourceFile, "z:phgskbdrsc.rsc" );

class CPhGsKbdContainer : public CGSBaseContainer
    {
public:
    void ConstructL( const TRect& aRect );
    ~CPhGsKbdContainer();
    void RefreshItemsL();                           // re-read the registry / settings into the list texts
    TInt CurrentIndex() const;
protected:
    void ConstructListBoxL( TInt aResLbxId );
private:
    CDesCArrayFlat* iItems;                         // the list's text array (owned here)
    };

class CPhGsKbdPlugin : public CGSBaseView
    {
public:
    static CPhGsKbdPlugin* NewL( TAny* aInitParams );
    ~CPhGsKbdPlugin();

    // CAknView
    TUid Id() const;
    void HandleClientRectChange();
    void DoActivateL( const TVwsViewId& aPrevViewId, TUid aCustomMessageId, const TDesC8& aCustomMessage );
    void DoDeactivate();
    void HandleCommandL( TInt aCommand );
    // CGSPluginInterface
    void GetCaptionL( TDes& aCaption ) const;
    TInt PluginProviderCategory() const;
    // CGSBaseView
    void ConstructL();
    void NewContainerL();
    void HandleListBoxSelectionL();

private:
    CPhGsKbdPlugin();
    CPhGsKbdContainer* PhContainer() { return static_cast<CPhGsKbdContainer*>( iContainer ); }
    // a radio-button page over aItems; aIndex in/out; ETrue when the user accepted
    TBool ChooseL( CDesCArray* aItems, TInt& aIndex );
    void ChooseKeyboardL();
    void ChooseKeyHeightL();
    void ChooseThemeL();
    void ChoosePopupsL();
    };

#endif
