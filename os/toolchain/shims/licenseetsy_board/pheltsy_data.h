/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// pheltsy_data.h -- OURS (2026-11-21, mobile data on the main image): where the packet-data APN comes from.
//
// The APN is the S60 one: the packet-data connection method's EPacketDataAPName (CommsDat OutgoingGPRS GPRS_APN), the
// value Settings > Access points edits. The 2010 release has no built-in operator table: its operator settings arrive by
// OMA Client Provisioning (devicecontrol omacpadapters/accesspointadapter, a configuration message from the operator).
// So a phone without that message gets its first packet-data access point here, from a small table keyed by the SIM's
// home network (the IMSI's MCC + MNC); an access point the user (or provisioning) already has is never changed, except
// that an empty APN is filled from the table.
//
// The work runs in a thread of its own: cmmanager is a client of the comms framework, which may in turn ask ETel (this
// thread's own server) about packet support; the AT engine polls the result instead of blocking the TSY.
#ifndef PHELTSY_DATA_H
#define PHELTSY_DATA_H

#include <e32base.h>
#include <e32debug.h>
#include <cmmanager.h>
#include <cmconnectionmethod.h>
#include <cmconnectionmethoddef.h>
#include <cmpluginpacketdatadef.h>

struct TPheApnProv
	{
	TBuf8<8> iPlmn;							// in: the IMSI's first 5 digits (MCC + the first two MNC digits)
	TBuf8<64> iApn;							// out: the APN to use (empty = none known)
	TInt iErr;								// out
	};

// home network -> the operator's public internet APN (from the operators' published settings)
static const char* PheApnForPlmn(const TDesC8& aPlmn)
	{
	static const char* const KTable[][2] = {
		{ "20201", "internet" },			// Cosmote (GR)
		};
	for (TUint i = 0; i < sizeof(KTable) / sizeof(KTable[0]); ++i)
		if (aPlmn == TPtrC8((const TUint8*)KTable[i][0]))
			return KTable[i][1];
	return 0;
	}

static void PheApnProvisionL(TPheApnProv& aP)
	{
	using namespace CMManager;
	const char* def = PheApnForPlmn(aP.iPlmn);
	TBuf<64> defApn;
	if (def)
		defApn.Copy(TPtrC8((const TUint8*)def));
	RCmManager cm;
	cm.OpenL();
	CleanupClosePushL(cm);
	RArray<TUint32> ids;
	CleanupClosePushL(ids);
	cm.ConnectionMethodL(ids, ETrue, EFalse, EFalse);
	TUint32 found = 0;
	for (TInt i = 0; i < ids.Count() && !found; ++i)
		{
		TUint32 bearer = 0;
		TRAPD(e, bearer = cm.GetConnectionMethodInfoIntL(ids[i], ECmBearerType));
		if (e == KErrNone && bearer == KUidPacketDataBearerType)
			found = ids[i];
		}
	if (found)
		{
		HBufC* apn = cm.GetConnectionMethodInfoStringL(found, EPacketDataAPName);
		aP.iApn.Copy(apn->Left(aP.iApn.MaxLength()));
		const TBool empty = apn->Length() == 0;
		delete apn;
		RDebug::Printf("[pheltsy] data: packet-data access point %u, APN \"%S\"", found, &aP.iApn);
		if (empty && defApn.Length())
			{
			RCmConnectionMethod m = cm.ConnectionMethodL(found);
			CleanupClosePushL(m);
			m.SetStringAttributeL(EPacketDataAPName, defApn);
			m.UpdateL();
			CleanupStack::PopAndDestroy(&m);
			aP.iApn.Copy(defApn);
			RDebug::Printf("[pheltsy] data: empty APN filled from the operator table: \"%S\"", &aP.iApn);
			}
		}
	else if (defApn.Length())
		{
		RCmConnectionMethod m = cm.CreateConnectionMethodL(KUidPacketDataBearerType);
		CleanupClosePushL(m);
		m.SetStringAttributeL(ECmName, _L("Mobile data"));
		m.SetStringAttributeL(EPacketDataAPName, defApn);
		m.UpdateL();
		const TUint32 id = m.GetIntAttributeL(ECmId);
		CleanupStack::PopAndDestroy(&m);
		aP.iApn.Copy(defApn);
		RDebug::Printf("[pheltsy] data: access point \"Mobile data\" (%u) provisioned, APN \"%S\"", id, &aP.iApn);
		}
	else
		RDebug::Printf("[pheltsy] data: no packet-data access point and no table entry for %S", &aP.iPlmn);
	CleanupStack::PopAndDestroy(2);			// ids, cm
	}

static TInt PheApnThread(TAny* aArg)
	{
	TPheApnProv& p = *static_cast<TPheApnProv*>(aArg);
	CTrapCleanup* tc = CTrapCleanup::New();
	if (!tc)
		return KErrNoMemory;
	TRAP(p.iErr, PheApnProvisionL(p));
	delete tc;
	return p.iErr;
	}

#endif
