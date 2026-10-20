// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// hbcore_null.cpp -- OURS (not Symbian source): a null Orbit "device dialog" client with the REAL
// HbCoreu.def ordinal table.
//
// This snapshot's classicui is part-way through the Symbian^4 migration: eiksrvui
// (EIKSRVUI.CPP ShowNotificationDialog) reports a dead application through Orbit's
// CHbDeviceDialogSymbian instead of an Avkon note. There is no Orbit here, and as loud unimpl stubs these calls turned
// ANY application crash into an eiksrvs panic and then a kernel fault. Null behaviour: the
// dialog object exists and Show() reports KErrNotSupported, so the "app closed" note is simply
// not shown. Everything else in HbCore stays a loud stub.
#include <e32base.h>
#include <hbdevicedialogsymbian.h>
#include <hbsymbianvariant.h>

EXPORT_C CHbDeviceDialogSymbian* CHbDeviceDialogSymbian::NewL(TInt)
	{
	return new (ELeave) CHbDeviceDialogSymbian;
	}

CHbDeviceDialogSymbian::CHbDeviceDialogSymbian()
	{
	}

EXPORT_C CHbDeviceDialogSymbian::~CHbDeviceDialogSymbian()
	{
	}

EXPORT_C TInt CHbDeviceDialogSymbian::Show(const TDesC&, const CHbSymbianVariantMap&, MHbDeviceDialogObserver*)
	{
	return KErrNotSupported;
	}

EXPORT_C TInt CHbDeviceDialogSymbian::Update(const CHbSymbianVariantMap&)
	{
	return KErrNotSupported;
	}

EXPORT_C void CHbDeviceDialogSymbian::Cancel()
	{
	}

EXPORT_C void CHbDeviceDialogSymbian::SetObserver(MHbDeviceDialogObserver*)
	{
	}

EXPORT_C CHbSymbianVariant* CHbSymbianVariant::NewL(const TAny* aData, TType aDataType)
	{
	return new (ELeave) CHbSymbianVariant(aData, aDataType);
	}

CHbSymbianVariant::CHbSymbianVariant(const TAny*, TType)
	{
	}

EXPORT_C CHbSymbianVariant::~CHbSymbianVariant()
	{
	}

EXPORT_C CHbSymbianVariantMap* CHbSymbianVariantMap::NewL()
	{
	return new (ELeave) CHbSymbianVariantMap;
	}

CHbSymbianVariantMap::CHbSymbianVariantMap()
	{
	}

EXPORT_C CHbSymbianVariantMap::~CHbSymbianVariantMap()
	{
	}

EXPORT_C TInt CHbSymbianVariantMap::Add(const TDesC&, CHbSymbianVariant* aVariant)
	{
	delete aVariant;	// the map takes ownership
	return KErrNone;
	}
