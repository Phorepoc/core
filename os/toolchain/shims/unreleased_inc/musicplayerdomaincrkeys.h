// musicplayerdomaincrkeys.h -- OURS (not Symbian source): a reconstruction of S60 Music Player's
// central-repository key header, which the 2010 release never shipped (Phonebook2's DRM manager and
// the MMS viewer / SMIL player include it). Searched LAST (components.py UNRELEASED_INC).
//
// Sources:
//  - the repository UID and KMusicPlayerFeatures = 1: sf.app.musicplayer's own mpxdbplugin.cpp
//    (KCRUIDMusicPlayerFeatures; a feature bitmask).
//  - KRequireDRMInPlayback / KPlaybackRestrictedMimeTypes: the release does not give their numbers.
//    OURS: 0x2 / 0x3; the board repository
//    (tools/phorepoc-x86/board/cenrep/101ffcd0.txt) carries both, at "no DRM required, nothing blocked".
#ifndef MUSICPLAYERDOMAINCRKEYS_H
#define MUSICPLAYERDOMAINCRKEYS_H

#include <e32std.h>

const TUid KCRUidMusicPlayerFeatures = { 0x101FFCD0 };

// TInt bitmask of Music Player features (mpxdbplugin.cpp: 0x08 = podcasting disabled)
const TUint32 KMusicPlayerFeatures = 0x00000001;
// TInt (TBool): only DRM-protected rich audio may be played
const TUint32 KRequireDRMInPlayback = 0x00000002;
// TDesC: space-separated MIME types whose playback is blocked
const TUint32 KPlaybackRestrictedMimeTypes = 0x00000003;

#endif
