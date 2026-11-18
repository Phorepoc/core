// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_ppogatt.cpp -- OURS: see phepebble_ppogatt.h.

#include "phepebble_ppogatt.h"

TPPoGattClient::TPPoGattClient(MPebbleLeTransport& aTransport, MPPoGattObserver& aObserver)
	: iTransport(aTransport), iObserver(aObserver), iState(EIdle)
	{
	ResetLink();
	}

void TPPoGattClient::ResetLink()
	{
	iVersion = 0;
	iTxWindow = 4;            // v0 window until the watch tells us its own
	iTxSeq = 0;
	iRxSeq = 0;
	iTxHead = iTxCount = 0;
	iInFlightFirst = iInFlightCount = 0;
	iIdleTicks = iTimeouts = 0;
	}

TInt TPPoGattClient::Start()
	{
	ResetLink();
	iResends = 0;
	iState = ESubscribing;
	TInt r = iTransport.Subscribe(KPebbleCharRevData, ETrue);
	if (r != KErrNone)
		iState = EIdle;
	return r;
	}

void TPPoGattClient::Stop()
	{
	iState = EIdle;
	ResetLink();
	}

void TPPoGattClient::HandleSubscribed(TInt aError)
	{
	if (iState != ESubscribing)
		return;
	if (aError != KErrNone)
		{
		Close(aError);
		return;
		}
	iState = EWaitResetRequest;
	iIdleTicks = 0;
	}

void TPPoGattClient::Close(TInt aReason)
	{
	TBool wasOpen = iState == EOpen;
	iState = EIdle;
	ResetLink();
	if (wasOpen || aReason != KErrNone)
		iObserver.PPoGattClosed(aReason);
	}

TInt TPPoGattClient::PacketPayload() const
	{
	TInt n = iTransport.Mtu() - 4;
	if (n > KPPoGattMaxPacket)
		n = KPPoGattMaxPacket;
	return n < 1 ? 1 : n;
	}

void TPPoGattClient::SendControl(TUint8 aType, TUint8 aSeq, const TUint8* aPayload, TInt aLength)
	{
	TUint8 pkt[8];
	pkt[0] = TUint8((aSeq << 3) | aType);
	for (TInt i = 0; i < aLength && i < 7; ++i)
		pkt[1 + i] = aPayload[i];
	iTransport.Write(KPebbleCharRevDataWr, pkt, 1 + aLength, EFalse);
	}

TInt TPPoGattClient::Send(const TUint8* aData, TInt aLength)
	{
	if (aLength > KPPoGattTxBuffer - iTxCount)
		return KErrOverflow;
	for (TInt i = 0; i < aLength; ++i)
		iTx[(iTxHead + iTxCount + i) % KPPoGattTxBuffer] = aData[i];
	iTxCount += aLength;
	Pump();
	return KErrNone;
	}

void TPPoGattClient::Pump()
	{
	if (iState != EOpen)
		return;
	const TInt max = PacketPayload();
	while (iTxCount > 0 && iInFlightCount < iTxWindow)
		{
		TInt slot = (iInFlightFirst + iInFlightCount) % KPPoGattMaxWindow;
		TUint8* pkt = iInFlight[slot];
		TInt n = iTxCount < max ? iTxCount : max;
		pkt[0] = TUint8((iTxSeq << 3) | ETypeData);
		for (TInt i = 0; i < n; ++i)
			pkt[1 + i] = iTx[(iTxHead + i) % KPPoGattTxBuffer];
		iTxHead = (iTxHead + n) % KPPoGattTxBuffer;
		iTxCount -= n;
		iInFlightLen[slot] = n + 1;
		iInFlightCount++;
		iTxSeq = TUint8((iTxSeq + 1) & 31);
		iTransport.Write(KPebbleCharRevDataWr, pkt, n + 1, EFalse);
		}
	}

void TPPoGattClient::HandleNotification(const TUint8* aValue, TInt aLength)
	{
	if (aLength < 1 || iState == EIdle || iState == ESubscribing)
		return;
	const TUint8 type = TUint8(aValue[0] & 7);
	const TUint8 seq = TUint8(aValue[0] >> 3);
	switch (type)
		{
	case ETypeResetRequest:
		{
		// {version, serial}: (re)start the link. A reset during a session ends that session first.
		if (iState == EOpen)
			iObserver.PPoGattClosed(KErrAbort);
		ResetLink();
		iVersion = aLength >= 2 ? aValue[1] : 0;
		const TUint8 windows[2] = {KPPoGattOurRxWindow, KPPoGattOurTxWindow};
		SendControl(ETypeResetComplete, 0, windows, iVersion >= 1 ? 2 : 0);
		iState = EWaitResetComplete;
		break;
		}
	case ETypeResetComplete:
		if (iState != EWaitResetComplete)
			break;
		if (iVersion >= 1 && aLength >= 3)
			{
			// the watch's {rx window, tx window}: we may have its rx window in flight
			TInt w = aValue[1];
			iTxWindow = w < 1 ? 1 : (w > KPPoGattMaxWindow ? KPPoGattMaxWindow : w);
			}
		iState = EOpen;
		iIdleTicks = iTimeouts = 0;
		iObserver.PPoGattOpened();
		Pump();
		break;
	case ETypeAck:
		{
		if (iState != EOpen)
			break;
		// cumulative: everything up to and including seq is acknowledged
		TInt found = -1;
		for (TInt i = 0; i < iInFlightCount; ++i)
			{
			TInt slot = (iInFlightFirst + i) % KPPoGattMaxWindow;
			if ((iInFlight[slot][0] >> 3) == seq)
				{
				found = i;
				break;
				}
			}
		if (found < 0)
			break;
		iInFlightFirst = (iInFlightFirst + found + 1) % KPPoGattMaxWindow;
		iInFlightCount -= found + 1;
		iIdleTicks = iTimeouts = 0;
		Pump();
		break;
		}
	case ETypeData:
		if (iState != EOpen)
			break;
		if (seq == iRxSeq)
			{
			SendControl(ETypeAck, seq, 0, 0);
			iRxSeq = TUint8((iRxSeq + 1) & 31);
			iObserver.PPoGattData(aValue + 1, aLength - 1);
			}
		else
			{
			// duplicate or gap: re-ack the last in-order packet so the watch resends from there
			SendControl(ETypeAck, TUint8((iRxSeq + 31) & 31), 0, 0);
			}
		break;
	default:
		break;
		}
	}

void TPPoGattClient::Tick()
	{
	if (iState != EOpen || iInFlightCount == 0)
		{
		iIdleTicks = 0;
		return;
		}
	if (++iIdleTicks < KPPoGattTimeoutTicks)
		return;
	iIdleTicks = 0;
	if (++iTimeouts > KPPoGattMaxTimeouts)
		{
		Close(KErrTimedOut);
		return;
		}
	// go back N: resend every unacked packet as it was
	for (TInt i = 0; i < iInFlightCount; ++i)
		{
		TInt slot = (iInFlightFirst + i) % KPPoGattMaxWindow;
		iTransport.Write(KPebbleCharRevDataWr, iInFlight[slot], iInFlightLen[slot], EFalse);
		iResends++;
		}
	}
