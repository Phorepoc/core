// phmixer_core.h -- OURS (audio Q0, docs/phorepoc/audio-mixing-plan.md 2.4): the PURE mixing functions of the userland
// mixer (phmixer.cpp): dB-code -> gain, streaming polyphase windowed-sinc resampler (mono/stereo S16 -> 48 kHz stereo S32 accumulator;
// the table: phmixer_sinc.h, tools/gen_phmix_sinc.py),
// sum + clip. No Symbian types, no includes, no allocation: the same header is compiled by the Symbian build (x32 and
// ARM) and by the host unit test tests/user/audiomix (MSVC), so the arithmetic is tested where it can be.
#ifndef PHMIXER_CORE_H
#define PHMIXER_CORE_H

#include "phmixer_sinc.h"

namespace PhMix
{
typedef short S16;
typedef int S32;
typedef unsigned int U32;

const int KOutRate = 48000;			// the mix format: 48 kHz, stereo, S16
const int KUnityQ14 = 16384;		// gain 1.0 in Q14

// soundsc volume code (0..255, 255 = full scale, 0.5 dB per step below it) -> linear gain in Q14.
// gain = 10^((code-255)/40) = 2^(-(255-code) * 0.0830482...), evaluated with an exponent in Q16 (5443/65536) and a
// 16-segment table of 2^-(i/16) for the fraction. Within 0.1 % of the exact value; 0 is mute.
inline S32 GainQ14FromCode(int aCode)
	{
	static const S32 KPow2[17] = { 16384, 15689, 15024, 14387, 13777, 13193, 12634, 12098, 11585, 11094,
								   10624, 10173, 9742, 9329, 8933, 8555, 8192 };
	if (aCode <= 0)
		return 0;
	if (aCode >= 255)
		return KUnityQ14;
	const S32 e = (255 - aCode) * 5443;			// Q16 octaves of attenuation
	const int n = e >> 16;						// whole octaves
	if (n >= 15)
		return 0;
	const S32 f = e & 0xFFFF;					// fraction, 0..65535
	const int seg = f >> 12;					// 0..15
	const S32 rem = f & 0xFFF;					// within the segment
	const S32 v = KPow2[seg] - (((KPow2[seg] - KPow2[seg + 1]) * rem) >> 12);
	return v >> n;
	}

// The streaming resampler state of ONE stream (2026-10-15: windowed sinc; the linear interpolation it replaced left
// images of the music above the source band -- the hiss under music on the phone). A ring of the last KSincTaps input
// frames per channel, stored twice so the window is contiguous; the output instant lies between window frames
// KSincTaps/2-1 and KSincTaps/2, at iPhase/65536. A stream starts with KSincTaps/2-1 zero frames (latency = 15 input
// frames). 48 kHz sources bypass it (no filtering, no latency).
struct TStream
	{
	S32 iPhase;					// Q16, 0..65535
	int iPrimed;				// the ring holds the leading zeros
	int iFill;					// frames in the window (KSincTaps = ready)
	int iW;						// ring index of the oldest frame
	S16 iH[2][2 * KSincTaps];	// L, R
	};

inline void Reset(TStream& aS)
	{
	aS.iPhase = 0;
	aS.iPrimed = 0;
	aS.iFill = 0;
	aS.iW = 0;
	for (int c = 0; c < 2; ++c)
		for (int k = 0; k < 2 * KSincTaps; ++k)
			aS.iH[c][k] = 0;
	}

inline void Push(TStream& aS, S16 aL, S16 aR)
	{
	aS.iH[0][aS.iW] = aS.iH[0][aS.iW + KSincTaps] = aL;
	aS.iH[1][aS.iW] = aS.iH[1][aS.iW + KSincTaps] = aR;
	if (++aS.iW == KSincTaps)
		aS.iW = 0;
	}

// Q16 input frames per output frame; rates above 48 kHz are not accepted by the virtual device (step is clamped to 1.0)
inline S32 StepQ16(int aRate)
	{
	if (aRate >= KOutRate)
		return 65536;
	if (aRate <= 0)
		return 0;
	return (S32)(((long long)aRate << 16) / KOutRate);
	}

// Mix one request segment into the accumulator. aIn = aInFrames frames of aChannels (1 or 2) S16 samples at aRate.
// aAcc = interleaved stereo S32 accumulator (aOutFrames frames), the stream's contribution is ADDED at *aAccPos onward
// (scaled by aGainQ14). Stops when the output quantum is full or the input segment is used up (state kept, so the next
// segment continues seamlessly). Returns the number of output frames produced; *aInUsed = input frames consumed.
inline int MixSegment(TStream& aS, const S16* aIn, int aInFrames, int aChannels, int aRate, S32 aGainQ14,
					  S32* aAcc, int aOutFrames, int aAccPos, int* aInUsed)
	{
	int in = 0, out = 0;
	if (aRate >= KOutRate)
		{	// the mix rate already: straight through
		while (aAccPos + out < aOutFrames && in < aInFrames)
			{
			const S32 l = aIn[in * aChannels];
			const S32 r = aChannels == 2 ? aIn[in * aChannels + 1] : l;
			S32* a = aAcc + (aAccPos + out) * 2;
			a[0] += (S32)(((long long)l * aGainQ14) >> 14);
			a[1] += (S32)(((long long)r * aGainQ14) >> 14);
			++in;
			++out;
			}
		*aInUsed = in;
		return out;
		}
	const S32 step = StepQ16(aRate);
	if (!aS.iPrimed)
		{
		for (int k = 0; k < KSincTaps / 2 - 1; ++k)
			Push(aS, 0, 0);
		aS.iFill = KSincTaps / 2 - 1;
		aS.iPrimed = 1;
		}
	while (aAccPos + out < aOutFrames)
		{
		while (aS.iFill < KSincTaps)
			{
			if (in >= aInFrames)
				goto done;
			const S16 l = aIn[in * aChannels];
			Push(aS, l, aChannels == 2 ? aIn[in * aChannels + 1] : l);
			++aS.iFill;
			++in;
			}
		{
		const short* h = KSinc[aS.iPhase >> 7];				// 65536 / KSincPhases = 128
		const S16* x0 = &aS.iH[0][aS.iW];
		long long s0 = 0, s1 = 0;
		if (aChannels == 2)
			{
			const S16* x1 = &aS.iH[1][aS.iW];
			for (int k = 0; k < KSincTaps; ++k)
				{
				s0 += (long long)h[k] * x0[k];
				s1 += (long long)h[k] * x1[k];
				}
			}
		else
			{
			for (int k = 0; k < KSincTaps; ++k)
				s0 += (long long)h[k] * x0[k];
			s1 = s0;
			}
		S32* a = aAcc + (aAccPos + out) * 2;
		a[0] += (S32)(((s0 >> 14) * aGainQ14) >> 14);
		a[1] += (S32)(((s1 >> 14) * aGainQ14) >> 14);
		}
		++out;
		aS.iPhase += step;
		if (aS.iPhase >= 65536)
			{
			aS.iPhase -= 65536;
			--aS.iFill;			// the next output needs one more input frame
			}
		}
done:
	*aInUsed = in;
	return out;
	}

// accumulator (interleaved S32) -> S16 with hard clipping
inline void ClipToS16(const S32* aAcc, S16* aOut, int aSamples)
	{
	for (int i = 0; i < aSamples; ++i)
		{
		S32 v = aAcc[i];
		aOut[i] = (S16)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
		}
	}
}	// namespace PhMix

#endif
