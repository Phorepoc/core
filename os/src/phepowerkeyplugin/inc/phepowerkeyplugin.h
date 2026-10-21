/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// phepowerkeyplugin.h -- OURS: the Power key Control Panel page (view)
#ifndef PHEPOWERKEYPLUGIN_H
#define PHEPOWERKEYPLUGIN_H

#include <ConeResLoader.h>
#include <gsplugininterface.h>
#include <gsfwviewuids.h>
#include <gsbaseview.h>

class CRepository;
class CPhePowerKeyPluginContainer;

// the ECOM implementation UID doubles as the view id (the parent folder is KGSGenPluginUid)
const TUid KPhePowerKeyPluginUid = { 0xE70157B2 };

// SysAp's power key repository (src/phorepoc-arm/board/cenrep/e70157b0.txt)
const TUid  KPhePowerKeyCRUid = { 0xE70157B0 };
const TUint32 KPhePowerKeyShort = 1;
const TUint32 KPhePowerKeyLong  = 2;
const TInt  KPhePowerKeyActions = 3;     // 0 menu, 1 lock / unlock, 2 switch off

class CPhePowerKeyPlugin : public CGSBaseView
    {
    public:
        static CPhePowerKeyPlugin* NewL( TAny* aInitParams );
        CPhePowerKeyPlugin();
        void ConstructL();
        ~CPhePowerKeyPlugin();

    public: // CAknView / MEikMenuObserver
        TUid Id() const;
        void HandleCommandL( TInt aCommand );

    public: // CGSPluginInterface
        void GetCaptionL( TDes& aCaption ) const;
        TInt PluginProviderCategory() const;

    public: // the repository
        /** The current action (0..2) of a key; the default on a read error. */
        TInt ActionL( TUint32 aKey ) const;

    private: // CGSBaseView
        void NewContainerL();
        void HandleListBoxSelectionL();

    private:
        CPhePowerKeyPluginContainer* Container();
        void ChangeL( TInt aItemId );

    private:
        CRepository* iRepository;
        RConeResourceLoader iResourceLoader;
    };

#endif // PHEPOWERKEYPLUGIN_H
