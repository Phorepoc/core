// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// device_rt.cpp -- OURS: the runtime leaves of a `device` image (emitter/build_mmp.py; an image for the REAL phone,
// C++ exceptions on). The phone's drtaeabi.dll carries the unwinder and the C++ ABI runtime; what g++'s personality
// (linked in from libsupc++) additionally names is std::terminate -- here what Symbian does with an exception
// nobody catches: panic the thread.
#include <e32std.h>

namespace std
	{
	void terminate()
		{
		for (;;)
			User::Invariant();
		}
	}
