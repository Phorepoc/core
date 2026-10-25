# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

"""components_apps -- the user-facing applications beyond the S^3 core: the MPX framework,
the Music Player, then the Video Player and the rest. Merged into components.COMPONENTS unconditionally (a key only
ships when an image has been built for it, so the default ROMs are unchanged until `./ph build` / `./ph arm` makes one).
Paths are relative to baseline/symbian-2010 like components.py ("../symbian-rcl3/..." = the S^3 / BRANCH_RCL_3 worktrees).
"""
import os

from paths import OS_ROOT, SYMBIAN, BUILD, EPOC32_ROOT  # noqa: E402
from paths import resolve as _resolve_src  # noqa: E402  ("baseline/..." -> the Symbian source)


# Video Player headers that no bld.inf export puts on our include path: the Helix stub APIs (MMF scaling / ROP custom commands) and the
# videoplayer_plat APIs (file details dialog, startup message)
_VP = "baseline/symbian-2010/oss.FCL.sf.app.videoplayer/videoplayer_plat/"
_HX = "baseline/symbian-2010/oss.FCL.sf.mw.helix/helix_ren/helix_stub/"
_VP_INC = ["-I", _resolve_src(_HX + "MMFScalingCustomCommands/inc"), "-I", _resolve_src(_HX + "rop_custom_commands_api/inc"),
           "-I", _resolve_src(_VP + "video_player_file_details_dialog_api/inc"), "-I", _resolve_src(_VP + "videoplayer_startup_api/inc")]

_VRP = "baseline/symbian-rcl3/oss.FCL.sf.app.voicerec/"
_VR_INC = ["-I", os.path.join(OS_ROOT, "shim/apps-inc"), "-DRD_MULTIPLE_DRIVE"]
for _d in ("voicerec_plat/voicerecorder_public_constants_api/inc", "voicerec_plat/new_file_service_api/inc", "voicerecorder/help/inc"):
    _VR_INC += ["-I", _resolve_src(_VRP + _d)]
_VR_INC += ["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.camera/camera_plat/camcorder_ui_constants_api/inc")]
_VR_RSC = [_VRP + "voicerec_plat/voicerecorder_public_constants_api/inc", _VRP + "voicerecorder/loc",
           "baseline/symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/waitnotedialog/loc"]

_RADIO_INC = ["-I", os.path.join(OS_ROOT, "shim/apps-inc"), "-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.radio/fmradio/fmradio/inc")]

COMPONENTS = {
    "mpxcollectionhelper": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/collectionhelper/group/mpxcollectionhelper.mmp"),
    "mpxcollectionmediator": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/harvester/collectionmediator/group/mpxcollectionmediator.mmp"),
    "mpxfilehandler": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/harvester/filehandler/group/mpxfilehandler.mmp"),
    "mpxmetadataextractor": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/harvester/metadataextractor/group/mpxmetadataextractor.mmp"),
    "mpxharvesterserver": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/harvester/server/group/mpxharvester.mmp"),
    "mpxharvesterutility": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/harvester/utility/group/mpxharvesterutility.mmp"),
    "mpxalbumartutility": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/mmappcommonui/albumartutility/group/albumartutility.mmp"),
    "mpxbacksteppingutility": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/mmappcommonui/backsteppingutility/group/backsteppingutility.mmp"),
    "playbackhelper": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/playbackhelper/group/playbackhelper.mmp"),
    "mpxplaylistengine": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/playlistengine/group/mpxplaylistengine.mmp"),
    "mpxplaylistrecognizer": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/playlistengine/group/mpxplaylistrecognizer.mmp"),
    "videoplaylistutility": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/videoplaylistutility/group/videoplaylistutility.mmp"),
    "asxparser": dict(mmp="oss.FCL.sf.mw.mmappfw/mmappcomponents/asxparser/group/asxparser.mmp"),
    "mpxcollectionengine": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/collectionframework/collectionengine/group/mpxcollectionengine.mmp"),
    "mpxcollectionserver": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/collectionframework/collectionserver/group/mpxcollectionserver.mmp"),
    "mpxcollectionutility": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/collectionframework/collectionutility/group/mpxcollectionutility.mmp"),
    "mpxcommon": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/commonframework/common/group/mpxcommon.mmp"),
    "mpxplaybackengine": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/playbackframework/playbackengine/group/mpxplaybackengine.mmp"),
    "mpxplaybackserver": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/playbackframework/playbackserver/group/mpxplaybackserver.mmp"),
    "mpxplaybackutility": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/playbackframework/playbackutility/group/mpxplaybackutility.mmp"),
    "mpxviewplugin": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/viewframework/baseviewplugins/group/mpxviewplugin.mmp"),
    "mpxviewutility": dict(mmp="oss.FCL.sf.mw.mmappfw/mpx/viewframework/viewutility/group/mpxviewutility.mmp"),
    "drmutilitycommon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/group/drmutilitycommon.mmp"),
    "drmautomatedusage": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/group/DRMAutomatedUsage.mmp",
                              flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/src")]),
    "drmautomatedusageimpl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/group/DRMAutomatedUsageImpl.mmp",
                                  flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/src")]),   # DrmQueue.h includes src/DrmQueue.inl
    # --- music player (app.musicplayer RCL_3)
    "mpx": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxmusicplayer/app/group/mpx.mmp"),
    "mpxcommonui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxmusicplayer/commonui/group/mpxcommonui.mmp"),
    "mpxmediakeyhandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxmusicplayer/mediakeyhandler/group/mpxmediakeyhandler.mmp"),
    "mpxmetadatahandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxmusicplayer/metadatahandler/group/mpxmetadatahandler.mmp"),
    "mpxmainview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/mainview/group/mpxmainview.mmp"),
    "mpxcollectionview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/collectionview/group/mpxcollectionview.mmp"),
    "mpxplaybackview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/playbackview/group/mpxplaybackview.mmp",
                          flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.os.devicesrv/devicesrv_plat/tv_out_config_api/inc")]),
    "mpxwaitnotedialog": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/waitnotedialog/group/mpxwaitnotedialog.mmp"),
    "mpxaddtracksdialog": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/addtracksdialog/group/mpxaddtracksdialog.mmp"),
    "mpxalbumarteditordialog": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/albumarteditordialog/group/mpxalbumarteditordialog.mmp"),
    "mpxmainviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/plugins/mainviewplugin/group/mpxmainviewplugin.mmp"),
    "mpxcollectionviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/plugins/collectionviewplugin/group/mpxcollectionviewplugin.mmp"),
    "mpxplaybackviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/plugins/playbackviewplugin/group/mpxplaybackviewplugin.mmp",
                                flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.os.devicesrv/devicesrv_plat/tv_out_config_api/inc")]),
    "mpxwaitnotedialogplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/plugins/waitnotedialogplugin/group/mpxwaitnoteplugin.mmp", libs=["eikdlg.lib"]),   # CEikDialog::PrepareLC lives in eikdlg (the mmp lists avkon only)
    "mpxaddtracksdialogplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/plugins/addtracksdialogplugin/group/mpxaddtracksdialogplugin.mmp"),
    "mpxalbumarteditordialogplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/plugins/albumarteditordialogplugin/group/mpxalbumarteditordialogplugin.mmp"),
    "mpxcommoncontainer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/commoncontainer/group/mpxcommoncontainer.mmp"),
    "mpxcommonplaybackview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/viewplugins/views/commonplaybackview/group/mpxcommonplaybackview.mmp",
                                flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.os.devicesrv/devicesrv_plat/tv_out_config_api/inc")]),
    "mpxsqlitedbcommon": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/collectionplugins/mpxsqlitedbcommon/group/mpxsqlitedbcommon.mmp"),
    "mpxsqlitedbplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/collectionplugins/mpxsqlitedbplugin/group/mpxsqlitedbplugin.mmp"),
    "mpxinmemoryplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/collectionplugins/mpxinmemoryplugin/group/mpxinmemoryplugin.mmp"),
    "mpxlocalaudioplayback": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/playbackplugins/localaudio/group/mpxlocalaudioplayback.mmp"),
    "mpxm3uplaylistparsers": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/playlistplugins/m3uplaylistplugin/group/mpxm3uplaylistparsers.mmp"),
    "mpxm3uplaylistplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/playlistplugins/m3uplaylistplugin/group/mpxm3uplaylistplugin.mmp"),
    # DRM Rights Manager app (also gives mpxmetadataeditordialog its drmrightsmanager.rsg)
    "drmrightsmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmrightsmanagerui/group/DRMRightsManager.mmp"),
    # the DRM UI helper the Music Player (via drmuihandling) calls first: was a stub (EUSRNIMP at mpx start)
    "drmuihandlingimpl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/group/DRMUiHandlingImpl.mmp"),
    # libraries the Music Player's closure found as stubs (EUSRNIMP at run time)
    "tspclientmapper": dict(mmp="../symbian-rcl3/oss.FCL.sf.os.devicesrv/accessoryservices/tspclientmapper/group/tspclientmapper.mmp"),
    "tvoutconfig": dict(mmp="oss.FCL.sf.adapt.stubs/systemswstubs/TVOutConfig/group/tvoutconfig.mmp",
                        flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.os.devicesrv/devicesrv_plat/tv_out_config_api/inc")]),
    "audioequalizerutility": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effectspresets/AudioEqualizerUtility/group/AudioEqualizerUtility.mmp"),
    "mpxaudioeffectengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/playbackplugins/audioeffects/group/mpxaudioeffectengine.mmp"),
    "ganes": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/ganes/group/ganes.mmp"),
    "drmutilitywmdrmwrapper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/group/drmutilitywmdrmwrapper.mmp"),
    # AVRCP player-info APIs (the metadata handler publishes the playing track to remote controls) and the UPnP command lib
    "remconplayerinformation": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothappprofiles/avrcp/playerinformation/group/remconplayerinformation.mmp"),
    "remconmediainformationapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothappprofiles/avrcp/mediainformationapi/group/remconmediainformationapi.mmp"),
    "remcongroupnavigationapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothappprofiles/avrcp/groupnavigationapi/group/remcongroupnavigationapi.mmp"),
    "upnpcommand": dict(mmp="oss.FCL.sf.mw.dlnasrv/upnpframework/upnpcommand/group/upnpcommand.mmp"),
    "avrcpipc": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothappprofiles/avrcp/avrcpipc/group/avrcpipc.mmp"),
    # the Remote Control Framework server: the Music Player's media-key handler opens a RemCon session and leaves (-1) without it
    "remconserver": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/server/group/remconserver.mmp"),
    "remcontargetselectorplugin": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/targetselectorplugin/group/remcontargetselectorplugin.mmp"),
    "remconconverterplugin": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/converterplugin/group/remconconverterplugin.mmp"),
    "remcontspcontroller": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryremotecontrol/group/RemConTspController.mmp",
                               flags=["-I", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/targetselectorplugin/public")]),
    "remconkeyeventconverter": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryremotecontrol/group/RemConKeyEventConverter.mmp"),
    # the Podcast collection plug-in: the harvester asks BOTH collections (music + podcast) for track counts; without it the podcast
    # collection id is 0 and the scan fails with -5 (the "Searching for music" note never ends)
    "mpxsqlitepodcastdbplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.musicplayer/mpxplugins/serviceplugins/collectionplugins/mpxsqlitepodcastdbplugin/group/mpxsqlitepodcastdbplugin.mmp"),
    # --- Video Player (app.videoplayer, master == the S^3 Avkon app: no Qt), playback plug-in over MMF (-> phevidctl)
    "mpxvideoplayer": dict(mmp="oss.FCL.sf.app.videoplayer/videoplayerapp/mpxvideoplayer/group/mpxvideoplayer.mmp", flags=_VP_INC, rsc_incs=["baseline/symbian-2010/oss.FCL.sf.app.videoplayer/videoplayerapp/mpxvideoplayer/loc"]),
    "lwplayer": dict(mmp="oss.FCL.sf.app.videoplayer/videoplayerapp/lwplayer/group/lwplayer.mmp", flags=_VP_INC, rsc_incs=["baseline/symbian-2010/oss.FCL.sf.app.videoplayer/videoplayerapp/lwplayer/loc"]),
    "mpxvideohelixplayback": dict(mmp="oss.FCL.sf.app.videoplayer/videoplayback/videohelix/group/mpxvideohelixplayback.mmp", flags=_VP_INC),
    "mpxvideoplaybackviews": dict(mmp="oss.FCL.sf.app.videoplayer/videoplayback/videoplaybackviews/group/mpxvideoplaybackviews.mmp", flags=_VP_INC, rsc_incs=["baseline/symbian-2010/oss.FCL.sf.app.videoplayer/videoplayback/videoplaybackviews/loc"]),
    "mpxvideoplaybackcontrols": dict(mmp="oss.FCL.sf.app.videoplayer/videoplayback/videoplaybackcontrols/group/mpxvideoplaybackcontrols.mmp", flags=_VP_INC, rsc_incs=["baseline/symbian-2010/oss.FCL.sf.app.videoplayer/videoplayback/videoplaybackcontrols/loc"]),
    "mpxvideoplaybackviewplugin": dict(mmp="oss.FCL.sf.app.videoplayer/videoplayback/videoplaybackviewplugin/group/mpxvideoplaybackviewplugin.mmp", flags=_VP_INC),
    "mpfiledetailsdialog": dict(mmp="oss.FCL.sf.app.videoplayer/filedetails/filedetailsdialog/group/MPFileDlg.mmp", flags=_VP_INC, rsc_incs=["baseline/symbian-2010/oss.FCL.sf.app.videoplayer/filedetails/filedetailsdialog/loc"]),
    # the Metadata Engine server: My Videos' collection plug-in (mdeclient) opens an MDS session at start
    "mdsserver": dict(mmp="oss.FCL.sf.mw.mds/metadataengine/server/group/mdsserver.mmp", rom="!mdsserver.exe",   # KMdSServerName = "!MdSServer"
                      rom_data=[r"private\200009F3\schema.mde=baseline/symbian-2010/oss.FCL.sf.mw.mds/metadataengine/data/schema.mde",
                                r"private\200009F3\defaultimportfile.mde=baseline/symbian-2010/oss.FCL.sf.mw.mds/metadataengine/data/defaultimportfile.mde"]),
    # My Videos: the collection plug-in (MDS-backed) + the Hg list main view + its view plug-in (mpxvideoplayer opens it at start)
    "vcxmyvideoscollectionplugin": dict(mmp="oss.FCL.sf.app.videoplayer/videocollection/mpxmyvideoscollection/group/vcxmyvideoscollectionplugin.mmp", flags=_VP_INC),
    "vcxhgmyvideos": dict(mmp="oss.FCL.sf.app.videoplayer/videocollection/hgmyvideos/group/vcxhgmyvideos.mmp",
                          flags=_VP_INC + ["-I", _resolve_src(_VP + "videocollection_api/inc"), "-I", os.path.join(OS_ROOT, "shim/apps-inc"),
                                       "-I", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.app.videoplayer/videofeeds/vccommon/help/inc")],
                          rsc_incs=["baseline/symbian-2010/oss.FCL.sf.app.videoplayer/videofeeds/vccommon/loc"]),
    "vcxhgmyvideosplugin": dict(mmp="oss.FCL.sf.app.videoplayer/videocollection/hgmyvideosplugin/group/vcxhgmyvideosplugin.mmp", flags=_VP_INC),
    # the My Videos overlay-indicator ECOM plugin (interface 0x20029FC4): CMyVideosIndicator::NewL leaves -17004 without it,
    # and the leave through the half-built model handler crashed the Videos thread (KERN-EXEC 3, eip 0x269)
    "myvideosindicatorplugin": dict(mmp="oss.FCL.sf.app.videoplayer/videocollection/myvideosindicatorplugin/group/myvideosindicatorplugin.mmp",
                                     flags=_VP_INC + ["-I", _resolve_src(_VP + "videocollection_api/inc")],
                                     rsc_incs=["baseline/symbian-2010/oss.FCL.sf.app.videoplayer/videoplayer_plat/videocollection_api/inc"]),
    # --- Voice Recorder (app.voicerec RCL_3): the app + 3 own libs, over AudioInputRouting (mmaudio devsound extensions)
    "audioinputrouting": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/audiorouting/Input/AudioInputBase/group/AudioInput.mmp"),
    "newservice": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.voicerec/voicerecorder/group/NewService.mmp", flags=_VR_INC),
    "voicerecorderutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.voicerec/voicerecorder/group/Utils.mmp", flags=_VR_INC),
    "voicerecorderrecview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.voicerec/voicerecorder/group/RecView.mmp", flags=_VR_INC),
    "voicerecorder": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.voicerec/voicerecorder/group/VoiceRecorderApp.mmp", flags=_VR_INC,
                          rsc_incs=_VR_RSC),
    # --- FM Radio (app.radio RCL_3 + mmmw radioutility): the UI runs without a tuner (radio server / tuner DD absent)
    # the radio server + the adaptation stub (adapt.stubs: no tuner hardware; the stub lets CRadioUtility open its session so the UI starts)
    "fmradiotunercontrol": dict(mmp="oss.FCL.sf.adapt.stubs/audiostubs/fmradiotunercontrolstub/Group/FMRadioTunerControlStub.mmp"),
    "radioserver": dict(mmp="oss.FCL.sf.mw.mmmw/mmserv/radioutility/radioserver/Server/Group/RadioServer.mmp"),
    "radiosession": dict(mmp="oss.FCL.sf.mw.mmmw/mmserv/radioutility/radioserver/Session/Group/RadioSession.mmp"),
    "radio_utility": dict(mmp="oss.FCL.sf.mw.mmmw/mmserv/radioutility/radio_utility/group/Radio_Utility.mmp"),
    "presetutility": dict(mmp="oss.FCL.sf.mw.mmmw/mmserv/radioutility/presetutility/group/presetutility.mmp"),
    "fmpresetutility": dict(mmp="oss.FCL.sf.mw.mmmw/mmserv/radioutility/presetutility/group/fmpresetutility.mmp"),
    "fmradioengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.radio/fmradio/fmradioengine/group/fmradioengine.mmp", flags=_RADIO_INC, rsc_incs=["baseline/symbian-rcl3/oss.FCL.sf.app.radio/fmradio/loc"]),
    "fmradio": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.radio/fmradio/fmradio/group/fmradio.mmp", flags=_RADIO_INC, rsc_incs=["baseline/symbian-rcl3/oss.FCL.sf.app.radio/fmradio/loc"]),
    "glx": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/gallery/group/glx.mmp",
                mif_dirs=["../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/icons"]),   # glx_aif.mif (the app-grid icon) + glxicons.mif
    "glxcloudview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/views/cloudview/group/glxcloudview.mmp"),
    "glxcollectionmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/controllers/collectionmanager/group/glxcollectionmanager.mmp"),
    "glxcollectionpluginalbums": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/plugins/glxcollectionpluginalbums/group/glxcollectionpluginalbums.mmp"),
    "glxcollectionpluginall": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/plugins/glxcollectionpluginall/group/glxcollectionpluginall.mmp"),
    # (no glxcollectionplugindownloads: RCL_3 dropped it -- no bld.inf or iby builds it, its .hrh is gone)
    "glxcollectionpluginimageviewer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/plugins/glxcollectionpluginimageviewer/group/glxcollectionpluginimageviewer.mmp"),
    "glxcollectionpluginmonths": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/plugins/glxcollectionpluginmonths/group/glxcollectionpluginmonths.mmp"),
    "glxcollectionplugintags": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/plugins/tagcollectionplugin/group/glxcollectionplugintags.mmp"),
    "glxcommandhandlerbase": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/commandhandlers/commandhandlerbase/group/glxcommandhandlerbase.mmp"),
    "glxcommandhandlerdrm": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/commandhandlers/commandhandlerdrm/group/glxcommandhandlerdrm.mmp"),
    "glxcommandhandlermarking": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/commandhandlers/commandhandlermarking/group/glxcommandhandlermarking.mmp"),
    "glxcommandhandlermoreinfo": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/commandhandlers/commandhandlermoreinfo/group/glxcommandhandlermoreinfo.mmp"),
    "glxcommon": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/common/group/glxcommon.mmp"),
    "glxcommoncommandhandlers": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/commandhandlers/commoncommandhandlers/group/glxcommoncommandhandlers.mmp",
                                     flags=["-idirafter", os.path.join(OS_ROOT, "toolchain/shims/unreleased_inc")]),   # SvgCodecImageConstants.hrh (OURS, empty)
    "glxcommonui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/commonui/group/glxcommonui.mmp"),
    "glxdataprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/dataprovider/group/dataprovider.mmp"),
    "glxdatasourcemanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/datasource/manager/group/glxdatasourcemanager.mmp"),
    "glxdatasourcemde": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/datasource/plugins/glxdatasourcemde/group/glxdatasourcemde.mmp"),
    "glxdrmutility": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/drmutility/group/glxdrmutility.mmp"),
    "glxfetcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/controllers/fetcher/group/glxfetcher.mmp"),
    "glxfullscreenview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/views/fullscreenview/group/glxfullscreenview.mmp"),
    "glxfullscreenviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/plugins/fullscreenviewplugin/group/glxfullscreenviewplugin.mmp"),
    "glxfullscreenviewpluginbase": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/plugins/fullscreenviewpluginbase/group/glxfullscreenviewpluginbase.mmp"),
    "glxgridview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/views/gridview/group/glxgridview.mmp"),
    "glxgridviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/plugins/gridviewplugin/group/glxgridviewplugin.mmp"),
    "glxgridviewpluginbase": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/plugins/gridviewpluginbase/group/glxgridviewpluginbase.mmp"),
    "glximageviewermanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/controllers/imageviewer/group/glximageviewermanager.mmp"),
    "glxivwr": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/imgvwr/group/glxivwr.mmp"),
    "glxlayouts": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/layouts/group/glxlayouts.mmp"),
    "glxlistview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/views/listview/group/glxlistview.mmp"),
    "glxlistviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/plugins/listviewplugin/group/glxlistviewplugin.mmp"),
    "glxlistviewpluginbase": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/plugins/listviewpluginbase/group/glxlistviewpluginbase.mmp"),
    "glxlogging": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/logging/client/group/glxlogging.mmp"),
    "glxmedialists": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/medialists/group/glxmedialists.mmp"),
    "glxmemoryplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/memoryplugin/group/glxmemoryplugin.mmp"),
    "glxmetadatadialog": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/views/metadatadialog/group/glxmetadatadialog.mmp"),
    "glxtagsbrowserviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/plugins/tagsbrowserviewplugin/group/glxtagsbrowserviewplugin.mmp"),
    "glxtexturemanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/texturemanager/group/glxtexturemanager.mmp"),
    "glxthumbnailcomposerplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/thumbnailcreator/thumbnailcomposerplugin/glxthumbnailcomposerplugin.mmp"),
    "glxthumbnailcreator": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/collectionframework/thumbnailcreator/group/glxthumbnailcreator.mmp"),
    "glxtvout": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/tvout/group/glxtvout.mmp"),
    "glxuiutilities": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/uiutilities/group/glxuiutilities.mmp"),
    "glxupnpcommandhandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/commandhandlers/commandhandlerupnp/group/glxupnpcommandhandlers.mmp"),
    "glxviewbase": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/views/viewbase/group/glxviewbase.mmp"),
    "glxvisuallistmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/visuallistmanager/group/glxvisuallistmanager.mmp"),
    "glxzoomview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/viewframework/views/zoomview/group/glxzoomview.mmp"),
    "shwsettingsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/slideshow/settingsdialog/group/shwsettingsdialog.mmp"),
    "shwslideshowengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/slideshow/engine/group/shwslideshowengine.mmp"),
    "shwslideshowviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/slideshow/view/group/shwslideshowviewplugin.mmp"),
    "osncore": dict(defs=["__declspec(x)="], mmp="../symbian-rcl3/oss.FCL.sf.mw.mmuifw/osncore/osncore/group/osncore.mmp"),   # <- glxmedialists
    # (incs_first: glib's own glib/*.h ahead of the exported ossrv_pub copies, which are older -- no G_GNUC_ALLOC_SIZE)
    "libglib": dict(mmp="oss.FCL.sf.os.ossrv/glib/build/symbian/glib/group/libglib.mmp", cdriver=True,   # <- osncore
                    incs_first=[os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.ossrv/glib")], arm_incs_first=True,
                    x32_flags=["-masm=att"]),   # (gatomic.c: AT&T inline assembler. x86 still fails to link: guniprop.c folds a table index into a negative absolute address)
    "glibbackend": dict(mmp="oss.FCL.sf.os.ossrv/glib/build/symbian/glibbackend/group/glibbackend.mmp"),   # <- libglib
    "exiflib": dict(mmp="oss.FCL.sf.os.imagingext/imagingmodules/exiflib/group/ExifLib.mmp"),   # <- glxtexturemanager
    "alfwidgetmodel": dict(defs=["__declspec(x)="], mmp="../symbian-rcl3/oss.FCL.sf.mw.mmuifw/widgetmodel/alfwidgetmodel/group/alfwidgetmodel.mmp"),   # <- glxuiutilities
    "alfwidgetutils": dict(defs=["__declspec(x)="], mmp="../symbian-rcl3/oss.FCL.sf.mw.mmuifw/alfwidgetutils/group/alfwidgetutils.mmp"),   # <- alfwidgetmodel
    "alfresourcepool": dict(defs=["__declspec(x)="], mmp="../symbian-rcl3/oss.FCL.sf.mw.mmuifw/resourcepool/group/alfresourcepool.mmp"),   # <- alfwidgetmodel
    "mulmodelutility": dict(defs=["__declspec(x)="], mmp="../symbian-rcl3/oss.FCL.sf.mw.mmuifw/mulwidgets/muldatamodel/group/mulmodelutility.mmp"),   # <- glxuiutilities
    "mullogging": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.mmuifw/mulwidgets/mullogging/group/mullogging.mmp"),   # <- mulmodelutility
    "mediaclientvideodisplay": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/client/mediaclientvideodisplay.mmp"),   # <- mpxvideoplaybackviews
}

# The mmps' MACRO lines are -D for the C++ compile (build_mmp does that) but NOT for rcomp: a resource that #ifdefs on one
# compiles to a different array than the code indexes (mpxsqlitedbplugin: __ENABLE_PODCAST_IN_MUSIC_MENU puts EBrowsePodcasts in the
# TMCBrowseType enum, the .rss without it has one item less -> MdcaPoint(EBrowseAlbumSong) panicked E32USER-CBase 21 opening an
# album). So every resource-bearing key here passes its mmp MACROs to rcomp as well, as the real abld does.
_RDEFS = {
    "mpx": ["__S60_MPX_AIW_ASSIGN_AS", "__ENABLE_MSK", "__ENABLE_PODCAST_IN_MUSIC_MENU", "CAMESE_IN_DRM_UTILITY", "SINGLE_CLICK_INCLUDED"],
    "mpxcommonui": ["__ENABLE_MSK", "CAMESE_IN_DRM_UTILITY"],
    "mpxmediakeyhandler": ["UPNP_INCLUDED", "__S60_NOTIFIER_EXTRA_ICONS", "__S60_AVKON_LAYOUTS"],
    "mpxmainview": ["__ENABLE_MSK", "__ENABLE_PODCAST_IN_MUSIC_MENU", "SINGLE_CLICK_INCLUDED"],
    "mpxcollectionview": ["UPNP_INCLUDED", "__ENABLE_MSK", "__ENABLE_PODCAST_IN_MUSIC_MENU", "__HG_COLLECTIONVIEW", "BACKSTEPPING_INCLUDED"],
    "mpxaddtracksdialog": ["SINGLE_CLICK_INCLUDED"],
    "mpxalbumarteditordialog": ["__S60_MPX_UI_SPEC_COMPLIANCE", "__S60_MPX_ALBUM_ART_EDITOR_MSK", "RD_MPX_TNM_INTEGRATION"],
    "mpxmainviewplugin": ["__ENABLE_PODCAST_IN_MUSIC_MENU"],
    "mpxcollectionviewplugin": ["__ENABLE_PODCAST_IN_MUSIC_MENU"],
    "mpxcommoncontainer": ["__ENABLE_MSK", "__ENABLE_PODCAST_IN_MUSIC_MENU", "__HG_COLLECTIONVIEW", "HG_MP_LOC_AVAILABLE"],
    "mpxcommonplaybackview": ["UPNP_INCLUDED", "__ENABLE_MSK", "CAMESE_IN_DRM_UTILITY", "BACKSTEPPING_INCLUDED"],
    "mpxsqlitedbplugin": ["__ENABLE_PODCAST_IN_MUSIC_MENU", "__RAMDISK_PERF_ENABLE"],
    "mpxsqlitepodcastdbplugin": ["__ENABLE_PODCAST_IN_MUSIC_MENU"],
    "voicerecorder": ["SINGLE_CLICK_INCLUDED", "RD_MULTIPLE_DRIVE"],
}
for _k, _d in _RDEFS.items():
    COMPONENTS[_k]["rdefs"] = _d

# Photos: the photos_plat/*_api/inc platform headers (their bld.inf exports are not applied by epoc32_export) for every photos key
_PH_PLAT = os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.photos/photos_plat")
_PH_INC = []
if os.path.isdir(_PH_PLAT):
    for _d in sorted(os.listdir(_PH_PLAT)):
        if os.path.isdir(os.path.join(_PH_PLAT, _d, "inc")):
            _PH_INC += ["-I", os.path.join(_PH_PLAT, _d, "inc")]
for _k, _c in COMPONENTS.items():
    if "oss.FCL.sf.app.photos/" in _c.get("mmp", ""):
        _c["flags"] = list(_c.get("flags", [])) + _PH_INC
        _c["rsc_incs"] = list(_c.get("rsc_incs", [])) + ["baseline/symbian-rcl3/oss.FCL.sf.app.photos/photosgallery/gallery/loc"]
