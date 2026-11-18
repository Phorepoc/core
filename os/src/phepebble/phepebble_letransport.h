// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_letransport.h -- OURS: how the service obtains its LE transport (Symbian side). The Phorepoc LE host
// provides PebbleCreateLeTransportL() in a static library linked into phepebblesrv.exe; until it lands the service links
// phepebble_lestub.cpp, whose transport answers KErrNotSupported (the UI then shows "Bluetooth LE not available").
// Callbacks must arrive on the creating thread (the service's active scheduler).

#ifndef PHEPEBBLE_LETRANSPORT_H
#define PHEPEBBLE_LETRANSPORT_H

#include <e32base.h>
#include "phepebble_transport.h"

/** Create the LE transport for aObserver; the caller owns it. Leaves on failure. */
MPebbleLeTransport* PebbleCreateLeTransportL(MPebbleLeObserver& aObserver);

#endif
