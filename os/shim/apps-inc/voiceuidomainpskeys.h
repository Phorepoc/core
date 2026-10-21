/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// OURS (a stand-in): the Voice UI accessory-monitor Publish&Subscribe keys. The open release does not carry
// voiceuidomainpskeys.h; FM Radio's system-event detector only subscribes to the key (slide/cover open-close), which
// nothing publishes here. The UID/values are placeholders, not the Nokia ones.
#ifndef VOICEUIDOMAINPSKEYS_H
#define VOICEUIDOMAINPSKEYS_H
#include <e32std.h>
const TUid KPSUidVoiceUiAccMonitor = { 0x10282F7C };
const TUint32 KVoiceUiOpenKey = 0x00000001;
enum TVoiceUiOpenKey { KVoiceUiIsClose = 0, KVoiceUiIsOpen = 1 };
#endif
