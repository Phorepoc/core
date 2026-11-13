/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// pheorientssy.cpp -- OURS: an orientation sensor (SSY plug-in of the sensor framework) for boards without an
// accelerometer. Orbit (Hb) turns its UI only from the sensor framework's orientation channel (HbSensorListener:
// KSensrvChannelTypeIdOrientationData); the release's orientationssy derives that channel from an accelerometer
// channel no board here has, so it registered the channel and failed every open (-1), and Hb stayed in its default
// orientation whatever the screen showed (Round 25: Orbit laid out portrait on the PC's landscape screen).
//
// This SSY serves the same channel from the user's choice (Control Panel "Screen orientation", the developer-menu
// repository key 13, pheorient.h) -- what the phone's sensor would report for the way the screen is turned:
//   Portrait  -> EOrientationDisplayUp       (Hb: Qt::Vertical)
//   Landscape -> EOrientationDisplayRightUp  (Hb: Qt::Horizontal)
//   Automatic -> the shape of the window server's current screen mode (the board's hardware state decides it)
// A change of the key is reported at once (a CenRep notification). A board with a real orientation sensor drops this
// component and builds that sensor's SSY instead.

#include <e32base.h>
#include <ecom/implementationproxy.h>
#include <centralrepository.h>
#include <w32std.h>
#include <ssycontrol.h>
#include <ssycallback.h>
#include <ssychanneldataprovider.h>
#include <ssypropertyprovider.h>
#include <sensrvchannelinfo.h>
#include <sensrvorientationsensor.h>
#include <sensrvgeneralproperties.h>
#include "../phedevmenu/phedevmenu.h"

const TUint32 KPheDevKeyOrientation = 13;		// pheorient.h: 0 Automatic | 1 Portrait | 2 Landscape
const TInt KPheOrientPortrait = 1, KPheOrientLandscape = 2;
_LIT8(KPheLocation, "NoLoc");
_LIT8(KPheVendor, "Phorepoc");

// the window server's current screen mode, wider than tall?
static TBool CurrentModeLandscape()
	{
	RWsSession ws;
	if (ws.Connect() != KErrNone)
		return EFalse;	// no window server to ask: the natural (portrait) shape
	TBool land = EFalse;
	CWsScreenDevice* dev = new CWsScreenDevice(ws);
	if (dev && dev->Construct() == KErrNone)
		{
		TPixelsTwipsAndRotation m;
		dev->GetScreenModeSizeAndRotation(dev->CurrentScreenMode(), m);
		land = m.iPixelSize.iWidth > m.iPixelSize.iHeight;
		}
	delete dev;
	ws.Close();
	return land;
	}

class CPheOrientSsy : public CSsyControl, public MSsyChannelDataProvider, public MSsyPropertyProvider
	{
public:
	static CPheOrientSsy* NewL(TAny* aCallback);
	~CPheOrientSsy();
	// CSsyControl
	void OpenChannelL(TSensrvChannelId aChannelId);
	void CloseChannelL(TSensrvChannelId aChannelId);
	// MSsyChannelDataProvider
	void StartChannelDataL(const TSensrvChannelId aChannelId, TUint8* aBuffer, TInt aCount);
	void StopChannelDataL(const TSensrvChannelId aChannelId);
	void ForceBufferFilledL(const TSensrvChannelId aChannelId);
	void GetChannelDataProviderInterfaceL(TUid aInterfaceUid, TAny*& aInterface);
	// MSsyPropertyProvider
	void CheckPropertyDependenciesL(const TSensrvChannelId aChannelId, const TSensrvProperty& aProperty,
	                                RSensrvChannelList& aAffectedChannels);
	void SetPropertyL(const TSensrvChannelId aChannelId, const TSensrvProperty& aProperty);
	void GetPropertyL(const TSensrvChannelId aChannelId, TSensrvProperty& aProperty);
	void GetAllPropertiesL(const TSensrvChannelId aChannelId, RSensrvPropertyList& aChannelPropertyList);
	void GetPropertyProviderInterfaceL(TUid aInterfaceUid, TAny*& aInterface);
private:
	// the callbacks to the sensor server run from an active object, never inside the server's own call (as the
	// release's SSYs do): one for "opened", one for "a data item is ready", one for the repository's notification
	class CStep : public CActive
		{
	public:
		CStep(CPheOrientSsy& aSsy, TInt aKind) : CActive(EPriorityStandard), iSsy(aSsy), iKind(aKind)
			{ CActiveScheduler::Add(this); }
		~CStep() { Cancel(); }
		void Kick() { if (!IsActive()) { iStatus = KRequestPending; SetActive(); TRequestStatus* s = &iStatus; User::RequestComplete(s, KErrNone); } }
		void Wait() { SetActive(); }
		TRequestStatus& Status() { return iStatus; }
	private:
		void RunL() { iSsy.Step(iKind, iStatus.Int()); }
		void DoCancel();
		CPheOrientSsy& iSsy;
		TInt iKind;
		};
	enum { EOpened, EFill, ERepo };
	CPheOrientSsy(MSsyCallback& aCallback) : iCallback(aCallback) {}
	void ConstructL();
	void Step(TInt aKind, TInt aStatus);
	void WatchRepo();
	TSensrvOrientationData::TSensrvDeviceOrientation Current() const;
	void PropertyFor(TSensrvPropertyId aId, TSensrvProperty& aProperty) const;

	MSsyCallback& iCallback;
	TSensrvChannelId iChannelId;
	CRepository* iRepo;
	CStep* iOpened;
	CStep* iFill;
	CStep* iRepoWatch;
	TUint8* iBuffer;		// the server's buffer while data listening is on
	TInt iCount;
	TBool iSent;			// the current orientation was reported on this listening
	TSensrvOrientationData::TSensrvDeviceOrientation iLast;
	friend class CStep;
	};

void CPheOrientSsy::CStep::DoCancel()
	{
	if (iKind == ERepo && iSsy.iRepo)
		iSsy.iRepo->NotifyCancel(KPheDevKeyOrientation);
	else if (iStatus == KRequestPending)
		{
		TRequestStatus* s = &iStatus;
		User::RequestComplete(s, KErrCancel);
		}
	}

CPheOrientSsy* CPheOrientSsy::NewL(TAny* aCallback)
	{
	CPheOrientSsy* self = new (ELeave) CPheOrientSsy(*static_cast<MSsyCallback*>(aCallback));
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CPheOrientSsy::ConstructL()
	{
	TRAP_IGNORE(iRepo = CRepository::NewL(KPheDevMenuRepoUid));	// none = Automatic
	iOpened = new (ELeave) CStep(*this, EOpened);
	iFill = new (ELeave) CStep(*this, EFill);
	iRepoWatch = new (ELeave) CStep(*this, ERepo);
	iLast = Current();
	RSensrvChannelInfoList list;
	CleanupClosePushL(list);
	TSensrvChannelInfo info(0, ESensrvContextTypeUser, ESensrvQuantityOrientation, KSensrvChannelTypeIdOrientationData,
	                        KPheLocation(), KPheVendor(), sizeof(TSensrvOrientationData),
	                        TSensrvOrientationData::KDataTypeId);
	list.AppendL(info);
	iCallback.RegisterChannelsL(list);
	iChannelId = list[0].iChannelId;
	CleanupStack::PopAndDestroy(&list);
	WatchRepo();
	RDebug::Printf("PH: [orientssy] channel %d registered, orientation %d", iChannelId, iLast);
	}

CPheOrientSsy::~CPheOrientSsy()
	{
	delete iRepoWatch;
	delete iFill;
	delete iOpened;
	delete iRepo;
	}

void CPheOrientSsy::WatchRepo()
	{
	if (iRepo && !iRepoWatch->IsActive() && iRepo->NotifyRequest(KPheDevKeyOrientation, iRepoWatch->Status()) == KErrNone)
		iRepoWatch->Wait();
	}

TSensrvOrientationData::TSensrvDeviceOrientation CPheOrientSsy::Current() const
	{
	TInt v = 0;
	if (!iRepo || iRepo->Get(KPheDevKeyOrientation, v) != KErrNone)
		v = 0;
	TBool land = v == KPheOrientLandscape ? ETrue : v == KPheOrientPortrait ? EFalse : CurrentModeLandscape();
	return land ? TSensrvOrientationData::EOrientationDisplayRightUp : TSensrvOrientationData::EOrientationDisplayUp;
	}

void CPheOrientSsy::Step(TInt aKind, TInt aStatus)
	{
	switch (aKind)
		{
	case EOpened:
		iCallback.ChannelOpened(iChannelId, KErrNone, this, this);
		break;
	case EFill:
		if (iBuffer && !iSent)
			{
			TSensrvOrientationData d;
			d.iTimeStamp.UniversalTime();
			d.iDeviceOrientation = iLast;
			Mem::Copy(iBuffer, &d, sizeof(d));
			iSent = ETrue;
			TUint8* next = iBuffer;
			TInt n = iCount;
			iCallback.BufferFilled(iChannelId, 1, next, n);
			iBuffer = next;
			iCount = n;
			}
		break;
	case ERepo:
		if (aStatus >= KErrNone || aStatus == KErrNotFound)
			{
			TSensrvOrientationData::TSensrvDeviceOrientation now = Current();
			if (now != iLast)
				{
				iLast = now;
				iSent = EFalse;
				RDebug::Printf("PH: [orientssy] orientation %d", iLast);
				iFill->Kick();
				}
			}
		WatchRepo();
		break;
		}
	}

void CPheOrientSsy::OpenChannelL(TSensrvChannelId aChannelId)
	{
	if (aChannelId != iChannelId)
		User::Leave(KErrNotFound);
	iOpened->Kick();
	}

void CPheOrientSsy::CloseChannelL(TSensrvChannelId aChannelId)
	{
	if (aChannelId != iChannelId)
		User::Leave(KErrNotFound);
	iBuffer = NULL;
	iCallback.ChannelClosed(iChannelId);
	}

void CPheOrientSsy::StartChannelDataL(const TSensrvChannelId aChannelId, TUint8* aBuffer, TInt aCount)
	{
	if (aChannelId != iChannelId)
		User::Leave(KErrNotFound);
	iBuffer = aBuffer;
	iCount = aCount;
	iLast = Current();
	iSent = EFalse;		// a new listener gets the current orientation first
	iFill->Kick();
	}

void CPheOrientSsy::StopChannelDataL(const TSensrvChannelId aChannelId)
	{
	if (aChannelId != iChannelId)
		User::Leave(KErrNotFound);
	iBuffer = NULL;
	}

void CPheOrientSsy::ForceBufferFilledL(const TSensrvChannelId aChannelId)
	{
	if (aChannelId != iChannelId)
		User::Leave(KErrNotFound);
	if (iBuffer)
		{
		TUint8* next = iBuffer;
		TInt n = iCount;
		iCallback.BufferFilled(iChannelId, 0, next, n);
		iBuffer = next;
		iCount = n;
		}
	}

void CPheOrientSsy::GetChannelDataProviderInterfaceL(TUid, TAny*& aInterface)
	{
	aInterface = NULL;
	}

// the properties the release's orientation SSY offers: availability (always) and a read-only data rate
void CPheOrientSsy::PropertyFor(TSensrvPropertyId aId, TSensrvProperty& aProperty) const
	{
	if (aId == KSensrvPropIdAvailability)
		aProperty = TSensrvProperty(KSensrvPropIdAvailability, KSensrvItemIndexNone, 1, 1, 0, ETrue, ESensrvIntProperty);
	else
		aProperty = TSensrvProperty(KSensrvPropIdDataRate, KSensrvItemIndexNone, 0, 0, 0, ETrue, ESensrvIntProperty);
	}

void CPheOrientSsy::CheckPropertyDependenciesL(const TSensrvChannelId, const TSensrvProperty&, RSensrvChannelList&)
	{
	}

void CPheOrientSsy::SetPropertyL(const TSensrvChannelId, const TSensrvProperty&)
	{
	User::Leave(KErrAccessDenied);	// both properties are read-only
	}

void CPheOrientSsy::GetPropertyL(const TSensrvChannelId aChannelId, TSensrvProperty& aProperty)
	{
	const TSensrvPropertyId id = aProperty.GetPropertyId();
	if (aChannelId != iChannelId || (id != KSensrvPropIdAvailability && id != KSensrvPropIdDataRate))
		User::Leave(KErrNotFound);
	PropertyFor(id, aProperty);
	}

void CPheOrientSsy::GetAllPropertiesL(const TSensrvChannelId aChannelId, RSensrvPropertyList& aChannelPropertyList)
	{
	if (aChannelId != iChannelId)
		User::Leave(KErrNotFound);
	TSensrvProperty p;
	PropertyFor(KSensrvPropIdAvailability, p);
	aChannelPropertyList.AppendL(p);
	PropertyFor(KSensrvPropIdDataRate, p);
	aChannelPropertyList.AppendL(p);
	}

void CPheOrientSsy::GetPropertyProviderInterfaceL(TUid, TAny*& aInterface)
	{
	aInterface = NULL;
	}

static const TImplementationProxy KTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(0xE7015A21, CPheOrientSsy::NewL)
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KTable) / sizeof(TImplementationProxy);
	return KTable;
	}
