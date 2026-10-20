// nokiatoneseq.h -- OURS (PHOREPOC board delta, docs/phorepoc/audio.md "Key clicks"): Nokia tone sequences -> the
// reference tone HW device's own SQNC format.
//
// Avkon's system sounds (the key click, confirmation/warning/error tones, the network tones: AvkonSystemSounds.ra)
// are Nokia byte-coded tone sequences, played through CMdaAudioToneUtility::PrepareToPlayDesSequence. A Nokia
// phone's (closed) audio adaptation played them; the 2010 release's reference adaptation
// (adapt.NokiaMMAudioReference tonehwdevice) accepts only its own "SQNC" format, so every such sound failed.
#ifndef NOKIATONESEQ_H
#define NOKIATONESEQ_H

#include <e32base.h>

namespace NokiaToneSeq
	{
	// a Nokia Smart Messaging ringing tone (.rng; audio U5): <command length 2> <ringing tone programming> ...
	TBool IsSmartMessagingTone(const TDesC8& aData);

	// a Nokia sequence starts with OPCODE_SEQUENCE_START (0x00) -- or is a Smart Messaging ringing tone
	TBool IsNokiaSequence(const TDesC8& aData);

	// the equivalent SQNC sequence for a sound device running at aSampleRate; KErrCorrupt on an unknown opcode
	TInt ToSqnc(const TDesC8& aNokia, TInt aSampleRate, HBufC8*& aSqnc);
	}

#endif
