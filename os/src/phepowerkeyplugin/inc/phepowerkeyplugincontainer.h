/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phepowerkeyplugincontainer.h -- OURS: the list container of the Power key page
#ifndef PHEPOWERKEYPLUGINCONTAINER_H
#define PHEPOWERKEYPLUGINCONTAINER_H

#include <gsbasecontainer.h>
#include <badesca.h>

class CGSListBoxItemTextArray;
class CPhePowerKeyPlugin;

class CPhePowerKeyPluginContainer : public CGSBaseContainer
    {
    public:
        CPhePowerKeyPluginContainer( CPhePowerKeyPlugin& aView );
        ~CPhePowerKeyPluginContainer();
        void ConstructL( const TRect& aRect );

        /** Re-reads both values from the repository into the list. */
        void UpdateListBoxL();
        /** The feature id of the selected item (EPhePkShortPress / EPhePkLongPress). */
        TInt CurrentFeatureId() const;

    protected: // CGSBaseContainer
        void ConstructListBoxL( TInt aResLbxId );

    private:
        void SetItemL( TInt aItemId, TUint32 aKey );

    private:
        CPhePowerKeyPlugin& iView;
        CGSListBoxItemTextArray* iListboxItemArray;   // owned
        CDesCArrayFlat* iValueNames;                  // owned
    };

#endif // PHEPOWERKEYPLUGINCONTAINER_H
