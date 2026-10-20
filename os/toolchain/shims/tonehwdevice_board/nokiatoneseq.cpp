// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// nokiatoneseq.cpp -- OURS (PHOREPOC board delta): Nokia tone sequences -> SQNC (see nokiatoneseq.h).
//
// The Nokia format, as AvkonSystemSounds.ra documents it opcode by opcode (all 34 Avkon sounds use only these):
//   0x00           OPCODE_SEQUENCE_START
//   0x11 <ms>      OPCODE_DURATION_UNIT: one tick = <ms> milliseconds (Avkon: 10)
//   0x40+n <ticks> a tone: note n (n = 0: silence), <ticks> long (0 = until stopped)
//                  f = 440 * 2^((n - 34) / 12) Hz -- the file's own examples: 39 = D 587 Hz, 54 = F 1397 Hz
//   0x05 <count>   OPCODE_REPEAT ... 0x06 OPCODE_REPEATEND: the part between, <count> times
//   0x07           OPCODE_RESTART: from the start again, until stopped
//   0x02 <mask>    OPCODE_CONTINUE_MASK (obsolete since DCT4) -- ignored
//   0x0A <info>    OPCODE_TONE_INDICATION (0x80 tone on / 0x7F off) -- no audible effect, ignored
//   0x0B           OPCODE_END
// SQNC (ToneGenerator.cpp TMdaSequenceGenerator): TInt16s -- "SQ" "NC", then tone records
// {samples, freq1, vol1, freq2, vol2}, -2 <count> = loop start, -3 = loop end (two levels at most), -1 = end.
#include "nokiatoneseq.h"
#include <e32math.h>
#include <e32debug.h>
#include "RingBCToneConverter.h"	// the release's Smart Messaging parser (ringbcconverter.cpp)
#undef _DEBUG_

namespace
{
const TInt KAmplitude = 1 << 14;			// as the reference generator's simple tones
const TInt KMaxRecordSamples = 0x7FFF;		// a record's sample count is a TInt16
const TInt KUntilStoppedLoops = 0x7FFF;		// "until stopped" = a long loop (SQNC loops count down)
const TInt KMaxLoopDepth = 2;

TInt NoteFrequency(TInt aNote)
	{
	TReal f;
	if (Math::Pow(f, 2.0, TReal(aNote - 34) / 12.0) != KErrNone)
		return 0;
	return TInt(440.0 * f + 0.5);
	}

class TSqncWriter
	{
public:
	TSqncWriter(TInt16* aOut, TInt aMax) : iOut(aOut), iMax(aMax), iLen(0) {}
	void Put(TInt aValue)	{ if (iLen < iMax) iOut[iLen] = TInt16(aValue); iLen++; }
	// a tone of aSamples, split into records of at most KMaxRecordSamples
	void Tone(TInt aSamples, TInt aFreq)
		{
		const TInt vol = aFreq ? KAmplitude : 0;
		while (aSamples > 0)
			{
			const TInt n = Min(aSamples, KMaxRecordSamples);
			Put(n); Put(aFreq); Put(vol); Put(0); Put(0);
			aSamples -= n;
			}
		}
	TInt Length() const	{ return iLen; }
private:
	TInt16* iOut;
	TInt iMax;
	TInt iLen;
	};

// the opcodes without an operand; every other one takes one byte
TBool NoOperand(TInt aOp)
	{
	return aOp == 0x00 || aOp == 0x06 || aOp == 0x07 || aOp == 0x0B;
	}

// does the sequence restart itself? (walked op by op: an operand can be 0x07, e.g. a 70 ms tick)
TBool HasRestart(const TDesC8& aNokia)
	{
	for (TInt i = 0; i < aNokia.Length(); i += NoOperand(aNokia[i]) ? 1 : 2)
		{
		if (aNokia[i] == 0x07)
			return ETrue;
		if (aNokia[i] == 0x0B)
			break;
		}
	return EFalse;
	}

// one pass over the Nokia bytes; with a NULL writer buffer it only measures
TInt Convert(const TDesC8& aNokia, TInt aRate, TSqncWriter& aOut)
	{
	const TInt len = aNokia.Length();
	const TBool restart = HasRestart(aNokia);
	aOut.Put('S' + ('Q' << 8));
	aOut.Put('N' + ('C' << 8));
	TInt depth = 0;
	if (restart)
		{
		aOut.Put(-2); aOut.Put(KUntilStoppedLoops);
		depth++;
		}
	TInt unit = 10;
	TInt i = 0;
	while (i < len)
		{
		const TInt op = aNokia[i];
		const TInt arg = i + 1 < len ? aNokia[i + 1] : -1;
		if (NoOperand(op))
			{
			if (op == 0x06)
				{
				if (depth == 0)
					return KErrCorrupt;
				aOut.Put(-3);
				depth--;
				}
			else if (op == 0x07 || op == 0x0B)
				break;
			i++;
			continue;
			}
		if (arg < 0)
			return KErrCorrupt;
		if (op == 0x11)
			unit = arg;
		else if (op == 0x05)
			{
			if (depth >= KMaxLoopDepth)
				return KErrCorrupt;
			aOut.Put(-2); aOut.Put(Max(arg, 1));
			depth++;
			}
		else if (op == 0x02 || op == 0x0A)
			{
			// obsolete continue mask / tone indication: nothing to play
			}
		else if (op >= 0x40)
			{
			const TInt note = op - 0x40;
			const TInt freq = note ? NoteFrequency(note) : 0;
			if (arg == 0)
				{
				// until stopped (the dial tone): a one-second tone, looped
				if (depth >= KMaxLoopDepth)
					aOut.Tone(aRate, freq);
				else
					{
					aOut.Put(-2); aOut.Put(KUntilStoppedLoops);
					aOut.Tone(aRate, freq);
					aOut.Put(-3);
					}
				}
			else
				aOut.Tone(TInt(TInt64(arg) * unit * aRate / 1000), freq);
			}
		else
			return KErrCorrupt;
		i += 2;
		}
	while (depth-- > 0)
		aOut.Put(-3);
	aOut.Put(-1);
	return KErrNone;
	}
}

// --- audio U5: Nokia Smart Messaging ringing tones (.rng; the OTA bit-stream: command parts, then a song of
// patterns of notes). Parsed by the release's own CRingBCNSMConverter (ringbcconverter.cpp) into instructions:
// note C..B (1..12, 0 = pause), duration full..1/32 (0..5), dotted/double-dotted/triplet, scale 0..3, style.
// Smart Messaging's scale 1 has A = 440 Hz (scale 2: 880 ...); a full note is 4 beats at the song's tempo, whose
// index selects from the specification's BPM table. Styles: natural = a short gap after the note, continuous =
// none, staccato = half the note sounds.
namespace
{
const TInt KSmBpm[32] = {25, 28, 31, 35, 40, 45, 50, 56, 63, 70, 80, 90, 100, 112, 125, 140,
						 160, 180, 200, 225, 250, 285, 320, 355, 400, 450, 500, 565, 635, 715, 800, 900};

TInt SmartMessagingFrequency(TInt aNote, TInt aScale)
	{
	TReal f;
	if (Math::Pow(f, 2.0, TReal(aScale) + TReal(aNote - ENoteA) / 12.0) != KErrNone)
		return 0;
	return TInt(440.0 * f + 0.5);
	}

void ConvertSong(const TCompData& aSong, TInt aRate, TSqncWriter& aOut)
	{
	aOut.Put('S' + ('Q' << 8));
	aOut.Put('N' + ('C' << 8));
	const TInt bpm = KSmBpm[aSong.iTempo & 31];
	const TInt count = aSong.iSongData ? aSong.iSongData->Count() : 0;
	for (TInt i = 0; i < count; i++)
		{
		const TInstruction& n = aSong.iSongData->At(i);
#ifdef PHOREPOC_TONE_TRACE
		// (diagnostic build only: X32_DEFS=tonehwdevice:PHOREPOC_TONE_TRACE; printed per pass, measure + write)
		if (i == 0)
			RDebug::Printf("[rng] %d instructions, tempo index %d", count, aSong.iTempo);
		RDebug::Printf("[rng] %d: value %d dur %d spec %d scale %d style %d", i, n.iValue, n.iDuration,
					   n.iDurspecifier, n.iScale, n.iStyle);
#endif
		// a full note = 4 beats; in samples: 4 * 60 / bpm * rate, halved per duration step
		TInt64 samples = TInt64(240) * aRate / bpm;
		samples >>= Min(Max(n.iDuration, 0), 5);
		if (n.iDurspecifier == EDurSpecifierDotted)
			samples = samples * 3 / 2;
		else if (n.iDurspecifier == EDurSpecifireDoubleDotted)
			samples = samples * 7 / 4;
		else if (n.iDurspecifier == EDurSpecifierTriplet)
			samples = samples * 2 / 3;
		const TInt freq = (n.iValue >= ENoteC && n.iValue <= ENoteB) ? SmartMessagingFrequency(n.iValue, n.iScale) : 0;
		TInt64 sound = samples;
		if (freq && n.iStyle == EStyleNatural)
			sound = samples * 15 / 16;
		else if (freq && n.iStyle == EStyleStaccato)
			sound = samples / 2;
		aOut.Tone(TInt(sound), freq);
		if (sound < samples)
			aOut.Tone(TInt(samples - sound), 0);
		}
	aOut.Put(-1);
	}
}

TBool NokiaToneSeq::IsSmartMessagingTone(const TDesC8& aData)
	{
	// <command length 2> then the 7-bit "ringing tone programming" command 0100101
	return aData.Length() >= 3 && aData[0] == 0x02 && (aData[1] >> 1) == 0x25;
	}

TBool NokiaToneSeq::IsNokiaSequence(const TDesC8& aData)
	{
	return (aData.Length() >= 2 && aData[0] == 0x00) || IsSmartMessagingTone(aData);
	}

TInt NokiaToneSeq::ToSqnc(const TDesC8& aNokia, TInt aSampleRate, HBufC8*& aSqnc)
	{
	aSqnc = NULL;
	TCompData* song = NULL;
	CRingBCNSMConverter* parser = NULL;
	if (IsSmartMessagingTone(aNokia))
		{
		TRAPD(err, parser = CRingBCNSMConverter::NewL(); song = parser->ConvertNsmToCompDataL(aNokia));
		if (err != KErrNone || !song)
			{
			delete parser;
			return err != KErrNone ? err : KErrCorrupt;
			}
		}
	TSqncWriter measure(NULL, 0);
	TInt err = KErrNone;
	if (song)
		ConvertSong(*song, aSampleRate, measure);
	else
		err = Convert(aNokia, aSampleRate, measure);
	HBufC8* buf = err == KErrNone ? HBufC8::New(measure.Length() * 2) : NULL;
	if (err == KErrNone && !buf)
		err = KErrNoMemory;
	if (err == KErrNone)
		{
		TPtr8 p = buf->Des();
		p.SetLength(measure.Length() * 2);
		TSqncWriter write(reinterpret_cast<TInt16*>(const_cast<TUint8*>(p.Ptr())), measure.Length());
		if (song)
			ConvertSong(*song, aSampleRate, write);
		else
			Convert(aNokia, aSampleRate, write);
		aSqnc = buf;
		}
	delete parser;		// (owns the song data)
	return err;
	}
