// phevidctl.h -- OURS (VDU rung V4, docs/phorepoc/vdu-plan.md): the MMF video PLAY controller for MP4 / 3GP files.
//
// What it is: a CMMFController plug-in (ECOM interface 0x101F4F47) that CVideoPlayerUtility (mediaclientvideo) loads for an
// .mp4 / .m4v / .3gp file, exactly the way the 2010 tree's AviPlayController serves .avi (derived from
// baseline/.../mmfwplugins/src/Plugin/Controller/Video/AviPlayController): data source = CMMFFile, custom commands =
// MMMFVideoPlayController / MMMFVideoController / MMMFVideoSetInitScreen, video through CMMFDevVideoPlay.
//   file -> mp4demux (moov sample tables) -> H.264 access units (Annex B, SPS/PPS before every sync sample, PTS = composition
//   time) -> CMMFDevVideoPlay::WriteCodedDataL -> the phevdec hardware decoder (A64 Video Engine) -> NV12 pictures in
//   display order -> this controller times them against the PTS, converts NV12 -> the screen format and draws them into the
//   client's direct-screen-access region (CDirectScreenBitmap, vsync paced, like the V2 player in phevdetest).
// Audio: the AAC track (mp4a/esds) is decoded by pheffmpeg.dll (LGPL FFmpeg, a separate replaceable DLL loaded with RLibrary;
//   missing/failed = video only) to PCM16 and played through the CMMFDevSound of the client's audio output sink (as AviPlayController);
//   DevSound's play position is the master clock that paces the pictures. GetAudioEnabled = the file has usable AAC audio.
// Deliberately not (yet): audio volume / balance custom commands, seeking / pause, graphics surfaces
// (CVideoPlayerUtility2 / MMMFVideoPlaySurfaceSupport), MPEG-4 / H.263 / MPEG-2-in-MP4, fragmented files.
//
// Test hook: custom command interface 0xE70157D5 (phevidtest uses CVideoPlayerUtility::CustomCommandSync):
//   EPheVidGetStats  -> TPheVidStats;   EPheVidSetBench (data1 = TInt 0/1): decode + count only (no timing, no drawing).
#ifndef PHEVIDCTL_H
#define PHEVIDCTL_H

#include <e32std.h>

const TInt KPheVidCtlImplUid = 0xE70157D1;		// the controller (dll UID3 0xE70157D0)
const TInt KPheVidCtlMp4FormatUid = 0xE70157D2;	// format plug-in entries: .mp4 / .m4v
const TInt KPheVidCtl3gpFormatUid = 0xE70157D3;	// .3gp / .3g2
const TUid KPheVidCtlCmdIf = {0xE70157D5};

enum TPheVidCtlCmd
	{
	EPheVidGetStats = 1,
	EPheVidSetBench = 2
	};

struct TPheVidStats
	{
	TInt iFrameW, iFrameH;
	TInt iSamples;			// video samples in the file
	TInt iDurationMs;		// from the file
	TInt iFed;				// coded samples written to DevVideo
	TInt iDelivered;		// pictures the decoder handed back (NextPictureL)
	TInt iDrawn;			// pictures drawn into the DSA region
	TInt iDropped;			// pictures skipped because they were more than 3 frame periods late
	TInt iNoRegion;		// pictures consumed with no (or an empty) DSA region: nothing to draw into
	TInt iElapsedMs;		// first picture due .. last picture consumed (wall clock)
	TInt iLateMaxMs, iLateAvgMs;	// presentation lateness against the PTS
	TInt iBench;
	TInt iHasAudio;		// the file has an audio track
	// audio (AAC via pheffmpeg.dll -> CMMFDevSound; the audio clock paces the video)
	TInt iAudioOn;			// audio was decoded and played this run
	TInt iAudioRate, iAudioCh;
	TInt iAudioPackets;	// AAC access units decoded
	TInt iAudioPcmFrames;	// PCM frames (per channel) handed to DevSound
	TInt iAudioBuffers;	// DevSound buffers filled
	TInt iAudioPlayedMs;	// DevSound play position at the end
	TInt iAudioClock;		// 0 none (wall clock paced), 1 GetTimePlayed, 2 SamplesPlayed
	TInt iAudioErr;		// first audio error (audio then dropped, video goes on)
	TInt iDriftAvgMs, iDriftMaxMs;	// A/V drift: how far the picture was behind the audio clock when shown (avg, max abs)
	TInt iError;
	};

#endif
