/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// ph_mixsc.h -- OURS (audio Q0, 2.1/2.2). FORCE-INCLUDED (-include) into every
// translation unit of mdasoundadapter.dll. It declares RMixSoundSc, a virtual SoundSc channel with the exact RSoundSc
// call surface the 2010 adapter uses, then #defines RSoundSc to it, so the UNMODIFIED mdasoundadapterbody.cpp opens
// "its" TX unit 0 / RX unit 4 on the userland mixer (phmixer.cpp) instead of the exclusive kernel LDD channel. Every
// RMdaDevSound::Open gets its own virtual channel; the mixer sums all of them onto the ONE real channel at 48 kHz.
// phmixer.cpp #undef's RSoundSc to reach the real driver. RMdaDevSound's exports are unchanged.
#ifndef PH_MIXSC_H
#define PH_MIXSC_H

#include <e32std.h>
#include <e32cmn.h>
#include <d32soundsc.h>		// the REAL RSoundSc, declared before the #define below

class RMixSoundSc
	{
public:
	RMixSoundSc() : iHandle(0), iUnit(-1), iVol(255), iChan(0), iRec(0) {}

	static TVersion VersionRequired()	{ return RSoundSc::VersionRequired(); }
	TInt Open(TInt aUnit);
	void Close();
	TInt Handle() const					{ return iHandle; }

	void Caps(TDes8& aCapsBuf);
	void AudioFormat(TDes8& aFormatBuf);
	TInt SetAudioFormat(const TDesC8& aFormatBuf);
	void GetBufferConfig(TDes8& aConfigBuf);
	TInt SetBufferChunkCreate(const TDesC8& aConfigBuf, RChunk& aChunk);
	TInt Volume();
	TInt SetVolume(TInt aVolume);
	void PlayData(TRequestStatus& aStatus, TInt aBufferOffset, TInt aLength, TUint aFlags = 0);
	void RecordData(TRequestStatus& aStatus, TInt& aLength);
	TInt ReleaseBuffer(TInt aChunkOffset);
	void CancelPlayData();
	void CancelRecordData();
	void Cancel(const TRequestStatus& aStatus);
	TInt BytesTransferred();
	void ResetBytesTransferred();
	TInt Pause();
	TInt Resume();

private:
	TInt iHandle;		// non-zero while open (the body only tests it)
	TInt iUnit;			// KSoundScTxUnit0 (a mixer channel) or KSoundScRxUnit0 (record pass-through)
	TInt iVol;			// RX: the level the client set (kept until the real unit is opened)
	TAny* iChan;		// TX: the mixer's TChannel
	TAny* iRec;			// RX: the REAL RSoundSc, opened lazily on the first record call
	};

#define RSoundSc RMixSoundSc

#endif
