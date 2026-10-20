/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// phkbd_layout.h -- OURS (Phorepoc): the phkbd pen-input layout (a full-screen portrait layout: the input context field
// on top, our key area at the bottom). A plug-in of the pen input server (ECom interface 0x102072FC), picked by the
// input-method registry (phimreg) like any other keyboard.
#ifndef PHKBD_LAYOUT_H
#define PHKBD_LAYOUT_H

#include <touchfeedback.h>
#include <peninputlayoutplugininterface.h>
#include <peninputlayout.h>

class CPhKbdBoard;
class CFepLayoutMultiLineIcf;

const TUint32 KPhKbdLayoutImplUid = 0xE7015803;      // layout implementation UID (phkbd.rss)

// the ECom side: the pen server asks for this, we hand it the layout
class CPhKbdLayoutImp : public CFepUiLayoutInterface
    {
public:
    static CPhKbdLayoutImp* NewL( TAny* aInitParams );
    MPenUiLayoutBase* CreateFepUiLayoutL( MLayoutOwner* aLayoutOwner, const TAny* aData );
    };

class CPhKbdLayout : public CFepUiLayout
    {
public:
    static CPhKbdLayout* NewL( MLayoutOwner* aLayoutOwner, const TAny* aInitData );
    ~CPhKbdLayout();

    // MPenUiLayoutBase
    TInt HandleCommand( TInt aCmd, TUint8* aData );
    TInt SizeChanged( const TAny* aData );
    TInt OnAppEditorTextComing( const TFepInputContextFieldData& aData );
    TInt PenInputType();
    void OnActivate();
    // MEventObserver
    void HandleControlEvent( TInt aEventType, CFepUiBaseCtrl* aCtrl, const TDesC& aEventData );

private:
    explicit CPhKbdLayout( MLayoutOwner* aLayoutOwner );
    void ConstructL();
    void ReloadSettings();
    void Arrange();
    void SendKeys( const TDesC& aText );
    void SendCase( TInt aPenCase );
    void SwitchKeyboard();

private:
    CPhKbdBoard* iBoard;                    // owned by the layout's root control
    CFepLayoutMultiLineIcf* iIcf;           // owned by the layout's root control
    TInt iKeyHeightPct;
    TInt iTheme;
    TBool iPopups;
    };

#endif
