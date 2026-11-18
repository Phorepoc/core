// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// e32err.h -- OURS: host shim, the error codes phepebble_codec returns (values as in Symbian's e32err.h).
#ifndef PHEPEBBLE_HOST_E32ERR_H
#define PHEPEBBLE_HOST_E32ERR_H
const int KErrNone = 0;
const int KErrNotFound = -1;
const int KErrArgument = -6;
const int KErrOverflow = -9;
const int KErrCorrupt = -20;
const int KErrNotSupported = -5;
const int KErrInUse = -14;
const int KErrNotReady = -18;
const int KErrTimedOut = -33;
const int KErrDisconnected = -36;
const int KErrAbort = -39;
#endif
