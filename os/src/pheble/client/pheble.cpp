// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheble.cpp -- OURS: the client side of pheble.h (thin IPC to "!PheBle" in the Bluetooth stack's process).

#include "pheble.h"
#include "pheblesrv.h"

EXPORT_C TInt RPheBle::Connect()
	{
	return CreateSession(KPheBleServerName, TVersion(KPheBleMajor, KPheBleMinor, 0), 4);
	}

EXPORT_C TInt RPheBle::StartScan(TBool aActive, TInt aTimeoutSeconds)
	{
	return SendReceive(EPheBleStartScan, TIpcArgs(aActive, aTimeoutSeconds));
	}

EXPORT_C void RPheBle::NextAdvert(TPheBleAdvertBuf& aAdvert, TRequestStatus& aStatus)
	{
	SendReceive(EPheBleNextAdvert, TIpcArgs(&aAdvert), aStatus);
	}

EXPORT_C TInt RPheBle::StopScan()
	{
	return SendReceive(EPheBleStopScan);
	}

EXPORT_C void RPheBle::ConnectDevice(const TPheBleAddr& aAddr, TPheBleConnBuf& aConn, TRequestStatus& aStatus)
	{
	TPckgC<TPheBleAddr> addr(aAddr);
	// the address is read synchronously by the server before it completes or keeps the message
	SendReceive(EPheBleConnect, TIpcArgs(&addr, &aConn), aStatus);
	}

EXPORT_C TInt RPheBle::Disconnect(TUint16 aConn)
	{
	return SendReceive(EPheBleDisconnect, TIpcArgs(aConn));
	}

EXPORT_C void RPheBle::NextEvent(TPheBleEventBuf& aEvent, TDes8& aValue, TRequestStatus& aStatus)
	{
	SendReceive(EPheBleNextEvent, TIpcArgs(&aEvent, &aValue), aStatus);
	}

EXPORT_C void RPheBle::Pair(TUint16 aConn, TRequestStatus& aStatus)
	{
	SendReceive(EPheBlePair, TIpcArgs(aConn), aStatus);
	}

EXPORT_C TInt RPheBle::ConfirmNumeric(TUint16 aConn, TBool aAccept)
	{
	return SendReceive(EPheBleConfirmNumeric, TIpcArgs(aConn, aAccept));
	}

EXPORT_C TInt RPheBle::DeleteBond(const TPheBleAddr& aAddr)
	{
	TPckgC<TPheBleAddr> addr(aAddr);
	return SendReceive(EPheBleDeleteBond, TIpcArgs(&addr));
	}

EXPORT_C void RPheBle::ExchangeMtu(TUint16 aConn, TRequestStatus& aStatus)
	{
	SendReceive(EPheBleExchangeMtu, TIpcArgs(aConn), aStatus);
	}

EXPORT_C void RPheBle::DiscoverAll(TUint16 aConn, TDes8& aEntries, TRequestStatus& aStatus)
	{
	SendReceive(EPheBleDiscoverAll, TIpcArgs(aConn, &aEntries), aStatus);
	}

EXPORT_C void RPheBle::Read(TUint16 aConn, TUint16 aHandle, TDes8& aValue, TPheBleResultBuf& aResult,
	TRequestStatus& aStatus)
	{
	SendReceive(EPheBleRead, TIpcArgs(aConn, aHandle, &aValue, &aResult), aStatus);
	}

EXPORT_C void RPheBle::Write(TUint16 aConn, TUint16 aHandle, const TDesC8& aValue, TBool aWithResponse,
	TPheBleResultBuf& aResult, TRequestStatus& aStatus)
	{
	SendReceive(EPheBleWrite, TIpcArgs(aConn | (aHandle << 16), &aValue, aWithResponse, &aResult), aStatus);
	}

EXPORT_C void RPheBle::Subscribe(TUint16 aConn, TUint16 aCccdHandle, TUint16 aValue, TPheBleResultBuf& aResult,
	TRequestStatus& aStatus)
	{
	SendReceive(EPheBleSubscribe, TIpcArgs(aConn, aCccdHandle | (aValue << 16), NULL, &aResult), aStatus);
	}

EXPORT_C TInt RPheBle::Mtu(TUint16 aConn)
	{
	return SendReceive(EPheBleMtu, TIpcArgs(aConn));
	}

EXPORT_C void RPheBle::CancelAll()
	{
	SendReceive(EPheBleCancelAll);
	}
