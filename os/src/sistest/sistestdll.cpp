// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// sistestdll.cpp -- OURS: the smallest DLL that proves an x32 DLL loads FROM DISK (a SIS install) under the real
// F32 loader: an export (the in-code export directory, relocated), writable data and a data pointer (data
// relocations), called from sistest.exe.
#include <e32std.h>

static TInt gValue = 40;
static TInt* gValuePtr = &gValue;

EXPORT_C TInt SisTestAnswer()
	{
	return *gValuePtr + 2;
	}
