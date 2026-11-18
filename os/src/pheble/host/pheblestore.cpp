// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheblestore.cpp -- OURS (BLE plan, SMP bonds): NimBLE's bond store on Symbian. NimBLE's RAM store holds the keys
// (our/peer security material, CCCDs); every write or delete rewrites the file c:<private path of the host process>
// pheble_bonds.dat with all records, and PheBleStoreInit loads it back. Record: obj_type(1) + the raw
// ble_store_value union (fixed size, same build on both sides). Runs on NimBLE's task only.

#include <e32base.h>
#include <f32file.h>
#include <s32file.h>
#include "pheblesrv.h"

extern "C" {
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "store/ram/ble_store_ram.h"
void ble_store_ram_init(void);
}

namespace {

_LIT(KBondFile, "pheble_bonds.dat");
const TInt KObjTypes[] = { BLE_STORE_OBJ_TYPE_OUR_SEC, BLE_STORE_OBJ_TYPE_PEER_SEC, BLE_STORE_OBJ_TYPE_CCCD };

TBool gLoading = EFalse;

TInt FileName(RFs& aFs, TFileName& aName)
	{
	TInt err = aFs.PrivatePath(aName);
	if (err != KErrNone)
		return err;
	aName.Insert(0, _L("c:"));
	aFs.MkDirAll(aName);
	aName.Append(KBondFile);
	return KErrNone;
	}

struct TSaveCtx
	{
	RFileWriteStream* iOut;
	TInt iError;
	};

int SaveOne(int aObjType, union ble_store_value* aValue, void* aCookie)
	{
	TSaveCtx* ctx = static_cast<TSaveCtx*>(aCookie);
	TRAP(ctx->iError,
		ctx->iOut->WriteUint8L((TUint8)aObjType);
		ctx->iOut->WriteL(reinterpret_cast<const TUint8*>(aValue), sizeof(union ble_store_value)));
	return ctx->iError == KErrNone ? 0 : 1;
	}

void SaveAll()
	{
	if (gLoading)
		return;
	RFs fs;
	if (fs.Connect() != KErrNone)
		return;
	TFileName name;
	RFileWriteStream out;
	if (FileName(fs, name) == KErrNone && out.Replace(fs, name, EFileWrite) == KErrNone)
		{
		TSaveCtx ctx = { &out, KErrNone };
		for (TUint i = 0; i < sizeof(KObjTypes) / sizeof(KObjTypes[0]) && ctx.iError == KErrNone; ++i)
			ble_store_iterate(KObjTypes[i], SaveOne, &ctx);
		TRAP_IGNORE(out.CommitL());
		out.Close();
		}
	fs.Close();
	}

int StoreWrite(int aObjType, const union ble_store_value* aVal)
	{
	int rc = ble_store_ram_write(aObjType, aVal);
	if (rc == 0)
		SaveAll();
	return rc;
	}

int StoreDelete(int aObjType, const union ble_store_key* aKey)
	{
	int rc = ble_store_ram_delete(aObjType, aKey);
	if (rc == 0)
		SaveAll();
	return rc;
	}

void LoadL(RFs& aFs, const TDesC& aName)
	{
	RFileReadStream in;
	User::LeaveIfError(in.Open(aFs, aName, EFileRead));
	CleanupClosePushL(in);
	union ble_store_value value;
	for (;;)
		{
		TInt type = 0;
		TRAPD(eof, type = in.ReadUint8L());
		if (eof != KErrNone)
			break;
		in.ReadL(reinterpret_cast<TUint8*>(&value), sizeof(value));
		ble_store_ram_write(type, &value);
		}
	CleanupStack::PopAndDestroy(&in);
	}

} // namespace

void PheBleStoreInit()
	{
	ble_store_ram_init();
	ble_hs_cfg.store_read_cb = ble_store_ram_read;
	ble_hs_cfg.store_write_cb = StoreWrite;
	ble_hs_cfg.store_delete_cb = StoreDelete;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
	RFs fs;
	if (fs.Connect() != KErrNone)
		return;
	TFileName name;
	if (FileName(fs, name) == KErrNone)
		{
		gLoading = ETrue;
		TRAP_IGNORE(LoadL(fs, name));
		gLoading = EFalse;
		}
	fs.Close();
	}
