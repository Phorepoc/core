// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// cmsetup.cpp -- OURS: the S60 connection-settings bring-up for the Path C board (X4.68). Not Symbian source.
//
// An S60 phone's CommsDat is made at first boot by dbcreator (commsdatstartup: reset CommsDat, create the S60
// tables -- IAP metadata, data-mobility selection policy, bearer priorities, WLAN service -- then import the
// variant's connection methods + destinations from Z:\private\10281BC3\<file>.xml). The release ships no such
// variant file. This board imports its IAPs with CED (tools/phorepoc-x86/commdb_xml.py), so what is left is
// what dbcreator would have added on top, done here with the REAL cmmanager API:
//   * RCmManagerExt::CreateTablesAndOpenL -- the S60 tables (MPM reads them; missing -> KErrNotFound)
//   * an "Internet" destination (SNAP, purpose ESnapPurposeInternet) holding the board's IAP, unless one
//     already exists -- S^3 apps start connections through MPM, which selects from destinations
// Command line: [iap-id], default 2 (CED's "Ethernet - DHCP", the preferred IAP). Idempotent; logs each step.
#include <e32base.h>
#include <e32debug.h>
#include <cmmanagerext.h>
#include <cmdestinationext.h>
#include <cmconnectionmethodext.h>
#include <cmmanagerdef.h>
#include <cdccommsdatcreator.h>
#include <cmconnectionmethoddef.h>
#include <cmpluginwlandef.h>
#include <cmdefconnvalues.h>
#include <commsdattypesv1_1.h>
#include <datamobilitycommsdattypes.h>

using namespace CommsDat;

_LIT(KInternet, "Internet");

// every record of one S60 table, deleted; returns how many there were
template <class R> static TInt PurgeTableL(CMDBSession& aDb, TMDBElementId aTable)
	{
	CMDBRecordSet<R>* set = new (ELeave) CMDBRecordSet<R>(aTable);
	CleanupStack::PushL(set);
	TRAP_IGNORE(set->LoadL(aDb));
	const TInt n = set->iRecords.Count();
	for (TInt i = 0; i < n; i++)
		set->iRecords[i]->DeleteL(aDb);
	CleanupStack::PopAndDestroy(set);
	return n;
	}

// CED re-imports the standard tables at every boot and knows nothing of the S60 ones, so a destination kept on
// C: from an earlier boot loses its Network + AccessPoint records while its selection-policy and SNAP-metadata
// rows survive (cmmanager then lists it but cannot open it: LoadDestAPRecordL -> KErrNotFound). Drop those rows.
static void PurgeStaleDestinationsL()
	{
	CMDBSession* db = CMDBSession::NewLC(KCDLatestVersion);
	TInt n1 = PurgeTableL<CCDDataMobilitySelectionPolicyRecord>(*db, CCDDataMobilitySelectionPolicyRecord::TableIdL(*db));
	TInt n2 = PurgeTableL<CCDSNAPMetadataRecord>(*db, CCDSNAPMetadataRecord::TableIdL(*db));
	RDebug::Printf("PH: [cmsetup] purged stale destinations: %d policy + %d metadata record(s)", n1, n2);
	CleanupStack::PopAndDestroy(db);
	}

static TUint32 IapFromCommandLine()
	{
	TBuf<16> arg;
	User::CommandLine(arg);
	TLex lex(arg);
	TUint32 iap = 2;
	if (lex.Val(iap, EDecimal) != KErrNone || iap == 0)
		iap = 2;
	return iap;
	}

// the destination's membership, by connection-method id; every WLAN connection method the CommsDat holds that is
// not in it yet is added (an S^3 phone's Internet destination holds the user's WLAN access points -- wlanjoin /
// the WLAN wizard create the IAPs at run time, so this runs after them: phebrlaunch starts cmsetup once wlanjoin is
// done). Returns the number added. Idempotent.
static TInt AddWlanMethodsL(RCmManagerExt& aCm, RCmDestinationExt& aDest)
	{
	RArray<TUint32> members;
	CleanupClosePushL(members);
	const TInt n = aDest.ConnectionMethodCount();
	for (TInt i = 0; i < n; i++)
		{
		RCmConnectionMethodExt m;
		TRAPD(e, m = aDest.ConnectionMethodL(i));
		if (e != KErrNone)
			continue;
		TUint32 id = 0;
		TRAP(e, id = m.GetIntAttributeL(CMManager::ECmId));
		if (e == KErrNone)
			members.Append(id);
		m.Close();
		}
	RArray<TUint32> cms;
	CleanupClosePushL(cms);
	TRAPD(le, aCm.ConnectionMethodL(cms, EFalse, EFalse, EFalse));
	RDebug::Printf("PH: [cmsetup] WLAN sweep: %d method(s) (%d), destination has %d", cms.Count(), le, members.Count());
	TInt added = 0;
	for (TInt i = 0; i < cms.Count(); i++)
		{
		if (members.Find(cms[i]) != KErrNotFound)
			continue;
		RCmConnectionMethodExt m;
		TRAPD(e, m = aCm.ConnectionMethodL(cms[i]));
		if (e != KErrNone)
			continue;
		CleanupClosePushL(m);
		TUint32 bearer = 0;
		TRAP(e, bearer = m.GetIntAttributeL(CMManager::ECmBearerType));
		if (e == KErrNone && bearer == KUidWlanBearerType)
			{
			TRAP(e, aDest.AddConnectionMethodL(m));
			RDebug::Printf("PH: [cmsetup] WLAN cm %u -> Internet: AddConnectionMethodL %d", cms[i], e);
			if (e == KErrNone)
				added++;
			}
		CleanupStack::PopAndDestroy(&m);
		}
	if (added)
		{
		TRAPD(ue, aDest.UpdateL());
		RDebug::Printf("PH: [cmsetup] %d WLAN method(s) added, UpdateL: %d", added, ue);
		}
	CleanupStack::PopAndDestroy(2, &members);
	return added;
	}

// An Internet destination that holds a WLAN access point AND the CED "Ethernet - DHCP" IAP (the x86 board's) is poison on a
// phone: MPM walks the destination in priority order, the Ethernet IAP's bearer loads enet.ldd (absent -> -1) and MPM shows the
// OCC discreet popup "Connection failed" / "Connection unavailable" over and over (boot 869). Where a WLAN method is a member,
// the LAN-bearer members are removed from the destination (the IAPs themselves stay in CommsDat). x86 (no WLAN) is unaffected.
#ifndef KPhLanBearerType
#define KPhLanBearerType 0x1020737A
#endif
static TUint32 BearerOfL(RCmDestinationExt& aDest, TInt aIndex)
	{
	RCmConnectionMethodExt m = aDest.ConnectionMethodL(aIndex);
	CleanupClosePushL(m);
	TUint32 b = m.GetIntAttributeL(CMManager::ECmBearerType);
	CleanupStack::PopAndDestroy(&m);
	return b;
	}

static void DropLanIfWlanL(RCmDestinationExt& aDest)
	{
	TBool haveWlan = EFalse;
	for (TInt i = 0; i < aDest.ConnectionMethodCount(); i++)
		{
		TUint32 b = 0;
		TRAPD(e, b = BearerOfL(aDest, i));
		if (e == KErrNone && b == KUidWlanBearerType)
			haveWlan = ETrue;
		}
	if (!haveWlan)
		return;
	TInt removed = 0;
	for (TInt i = aDest.ConnectionMethodCount() - 1; i >= 0; i--)
		{
		TUint32 b = 0;
		TRAPD(e, b = BearerOfL(aDest, i));
		if (e != KErrNone || b != KPhLanBearerType)
			continue;
		RCmConnectionMethodExt m = aDest.ConnectionMethodL(i);
		CleanupClosePushL(m);
		TRAP(e, aDest.RemoveConnectionMethodL(m));
		RDebug::Printf("PH: [cmsetup] LAN method (index %d) out of Internet, a WLAN member exists: %d", i, e);
		if (e == KErrNone)
			removed++;
		CleanupStack::PopAndDestroy(&m);
		}
	if (removed)
		{
		TRAPD(ue, aDest.UpdateL());
		RDebug::Printf("PH: [cmsetup] %d LAN method(s) dropped, UpdateL: %d", removed, ue);
		}
	}

static void SetupL(TUint32 aIap)
	{
	// dbcreator's table set first (what commsdatstartup does at an S60 first boot): WLAN service/device settings,
	// data mobility, virtual IAP next layer, default connection ... -- ConnMon + MPM load them all
	CCdcCommsDatCreator* cdc = CCdcCommsDatCreator::NewLC();
	TRAPD(ct, cdc->CreateTablesL());
	RDebug::Printf("PH: [cmsetup] dbcreator CreateTablesL: %d", ct);
	CleanupStack::PopAndDestroy(cdc);
	RCmManagerExt cm;
	cm.CreateTablesAndOpenL();
	CleanupClosePushL(cm);
	RDebug::Printf("PH: [cmsetup] S60 tables ready");

	RArray<TUint32> dests;
	CleanupClosePushL(dests);
	// a fresh S60 table set has no destination records: CommsDat leaves KErrNotFound loading an empty
	// record set, which here only means "none yet"
	TRAPD(e, cm.AllDestinationsL(dests));
	RDebug::Printf("PH: [cmsetup] AllDestinationsL: %d (%d destinations)", e, dests.Count());
	if (e != KErrNone && e != KErrNotFound)
		User::Leave(e);
	for (TInt i = 0; i < dests.Count(); i++)
		{
		RCmDestinationExt d;
		TRAPD(oe, d = cm.DestinationL(dests[i]));
		if (oe == KErrNone)
			{
			d.Close();
			continue;
			}
		RDebug::Printf("PH: [cmsetup] destination %u cannot be opened: %d", dests[i], oe);
		PurgeStaleDestinationsL();
		dests.Reset();
		break;
		}
	// idempotent across boots (C: keeps CommsDat on a persistent disk): an existing "Internet" destination --
	// by purpose, or by name when an earlier run could not write its metadata -- is repaired, not duplicated
	for (TInt i = 0; i < dests.Count(); i++)
		{
		RCmDestinationExt d = cm.DestinationL(dests[i]);
		CleanupClosePushL(d);
		TUint32 purpose = 0;
		TRAPD(pe, purpose = d.MetadataL(CMManager::ESnapMetadataPurpose));
		HBufC* dn = d.NameLC();
		const TBool isInternet = (pe == KErrNone && purpose == CMManager::ESnapPurposeInternet) || *dn == KInternet;
		CleanupStack::PopAndDestroy(dn);
		if (isInternet)
			{
			RDebug::Printf("PH: [cmsetup] Internet destination exists (id %u), purpose %d/%u", dests[i], pe, purpose);
			if (pe != KErrNone || purpose != CMManager::ESnapPurposeInternet)
				{
				TRAP(e, d.SetMetadataL(CMManager::ESnapMetadataPurpose, CMManager::ESnapPurposeInternet));
				RDebug::Printf("PH: [cmsetup]   SetMetadataL(purpose Internet): %d", e);
				TRAP(e, d.UpdateL());
				RDebug::Printf("PH: [cmsetup]   UpdateL: %d", e);
				}
			TCmDefConnValue def;
			def.iType = ECmDefConnDestination;
			def.iId = dests[i];
			TRAP(e, cm.WriteDefConnL(def));
			RDebug::Printf("PH: [cmsetup]   WriteDefConnL: %d", e);
			TRAP(e, AddWlanMethodsL(cm, d));
			RDebug::Printf("PH: [cmsetup]   WLAN sweep: %d", e);
			TRAP(e, DropLanIfWlanL(d));
			RDebug::Printf("PH: [cmsetup]   drop LAN: %d", e);
			CleanupStack::PopAndDestroy(3, &cm);    // d, dests, cm
			return;
			}
		CleanupStack::PopAndDestroy(&d);
		}

	// every connection method cmmanager recognises (its bearer plug-ins decide), logged; prefer the one
	// named "Ethernet - DHCP" (CED's), else the command-line IAP id
	RArray<TUint32> cms;
	CleanupClosePushL(cms);
	TRAP(e, cm.ConnectionMethodL(cms, EFalse, EFalse, EFalse));
	RDebug::Printf("PH: [cmsetup] ConnectionMethodL(list): %d, %d method(s)", e, cms.Count());
	_LIT(KDhcpName, "Ethernet - DHCP");
	for (TInt i = 0; i < cms.Count(); i++)
		{
		RCmConnectionMethodExt m;
		TRAPD(e2, m = cm.ConnectionMethodL(cms[i]));
		if (e2 != KErrNone)
			{
			RDebug::Printf("PH: [cmsetup]   cm %u: open %d", cms[i], e2);
			continue;
			}
		HBufC* name = NULL;
		TRAP(e2, name = m.GetStringAttributeL(CMManager::ECmName));
		TBuf8<64> n8;
		if (name)
			n8.Copy(name->Left(64));
		RDebug::Printf("PH: [cmsetup]   cm %u: \"%S\"", cms[i], &n8);
		if (name && *name == KDhcpName)
			aIap = cms[i];
		delete name;
		m.Close();
		}
	CleanupStack::PopAndDestroy(&cms);
	RCmConnectionMethodExt method;
	TRAP(e, method = cm.ConnectionMethodL(aIap));
	RDebug::Printf("PH: [cmsetup] ConnectionMethodL(%u): %d", aIap, e);
	User::LeaveIfError(e);
	CleanupClosePushL(method);
	RCmDestinationExt dest;
	TRAP(e, dest = cm.CreateDestinationL(KInternet));
	RDebug::Printf("PH: [cmsetup] CreateDestinationL: %d", e);
	User::LeaveIfError(e);
	CleanupClosePushL(dest);
	TRAP(e, dest.AddConnectionMethodL(method));
	RDebug::Printf("PH: [cmsetup] AddConnectionMethodL: %d", e);
	User::LeaveIfError(e);
	// the purpose metadata (MPM loads the destination's SNAP metadata record; without it -> KErrNotFound). Its
	// earlier USER 7 was CommsDat's StoreL bug (toolchain/shims/commsdat_gcc), not this call
	TRAP(e, dest.SetMetadataL(CMManager::ESnapMetadataPurpose, CMManager::ESnapPurposeInternet));
	RDebug::Printf("PH: [cmsetup] SetMetadataL(purpose Internet): %d", e);
	TRAP(e, dest.UpdateL());
	RDebug::Printf("PH: [cmsetup] UpdateL: %d", e);
	User::LeaveIfError(e);
	TCmDefConnValue def;
	def.iType = ECmDefConnDestination;
	def.iId = dest.Id();
	TRAP(e, cm.WriteDefConnL(def));
	RDebug::Printf("PH: [cmsetup] WriteDefConnL(destination %u): %d", def.iId, e);
	TRAP(e, AddWlanMethodsL(cm, dest));
	RDebug::Printf("PH: [cmsetup] WLAN sweep: %d", e);
	TRAP(e, DropLanIfWlanL(dest));
	RDebug::Printf("PH: [cmsetup] drop LAN: %d", e);
	RDebug::Printf("PH: [cmsetup] created destination Internet (id %u) with IAP %u", dest.Id(), aIap);
	CleanupStack::PopAndDestroy(4, &cm);    // dest, method, dests, cm
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	TUint32 iap = IapFromCommandLine();
	TRAPD(err, SetupL(iap));
	RDebug::Printf("PH: [cmsetup] done: %d", err);
	delete tc;
	RProcess::Rendezvous(KErrNone);   // never blocks the boot list; the log says what happened
	return err;
	}
