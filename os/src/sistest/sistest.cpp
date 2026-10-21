// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// sistest.cpp -- OURS: calls sistestdll.dll, both installed from one SIS; prints "[sistest] answer 42".
#include <e32std.h>
#include <e32debug.h>

IMPORT_C TInt SisTestAnswer();

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[sistest] answer %d", SisTestAnswer());
	return KErrNone;
	}
