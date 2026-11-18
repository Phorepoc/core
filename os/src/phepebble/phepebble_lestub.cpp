// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_lestub.cpp -- OURS: the stand-in LE transport until the LE host provides PebbleCreateLeTransportL().

#include "phepebble_letransport.h"

namespace
	{
	class TStubTransport : public MPebbleLeTransport
		{
	public:
		TInt StartScan(const TUint8*, TInt, TInt) { return KErrNotSupported; }
		void StopScan() {}
		TInt Connect(const TPebbleLeAddr&) { return KErrNotSupported; }
		void Disconnect() {}
		TInt Pair() { return KErrNotSupported; }
		TInt Read(TUint32) { return KErrNotSupported; }
		TInt Write(TUint32, const TUint8*, TInt, TBool) { return KErrNotSupported; }
		TInt Subscribe(TUint32, TBool) { return KErrNotSupported; }
		TInt Mtu() const { return 23; }
		};
	}

MPebbleLeTransport* PebbleCreateLeTransportL(MPebbleLeObserver& /*aObserver*/)
	{
	return new (ELeave) TStubTransport;
	}
