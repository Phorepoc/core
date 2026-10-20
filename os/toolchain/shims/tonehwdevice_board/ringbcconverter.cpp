// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// ringbcconverter.cpp -- OURS (PHOREPOC board delta, audio U5): the release's own Nokia Smart Messaging ringing-tone
// parser, CRingBCNSMConverter (app.messaging smartmessaging/ringbc, the ringing-tone BIO control's), compiled into
// the tone HW device so nokiatoneseq.cpp can play .rng files. Unchanged; this wrapper only quiets its trace:
// RingBCToneConverter.h #defines _DEBUG_ unconditionally, OUTSIDE its include guard, so the .cpp always prints an
// RDebug line per bit field and note. RDebug is declared first (its headers are guarded) and then, for this one TU,
// names a no-op.
#include <e32debug.h>
#include <e32svr.h>

namespace
	{
	struct TRingBcQuiet
		{
		static void Print(...) {}
		};
	}
#define RDebug TRingBcQuiet
#include "RingBCToneConverter.cpp"
#undef RDebug
