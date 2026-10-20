# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-1.0

"""components -- the registry build_mmp.py builds from.

Key = the LIBRARY stem clients name in their .mmp (`LIBRARY estor.lib` -> "estor").
Fields:
    mmp     path of the REAL 2010 .mmp, relative to <symbian>/symbian-2010
    rom     file name in the ROM / import-block name (default: <key>.dll|.exe by type)
    def     override .def path (relative to <symbian>/symbian-2010) when the abld
            rule can't find it
    defs    extra -D macros          flags   extra g++ flags
    nosrc   True: ignore the mmp SOURCE list (component is extra-only, e.g. a null shim)
    drop    source basenames to leave out (lower-case)
    extra   extra source files (relative to the OS root, see paths.py) -- shims / fill-ins, OURS
    x32_*   x32 (tools/emitter/build_mmp_x32.py) only: x32_mmp replaces mmp, x32_extra replaces extra,
            x32_flags / x32_drop add to flags / drop, x32_real = ignore drop/extra/nosrc (the real
            component on Path C where the older ARM line carries a null shim)
    allow_wsd   True to accept writable static data in a DLL
    static_ctors    True: _E32Dll runs the DLL's .init_array on process attach (needs WSD)
    cdriver     True: compile .c/.C sources with the gcc (C) driver -- g++ builds them as C++
    rdefs       extra -D macros for the resource compiler (rcomp) only
    stack       EXE stack size override (bytes) when the mmp's EPOCSTACKSIZE / the 8K default is too small
    device      True (build_mmp.py, ARM): an image for a stock Symbian loader (RAM-loaded, not XIP) -- C++ exceptions (the release's
                TRAP = try/catch, Leave = throw), the ARM runtime imported from drtaeabi/dfpaeabi/scppnwdl
                (only GCC's personality linked in), euser ordinals of the release .def only, ELF import format,
                the mmp's CAPABILITY / VENDORID / SID in the header

    nga         True: part of the NGA (OpenWF-C) graphics stack -- shipped only in a ROM built with NGA=1
                (tools/x32_rom_args.py); the non-NGA twins (NGA_TWINS) are then left out
    rom_as      the Sys/Bin name an NGA twin is staged under (its rom= is a distinct image file)
    arm_heap_max    build_mmp.py (ARM) EXE heap max override (bytes)

BUILTIN: providers built by their own bespoke scripts (euser/efsrv/hal/efile).
"""
import os

import build_euser_dll as BE
import devlink

G = "oss.FCL.sf.os.graphics"
OSSRV = "oss.FCL.sf.os.ossrv"
PDATA = "oss.FCL.sf.os.persistentdata"
TXT = "oss.FCL.sf.os.textandloc"
CUI = "../symbian-rcl3/oss.FCL.sf.mw.classicui"
CFW = "oss.FCL.sf.os.commsfw"
KHS = "oss.FCL.sf.os.kernelhwsrv/kernel/eka/compsupp/"
_SSM = "oss.FCL.sf.os.devicesrv/sysstatemgmt"
NET = "oss.FCL.sf.os.networkingsrv"
from paths import OS_ROOT, SYMBIAN, BUILD, EPOC32_ROOT  # noqa: E402
from paths import resolve as _resolve_src  # noqa: E402  ("baseline/..." -> the Symbian source)

BUILTIN = {
    "euser": ("euser.dll", BE.DEF_PATH),
    "efsrv": ("efsrv.dll", devlink.EFSRV_DEF),
    "hal": ("hal.dll", devlink.HAL_DEF),
    "efile": ("efile.exe", devlink.EFILE_DEF),
    # the ARM EABI runtime DLLs every device image imports (RVCT helpers, EHABI + C++ ABI, operator new/delete):
    "drtaeabi": ("drtaeabi.dll", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.kernelhwsrv/kernel/eka/compsupp/eabi/drtaeabiu.def")),
    "dfpaeabi": ("dfpaeabi.dll", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.kernelhwsrv/kernel/eka/compsupp/eabi/dfpaeabiu.def")),
    "scppnwdl": ("scppnwdl.dll", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.kernelhwsrv/kernel/eka/compsupp/eabi/scppnwdlu.def")),
}

_SFVG = "oss.FCL.sf.os.graphics/openvg/openvgrefimplementation/sfopenvg/group/"
_OENV = "oss.FCL.sf.os.ossrv/genericopenlibs/openenvcore/"
_ALF = "oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/"
# ALF_LOG=1 ph component build -arch arm alfrenderstage alfappservercore alfdecoderserverclient_real: the release's own RDebug::Print logging
# (alflogger.h _ALF_LOGGING) -- "CAlfRenderStage: ready to rock", connect/bridge steps; per-frame lines too, so first boots only
_ALFLOG = dict(defs=["_ALF_LOGGING"]) if os.environ.get("ALF_LOG") == "1" else {}
_ALFRC = os.environ.get("ALF_RC", "c")
# ALF_TRACE (default on; =0 off): toolchain/shims/alf_fix/ph_alflog.h force-included -- the __ALFLOGSTRING* lifecycle lines become
# throttled "PH: alf: ..." RDebug::Printf lines (the kernel log keeps only PH: lines); + the *_ph.cpp wrappers that add PH: lines
# at the alf server start, the alfredserver thread and the compositor stage construction. WSD: the per-site counters.
_ALFTRACE = os.environ.get("ALF_TRACE") != "0"
_AG = "toolchain/shims/alf_fix/gen/"
_ALFT_FLAGS = ["-include", os.path.join(OS_ROOT, "toolchain/shims/alf_fix/ph_alflog.h")] if _ALFTRACE else []
# (audio U5) the messaging ringing-tone BIO control's Smart Messaging parser, compiled into the tone HW device
_RINGBC = "baseline/symbian-2010/oss.FCL.sf.app.messaging/messagingappbase/smartmessaging/ringbc"
# Q0 (ON by default): the userland mixer. mdasoundadapter is compiled with ph_mixsc.h force-included (RSoundSc ->
# RMixSoundSc) + phmixer.cpp, and MMRC's decision rule is replaced by "always process", so concurrent clients (music
# stream + effects + key clicks) reach the mixer instead of being denied/paused. PHOREPOC_NOMIX=1 = the stock
# (exclusive-channel) chain, for the A0 before/after measurement. REMOVE this block + the three entries' ** spreads
# to drop Q0.
_MIXQ0 = os.environ.get("PHOREPOC_NOMIX") != "1"
_MIXDIR = "toolchain/shims/mdasoundadapter_mix"
_MDASND_MIX = dict(allow_wsd=True, extra=[_MIXDIR + "/phmixer.cpp"],
                   flags=["-include", os.path.join(OS_ROOT, _MIXDIR, "ph_mixsc.h")]) if _MIXQ0 else {}
_MMRC_MIX = dict(drop=["mmrcserverrulemanager.cpp"],
                 extra=["toolchain/shims/mmrc_mixing/mmrcserverrulemanager.cpp"]) if _MIXQ0 else {}
# A0 (OFF by default, temporary): PHOREPOC_AUDIO_A0=1 turns on the release's own trace switches of the audio chain
# (compile-time defines, no source edits): adapter Open/Close/PlayData/state prints, MMRC + ACF decisions, the SW codec
# wrapper's Start/Stop/Pause, and (Q0 on) the mixer's lifecycle. Remove the _A0 spreads when A0 is done.
_A0 = os.environ.get("PHOREPOC_AUDIO_A0") == "1"
def _a0(*defs):
    return dict(defs=list(defs)) if _A0 else {}
# STLport (Open C++) on x32: a board copy of its config/stlcomp.h first on the path -- the release selects a
# compiler configuration by __GCCE__/__ARMCC__/__WINSCW__, and our __GCC32__ build fell through to none. Every
# component that uses the STL (OS_LAYER_STDCPP_SYSTEMINCLUDE) takes incs_first=STLPORT_X32
STLPORT_X32 = ["toolchain/shims/stlport_board"]
# ... and symcpp.h (std::nothrow_t + the nothrow new/delete), which GCCE's and RVCT's compiler pre-include headers
# include and our __GCC32__ prefix does not: x32_flags=STLPORT_X32_FLAGS
STLPORT_X32_FLAGS = ["-include", "symcpp.h"]
# S^3 menu code tests references against NULL (`if ( &aParam != NULL )`, hnliwutils.cpp): RVCT kept the
# test, modern g++ deletes it as undefined -> the NULL list is dereferenced. Keep such checks.
_KEEPNULL = "-fno-delete-null-pointer-checks"
# components whose sources include the trace compiler's generated OstTraceDefinitions.h (not in the
# release): our traces-off stand-in, after every real include dir -- toolchain/shims/ost_off
OST_OFF = ["-I", os.path.join(OS_ROOT, "toolchain/shims/ost_off")]   # -I: ahead of casefix, which links a package's own (tracing-on) copy
# generated headers the release carries only in its API reference export tree (featureuids.h, from
# featuredatabase.xml): searched LAST, after every real include dir
API_REF_LAST = ["-idirafter", os.path.join(SYMBIAN, "symbian-2010/oss.API_REF.Public_API/epoc32/include")]
# APIs the release uses but never shipped (Music Player CR keys, the VoIP RCSE registry): OUR
# reconstructions, searched LAST -- toolchain/shims/unreleased_inc
UNRELEASED_INC = ["-idirafter", os.path.join(OS_ROOT, "toolchain/shims/unreleased_inc")]
# feature ids the RCL_3 input-method code uses that no 2010 header defines (a later S^3 feature list):
# OUR ids, outside every range features.dat knows, so FeatureManager::FeatureSupported() is false --
WEBKIT_NOT_UNIX = ["-U__unix__", "-U__unix", "-Uunix", "-U__linux__", "-U__linux", "-Ulinux", "-U__gnu_linux__"]
UNRELEASED_FEATURES = ["KFeatureIdFfVirtualFullscrPortraitQwertyInput=0x7FFF0001",
                       "KFeatureIdFfCameraLongKeyPress=0x7FFF0002"]
_XDOM = dict(flags=["-I" + os.path.join(EPOC32_ROOT, "include/xml/dom"), _KEEPNULL], defs=["__SOFTFP=", "__declspec(x)="])
_WRTXML = dict(_XDOM, flags=_XDOM["flags"] + [a for d in ("stdapis/libxml2", "xml/utils") for a in ("-idirafter", os.path.join(EPOC32_ROOT, "include", d))])


COMPONENTS = {
    # --- tier 1: euser-only leaves ------------------------------------------
    "palette": dict(mmp=f"{G}/graphicsdeviceinterface/colourpalette/group/palette.mmp"),
    "lookuptable": dict(mmp=f"{G}/graphicsdeviceinterface/gdi/group/lookuptable.mmp"),
    "linebreak": dict(mmp=f"{TXT}/fontservices/textbase/group/LineBreak.mmp"),
    # --- tier 2 ---------------------------------------------------------------
    # prelude's __builtin_offsetof rejects; its per-TU switch gives the classic 0x1000 form (= ARM's)
    "estor": dict(mmp=f"{PDATA}/persistentstorage/store/BMAKE/ESTOR.MMP", x32_flags=["-DPORT_FOFF_DYNAMIC"]),
    "bsul": dict(mmp=f"{OSSRV}/lowlevellibsandfws/apputils/bsul/group/bsul.mmp"),
    "centralrepository": dict(
        mmp=f"{PDATA}/persistentstorage/centralrepository/group/cenrepcli.mmp"),
    # --- tier 3 ---------------------------------------------------------------
    "bafl": dict(mmp=f"{OSSRV}/lowlevellibsandfws/apputils/group/BAFL.MMP"),
    "ecompatchdata": dict(
        mmp=f"{OSSRV}/lowlevellibsandfws/pluginfw/Framework/MMPFiles/EComPatchData.mmp"),
    "ecom": dict(mmp=f"{OSSRV}/lowlevellibsandfws/pluginfw/Framework/MMPFiles/ECom.mmp"),
    # --- tier 4: graphics -------------------------------------------------------
    "gdi": dict(mmp=f"{G}/graphicsdeviceinterface/gdi/group/GDI.MMP"),
    "fntstr": dict(mmp=f"{TXT}/fontservices/fontstore/group/FNTSTORE.MMP"),
    # rtti: aknicon dynamic_casts CFbsBitmap* -> its vtable needs the typeinfo slot
    "fbscli": dict(mmp=f"{G}/fbs/fontandbitmapserver/group/FBSCLI.MMP", rtti=True),
    # the FreeType font rasteriser (ECom plug-in FBSERV loads) + the release's reference TrueType fonts (DejaVu,
    # what the Symbian Foundation's public "Sirocco" ROM carried: referencefonts/group/reffonts_for_sirocco.iby) --
    "freetype": dict(mmp=f"{TXT}/fontservices/freetypefontrasteriser/group/FREETYPE.MMP", cdriver=True,
                     # ascender gave descent <= 0 and every label baseline a few pixels low)
                     drop=["ftrast2.cpp"], extra=["toolchain/shims/freetype_board/FTRAST2.CPP"]),
    "fbserv": dict(mmp=f"{G}/fbs/fontandbitmapserver/group/FBSERV.MMP"),
    # variant picks (the *_switch.mmp shims are emulator-only): generic screen
    # driver, non-GCE bitgdi, software sgresource, the stub extended-bitmap rasterizer
    "scdv": dict(mmp=f"{G}/graphicsdeviceinterface/screendriver/group/generic.mmp",
                 rom="scdv.dll"),
    "bitgdi": dict(mmp=f"{G}/graphicsdeviceinterface/bitgdi/group/BITGDI.MMP",
                   rom="bitgdi.dll"),
    # window server CFbsScreenDevice::GetSurface / (Set)DeviceOrientation panic BITGDI 5 in the non-GCE one. Same
    # ordinals (the shared BITGDI2.DEF); staged as bitgdi.dll in an NGA=1 ROM (NGA_TWINS "bitgdi").
    "bitgdi_nga": dict(mmp=f"{G}/graphicsdeviceinterface/bitgdi/group/bitgdi_gce.mmp", nga=True,
                       rom="bitgdi_nga.dll", rom_as="bitgdi.dll"),
    # OURS: null provider (real ordinal table) -- see toolchain/shims/sgresource_null.cpp
    "sgresource": dict(
        mmp=f"{G}/graphicsresourceservices/graphicsresourceimplementation/group/sgresource_sw.mmp",
        rom="sgresource.dll", nosrc=True, extra=["toolchain/shims/sgresource_null.cpp"],
        **{"def": f"{G}/graphicsresourceservices/graphicsresourceinterface/eabi/sgresourceu.def"}),
    # OURS: null provider -- see toolchain/shims/surfaceupdateclient_null.cpp
    "surfaceupdateclient": dict(
        mmp=f"{G}/graphicscomposition/surfaceupdate/group/surfaceupdateclient.mmp",
        nosrc=True, extra=["toolchain/shims/surfaceupdateclient_null.cpp"],
        **{"def": f"{G}/graphicscomposition/surfaceupdate/eabi/surfaceupdateclientU.def"}),
    # the extended-bitmap rasterizer = S60's SvgtFbsRasterizer (NVG icons -> nvgdecoder_sw -> the
    # software OpenVG through VGI). Was FBS's own stubrasterizer.mmp until the vector stack built.
    "fbsrasterizer": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/uigraphics/AknIcon/SvgtFbsRasterizer/group/SvgtFbsRasterizer.mmp",
                          rom="fbsrasterizer.dll", defs=["__declspec(x)="] + (["PHOREPOC_RASTER_GPU"] if os.environ.get("RASTER_GPU") == "1" else []),
                          # OURS bridge: the frozen ordinal 1 CFbsRasterizer::New -> CSvgtFbsRasterizer::New
                          # (+ the release source as a board copy: missing-pieces.md "Open", the colour-icon fill)
                          drop=["svgtfbsrasterizer.cpp"], libs=["efsrv.lib"],   # efsrv: the persistent icon cache
                          extra=["toolchain/shims/svgtfbsrasterizer/factory.cpp",
                                 "toolchain/shims/svgtfbsrasterizer/SvgtFbsRasterizer.cpp"],
                          # every NVG icon + skin item then drew as nothing. Now the real one, as ARM.
                          **{"def": f"{G}/fbs/fontandbitmapserver/eabi/fbsrasterizeru.def"}),
    # --- tier 5: window server (non-NGA = CFbsScreenDevice/SCDV direct) ----------
    "ektran": dict(mmp="oss.FCL.sf.os.kernelhwsrv/kernel/eka/ewsrv/ektran.mmp"),
    "ws32": dict(mmp=f"{G}/windowing/windowserver/group/w32_nonnga.mmp", rom="ws32.dll",
                 rtti=True,           # CWsBitmap : CFbsBitmap
                 # the non-NGA TAdvancedPointerEvent copy ctor/operator= are dummies -> copies of pointer events are
                 # garbage (Avkon's gesture framework: no taps); the OURS copy does what the NGA client does
                 drop=["advancedpointerevent.cpp"], extra=["toolchain/shims/ws32_board/advancedpointerevent.cpp"]),
    "wsgraphicdrawer": dict(
        mmp=f"{G}/windowing/windowserver/group/wsgraphicdrawer_nonnga.mmp",
        rom="wsgraphicdrawer.dll"),
    # (osbwin32 = emulator-only off-screen window; wserv imports nothing from it on ARM)
    # heap_max: EPOCHEAPSIZE says 3 MB, but here WSERV's own heap hosts the software OpenVG that rasterises
    # in CSvgtFbsRasterizer::RenderBitmapL and stayed white squares)
    "wserv": dict(mmp=f"{G}/windowing/windowserver/group/openwfc/wserv_nonnga.mmp", rom="wserv.exe",
                  heap_max=0x1000000,
                  drop=["INIFILE.CPP"], extra=["toolchain/shims/wserv_board/inifile_board.cpp"], libs=["centralrepository.lib"]),
    # WSERV's standard ECOM plug-ins: std render stage, flicker buffer, fader
    "wsstdplugin": dict(mmp=f"{G}/windowing/windowserver/group/stdplugin_nonnga.mmp",
                        rom="2001b70b.dll"),
    # wserv, wsgraphicdrawer, wsstdplugin, remotegc, sgresource, surfaceupdateclient -- are left out instead). A twin
    # builds to its own image file (rom=<name>_nga.<ext>) so building it never overwrites the non-NGA image the current
    # ROM ships; `rom_as` is the name the NGA ROM stages it under. Clients import by LIBRARY stem, so S60 code keeps
    # resolving to the twin's key (ws32 -> "ws32.dll") -- safe because both twins are frozen by the SAME .def.
    # Everything below is the real 2010 source, unmodified, built from its real .mmp.
    "surfacemanager": dict(
        mmp="oss.FCL.sf.adapt.NokiaGraphics/graphicscompositionref/surfacemgrref/group/surfacemanager.mmp", nga=True,
        **{"def": f"{G}/graphicshwdrivers/surfacemgr/eabi/surfacemanageru.def"}),
    "surfaceupdateserver": dict(mmp=f"{G}/graphicscomposition/surfaceupdate/group/surfaceupdateserver.mmp", nga=True),
    "surfaceupdateclient_nga": dict(
        mmp=f"{G}/graphicscomposition/surfaceupdate/group/surfaceupdateclient.mmp", nga=True,
        rom="surfaceupdateclient_nga.dll", rom_as="surfaceupdateclient.dll"),
    "wfcinterop": dict(mmp=f"{G}/graphicscomposition/openwfsupport/group/openwfc_interop.mmp", nga=True,
                       rom="wfcinterop.dll", **{"def": f"{G}/graphicscomposition/openwfsupport/eabi/WFCinteropu.def"},
                       drop=["contentupdateproxy.cpp", "streammap.cpp"],
                       extra=["toolchain/shims/wfcinterop_board/contentupdateproxy.cpp", "toolchain/shims/wfcinterop_board/streammap.cpp"],
                       flags=["-include", "e32debug.h"]),
    # the Khronos OpenWF-C reference compositor (C, over P.I.P.S. libc/libpthread/libm)
    "libwfc": dict(mmp=f"{G}/graphicscomposition/openwfcompositionengine/group/openwfc.mmp", nga=True,
                   rom="libWFC.dll", cdriver=True, allow_wsd=True, static_ctors=True,
                   # wfcSetContextAttribi traced (src/nga/libwfc/phtrace_wfc.cpp defines the exported name)
                   defs=["__SOFTFP=", "wfcSetContextAttribi=ph_real_wfcSetContextAttribi", "wfcSetElementAttribiv=ph_real_wfcSetElementAttribiv", "wfcSetElementAttribfv=ph_real_wfcSetElementAttribfv"],
                   vfp=True,
                   # OURS owfnotifications.cpp: a source update wakes the composer thread (the 2010 one polled every
                   drop=["owfnotifications.cpp"], extra=["src/nga/libwfc/owfnotifications.cpp", "src/nga/libwfc/phtrace_wfc.cpp"],
                   **{"def": f"{G}/graphicscomposition/openwfcinterface/eabi/libWFCu.def"}),
    # (the mmp's ARM TARGET is openwfc_ri_displaychannel.dll; libWFC links it as openwfc_ri_display.lib)
    # for the PREVIOUS one, not itself, so the next compose overlaps the vblank wait (3 composition buffers in the
    # kernel, src/phorepoc-arm/a64/display/lddinc/display.h). The 2010 mmp is {G}/graphicscomposition/
    # openwfc_ri_displaychannel/group/openwfc_ri_displaychannel.mmp.
    "openwfc_ri_display": dict(
        mmp=os.path.join(OS_ROOT, "src/nga/openwfc_ri_display/openwfc_ri_displaychannel.mmp"), nga=True,
        rom="openwfc_ri_displaychannel.dll",
        **{"def": f"{G}/graphicscomposition/openwfcompositionengine/eabi/openwfc_ri_displayu.def"}),
    "eglsynchelper": dict(mmp=f"{G}/graphicscomposition/openwfsupport/group/eglsynchelper.mmp", nga=True, cdriver=True),
    # libEGL.dll (the name every client links; the window server's eglsynchelper too): OUR frozen-ABI front over
    # phgl.dll, the Mesa backend (GPU plan G2/G4: EGL + GLES 2.0/1.1 on the Mali-400 through lima, or softpipe).
    "libegl": dict(mmp=os.path.join(OS_ROOT, "src/phgl/libegl.mmp"), nga=True, rom="libEGL.dll", defs=["__declspec(x)="],
                   **{"def": f"{G}/egl/eglinterface/eabi/libegl14U.def"}),
    # the 2010 reference implementation it replaced (reusable syncs only), kept buildable for comparison; never
    # staged (its rom name is not the one clients link)
    "libegl_ref": dict(mmp=f"{G}/egl/eglrefimpl/group/eglref.mmp", nga=True, rom="libegl_ref.dll",
                       **{"def": f"{G}/egl/eglrefimpl/eabi/lib_ref_eglu.def"}),
    "libglesv2": dict(mmp=os.path.join(OS_ROOT, "src/phgl/libglesv2.mmp"), nga=True, rom="libGLESv2.dll",
                      defs=["__declspec(x)="],
                      **{"def": f"{G}/opengles/openglesinterface/eabi/libglesv2u.def"}),
    "libglesv1_cm": dict(mmp=os.path.join(OS_ROOT, "src/phgl/libglesv1_cm.mmp"), nga=True, rom="libGLESv1_CM.dll",
                         defs=["__declspec(x)="],
                         **{"def": f"{G}/opengles/openglesinterface/eabi/libglesv1_cm11u.def"}),
    # libGLES_CM.dll: the older S60 GLES 1.x DLL (gl* -> phgl, egl* -> libEGL), linked by SDK games (King Oddball)
    "libgles_cm": dict(mmp=os.path.join(OS_ROOT, "src/phgl/libgles_cm.mmp"), nga=True, rom="libGLES_CM.dll",
                       defs=["__declspec(x)="],
                       **{"def": f"{G}/opengles/openglesinterface/eabi/opengles11u.def"}),
    # never linked. ROM_TAIL=phgl while it is being worked on (the delta upload).
    "phgl": dict(mmp=os.path.join(OS_ROOT, "src/phgl/phgl.mmp"), nga=True, vfp=True, static_ctors=True, allow_wsd=True,
                 **{"def": os.path.join(OS_ROOT, "src/phgl/gen/phgl.def")},
                 arm_archives=[f".mesa-build/arm/{a}.a" for a in (
                     "libegl_symbian", "libgles2_entry", "libgles1_entry", "libmesa", "libglapi", "libsoftpipe",
                     "liblima", "libpanfrost_shared", "libgallium", "libgalliumvl_stub",
                     "libglsl", "libglcpp", "libglsl_util", "libnir", "libcompiler", "libmesa_util",
                     "libmesa_util_c11", "libblake3", "libws_null")]),
    "directgdi": dict(mmp=f"{G}/graphicsdeviceinterface/directgdi/group/directgdi.mmp", nga=True),
    "directgdiadapter": dict(
        mmp=f"{G}/graphicsdeviceinterface/directgdiadaptation/group/directgdiadapter_sw.mmp", nga=True,
        rom="directgdiadapter.dll", **{"def": f"{G}/graphicsdeviceinterface/directgdiinterface/eabi/directgdiadapteru.def"},
        # extended bitmaps (S60's NVG icons + skin graphics) drawn, not refused: BitGdi flattens them
        drop=["swdirectgdibitblt.cpp", "swdirectgdidrawbmp.cpp", "swdirectgdimain.cpp"],
        extra=["toolchain/shims/directgdi_sw/swdirectgdibitblt.cpp", "toolchain/shims/directgdi_sw/swdirectgdidrawbmp.cpp",
               "toolchain/shims/directgdi_sw/swdirectgdimain.cpp", "toolchain/shims/directgdi_sw/swdgdiflatten.cpp"],
        libs=["bitgdi.lib"], allow_wsd=True),
    "graphicsresource": dict(mmp=f"{G}/graphicsresourceservices/graphicsresource/group/graphicsresource.mmp", nga=True),
    "graphicsresourceadapter": dict(
        mmp=f"{G}/graphicsresourceservices/graphicsresourceadaptation/group/graphicsresourceadapter_sw.mmp", nga=True,
        rom="graphicsresourceadapter.dll",
        **{"def": f"{G}/graphicsresourceservices/graphicsresource/eabi/graphicsresourceadapteru.def"}),
    # the REAL software sgresource (RSgImage over SurfaceManager chunks), retiring toolchain/shims/sgresource_null.cpp
    "sgresource_nga": dict(
        mmp=f"{G}/graphicsresourceservices/graphicsresourceimplementation/group/sgresource_sw.mmp", nga=True,
        rom="sgresource_nga.dll", rom_as="sgresource.dll",
        **{"def": f"{G}/graphicsresourceservices/graphicsresourceinterface/eabi/sgresourceu.def"}),
    "ws32_nga": dict(mmp=f"{G}/windowing/windowserver/group/W32_nga.MMP", nga=True, rtti=True,
                     rom="ws32_nga.dll", rom_as="ws32.dll"),
    "wsgraphicdrawer_nga": dict(mmp=f"{G}/windowing/windowserver/group/WSGRAPHICDRAWER_nga.MMP", nga=True,
                                rom="wsgraphicdrawer_nga.dll", rom_as="wsgraphicdrawer.dll"),
    "wserv_nga": dict(mmp=f"{G}/windowing/windowserver/group/openwfc/WServ_nga.MMP", nga=True,
                      rom="wserv_nga.exe", rom_as="wserv.exe", heap_max=0x1000000,
                      arm_heap_max=0x1000000,
                      **({"drop": ["WSTOP.CPP", "INIFILE.CPP", "CLIENT.CPP", "WINBASE.CPP", "POINTER.CPP", "EVENT.CPP"], "extra": [".wsdbg-build/WSTOP.CPP", "toolchain/shims/wserv_board/inifile_board.cpp",
                                                                          "toolchain/shims/wserv_board/client_nga.cpp", "toolchain/shims/wserv_board/winbase_nga.cpp", "toolchain/shims/wserv_board/pointer_nga.cpp", "toolchain/shims/wserv_board/event_nga.cpp"]}
                         if os.environ.get("WSERV_DBG") == "1" else
                         {"drop": ["INIFILE.CPP", "CLIENT.CPP", "WINBASE.CPP", "POINTER.CPP", "EVENT.CPP"], "extra": ["toolchain/shims/wserv_board/inifile_board.cpp", "toolchain/shims/wserv_board/client_nga.cpp", "toolchain/shims/wserv_board/winbase_nga.cpp", "toolchain/shims/wserv_board/pointer_nga.cpp", "toolchain/shims/wserv_board/event_nga.cpp"]}),
                      defs=["PH_WS_NGA"] + (["LOG_WSERV_EVENTS"] if os.environ.get("WSERV_EVLOG") == "1" else []), libs=["centralrepository.lib"],
                      flags=["-finput-charset=ISO-8859-1"] if os.environ.get("WSERV_EVLOG") == "1" else []),   # (EVQUEUE.CPP's trace texts are Latin-1)
    # the NGA standard render stage: displayrenderstage -> libWFC (+ the fb/debugbar stages); replaces 2001b70b
    "wsstdplugin_nga": dict(mmp=f"{G}/windowing/windowserverplugins/openwfc/group/stdplugin.mmp", nga=True,
                            rom="10286378.dll",
                            drop=["openwfcwrapper.cpp"], extra=["toolchain/shims/wsstdplugin_board/openwfcwrapper.cpp"]),
    # "keyrouter.dll" (wsini KEYROUTERPLUGIN overrides) and LEAVES without it -- the NGA boot's EWsPanicFailedToInitialise
    "keyrouter": dict(mmp=f"{G}/windowing/windowserverplugins/keyeventrouting/group/keyeventrouting.mmp", nga=True,
                      drop=["keyrouter.cpp"], extra=["toolchain/shims/keyrouter_board/keyrouter.cpp"],   # no combined power/end key
                      flags=["-iquote", _resolve_src(f"baseline/symbian-2010/{G}/windowing/windowserverplugins/keyeventrouting/src")]),
    "remotegc_nga": dict(mmp=f"{G}/windowing/windowserver/group/RemoteGc_nga.mmp", nga=True,
                         rom="remotegc_nga.dll", rom_as="remotegc.dll"),
    # switcher -- WSERV's own default fader lightens: 128 / 255). Selected by wsini FADER + PLUGINS EXTENDEDFADER
    "aknextendedfader": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/AknExtendedFader/group/aknextendedfader_armv5.mmp",
                             rom="aknextendedfader_armv5.dll",
                             x32_flags=["-UAKNEXTENDEDFADER_UREL_TRACES"]),   # its mmp turns RDebug tracing on (every fade)
    # the ECOM server itself (+ its domain-manager client dependency)
    "domaincli": dict(
        mmp="oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/domainmgr/group/domaincli.mmp"),
    # the Domain Manager + the SSM startup hierarchy (KDmHierarchyIdStartup = 2): servers
    # such as viewsrv wait on it for the system state to reach "Normal"
    "domainsrv": dict(
        mmp="oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/domainmgr/group/domainsrv.mmp"),
    "domainpolicy": dict(   # the power hierarchy (id 1) domainsrv loads at construction
        mmp="oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/domainmgr/group/domainpolicy.mmp"),
    "domainpolicy2": dict(
        mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstatemgr/dompolicy/group/domainpolicy2.mmp"),
    "ecomserver": dict(
        mmp=f"{OSSRV}/lowlevellibsandfws/pluginfw/Framework/MMPFiles/EComServer.mmp"),
    # minigui: the text console implemented ON the graphical window server
    "econswserv": dict(mmp=f"{G}/windowing/windowserver/minigui/group/econswserv.mmp"),
    # --- central repository server + its leaves ------------------------------------
    "abclient": dict(mmp=f"{OSSRV}/genericservices/activebackupclient/group/abclient.mmp"),
    "bsulinifile": dict(mmp=f"{OSSRV}/lowlevellibsandfws/apputils/bsul/group/bsulinifile.mmp"),
    "charconv": dict(mmp=f"{TXT}/charconvfw/charconv_fw/group/charconv.mmp"),
    "sysutil": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/sysutil/group/sysutil.mmp"),
    "centralrepositorysrv": dict(
        mmp=f"{PDATA}/persistentstorage/centralrepository/group/cenrepsrv.mmp"),   # bring-up tip: defs=["CENTREP_TRACE"] names a corrupt/missing repository on serial
    # --- tier 6: application framework (control environment) -----------------------
    "numberconversion": dict(mmp=f"{TXT}/textrendering/numberformatting/group/NumberConversion.mmp"),
    "ssmcmn": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstatemgr/cmn/group/ssmcmn.mmp"),
    "viewsrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/viewserver/group/VIEWSRV.MMP"),
    "viewcli": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/viewserver/group/VIEWCLI.MMP"),
    "egul": dict(mmp=f"{CUI}/lafagnosticuifoundation/uigraphicsutils/group/EGUL.MMP"),
    "cone": dict(mmp=f"{CUI}/lafagnosticuifoundation/cone/group/CONE.MMP",
                 drop=["COEPRIV.CPP"], extra=["toolchain/shims/cone_diag/COEPRIV.CPP"]),
    # resource-only: the error-resolver texts (errorres.rsg is included by eikcore & co)
    "errorres": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/commonengineresources/group/commonengineresources.mmp",
                     rsc_only=True,
                     # ...and at the path the S60 CTextResolver (commonengine) opens; same .rsg, so the same file
                     rom_data=[r"resource\errors\errorres.rsc=emitter/rsc/resource/errorres.rsc"]),
    # --- tier 7: Uikon + Avkon (S60 classicui) and their support libraries ----------
    "apmime": dict(mmp="oss.FCL.sf.os.ossrv/genericservices/mimerecognitionfw/apmime/APMIME.MMP"),
    "apparc": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group/APPARC.MMP",
                   drop=["apacmdln.cpp"], extra=["toolchain/shims/apparc_board/APACMDLN.CPP"],
                   flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/apparc")]),
    "apgrfx": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group/APGRFX.MMP"),
    "apfile": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group/APFILE.MMP"),
    "aplist": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group/APLIST.MMP"),
    "apserv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group/APSERV.MMP"),
    "field": dict(mmp="oss.FCL.sf.os.textandloc/textrendering/texthandling/group/FIELD.MMP"),
    # TXTFMSTM.CPP: DoInvariantCheck() relied on RVCT's operand order (`streamLoc+Length(streamLoc,..)`), fixed copy in etext_gcc
    "etext": dict(mmp="oss.FCL.sf.os.textandloc/textrendering/texthandling/group/ETEXT.MMP",
                  drop=["TXTFMSTM.CPP"], extra=["toolchain/shims/etext_gcc/TXTFMSTM.CPP"]),
    "tagma": dict(mmp="oss.FCL.sf.os.textandloc/textrendering/textformatting/group/TAGMA.MMP"),
    "form": dict(mmp="oss.FCL.sf.os.textandloc/textrendering/textformatting/group/FORM2.MMP"),
    "clock": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/lafagnosticuifoundation/clockanim/group/CLOCK.MMP"),
    "bmpanim": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/lafagnosticuifoundation/bmpanimation/group/BmpAnim.mmp"),
    "fepbase": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/fep/frontendprocessor/group/FEPBASE.MMP"),
    "eiksrvc": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/commonuisupport/uikon/group/EIKSRVC.MMP"),
    "cdlengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/cdl/CdlEngine/group/CdlEngine.mmp"),
    "aknlayout2": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/aknlayout2/group/AknLayout2.mmp"),
    "aknlayout2scalable": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/aknlayout2/group/AknLayout2Scalable.mmp"),
    "fontprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/fontsupport/fontprovider/Group/FontProvider.mmp"),
    "fontutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/fontsupport/fontutils/FontUtils/group/FontUtils.mmp"),
    "commonengine": dict(mmp="oss.FCL.sf.os.ossrv/genericservices/s60compatibilityheaders/commonengine/group/commonengine.mmp"),
    "sysagt2svr": dict(mmp="oss.FCL.sf.os.ossrv/genericservices/systemagent/group/SysAgt2Svr.mmp"),
    "sysagt2cli": dict(mmp="oss.FCL.sf.os.ossrv/genericservices/systemagent/group/SysAgt2Cli.mmp"),
    "cenrepnotifhandler": dict(mmp="oss.FCL.sf.os.persistentdata/persistentstorage/centralrepository/cenrepnotifierhandler/group/cenrepnotifhandler.mmp"),
    "akncapserverclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/AknCapServer/group/AknCapServerClient.mmp"),
    "aknnotify": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/notifsrc/aknNotify.mmp"),
    "aknicon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/uigraphics/AknIcon/group/AknIcon.mmp",
                    ),
    "aknskins": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/AknSkins.mmp", defs=["AKNS_TRACING_ENABLED_ERROR"]),
    "eikcoctllaf": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/uiklaf/Group/EikCoCtlLaf.MMP"),
    # its DEFFILE arm is `#if defined(ARMCC)`; the def is exported under epoc32/include
    "uiklaf": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/uiklaf/Group/UIKLAF.MMP",
                   **{"def": os.path.join(EPOC32_ROOT, "include/uiklaf/private/eabi/uiklafu.def")}),
    "aknlocalskinlib": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/aknlocalskinlib.mmp"),
    "aknsoomplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/aknsoomplugin.mmp"),
    "aknswallpaperplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/aknswallpaperplugin.mmp"),
    "aknskinsrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/AknSkinSrv.mmp", defs=["AKNS_TRACING_ENABLED_ERROR"]),
    "aknskinrenderlib": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/AknSkinRenderLib.mmp", defs=["AKNS_TRACING_ENABLED_ERROR"],
                             flags=["-U__MARM_ARMV5__"],
                             x32_flags=["-D__NAKED__="]),   # x32: the non-ARM branch is an empty stub; cpudefs.h defines __NAKED__ only for MSVC/__X86GCC__  # RCL_3 fx_asm_versions.h keys its RVCT __asm paths off __MARM_ARMV5__ (master: __ARMCC__) -> take the C paths,
    "eikcdlg": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/EikStd/cdlgsrc/EIKCDLG.MMP"),
    "eikcore": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/commonuisupport/uikon/group/EIKCORE.MMP",
                    x32_flags=["-DPORT_FOFF_DYNAMIC"],
                    # root, so dynamic_cast<CAknAppUi*>(CEikAppUi*) across DLLs works (SDK apps' orientation code; King Oddball)
                    import_typeinfo=True),    # eikprivate.cpp: _FOFF(TUnlimitedFileName, iBuf[maxLength]), a run-time index
    "eikcoctl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/EikStd/coctlsrc/EIKCOCTL.MMP",
                     x32_flags=["-Dabs(x)=__builtin_abs(x)"]),
    "eikctl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/eikctl/group/eikctl.mmp"),
    "eikdlg": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/EikStd/dlgsrc/EIKDLG.MMP"),
    # AknEcs.cpp = OURS one-line stem of this snapshot's file (toolchain/shims/stems; the stemkit's
    # own AknEcs.cpp stem targets a newer classicui): no phone server here and the real phoneclient
    # panics 'PhSrvStartUp' -- run the emergency-call detector with a null CPhCltEmergencyCall
    "avkon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/group/avkon.mmp",
                  drop=["aknecs.cpp", "AknAppUi.cpp"], extra=["toolchain/shims/stems/AknEcs.cpp", "src/phorepoc/avkon_trace/AknAppUi.cpp"],
                  import_typeinfo=True),   # the AppUi typeinfo chain: CEikAppUi's real typeinfo from eikcore (see eikcore)
    "akntransitionutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/AknTransitionUtils/group/akntransitionutils.mmp"),
    # Open Dynamics Engine (in classicui!): aknphysics -- every Avkon menu/list kinetic scroll -- creates
    # an ODE world; + the classic C library it is written against
    "estlib": dict(mmp="oss.FCL.sf.os.ossrv/genericopenlibs/cstdlib/BMMP/ESTLIB.MMP", cdriver=True,
                   flags=["-fno-tree-loop-distribute-patterns", "-fno-builtin"],       # see libc
                   x32_drop=["jmp_marm.cia"], x32_extra=["tools/phorepoc-x86/port/x64user/xu_setjmp.cpp"]),
    # P.I.P.S. / Open C (openenvcore): the REAL libc family -- matrixmenu's libxml2 and all of
    # Track S4 (Qt) sit on it. EPOCALLOWDLLDATA throughout (ROM-DLL WSD). ewsd is emulator-only.
    # __SOFTFP: on ARM the compiler prefix (gcce.h) supplies it, not the stdapis headers
    "libm": dict(mmp=_OENV + "libm/group/libm.mmp", cdriver=True, allow_wsd=True, vfp=True, defs=["__SOFTFP="]),
    # ftable.cpp + inc/sysif.h: OURS copies -- the fd table as a flat, pre-reserved array (the segmented one's unlocked
    "backend": dict(mmp=_OENV + "backend/group/backend.mmp", cdriver=True, allow_wsd=True, static_ctors=True, defs=["__SOFTFP="],
                    incs_first=["toolchain/shims/backend_board/inc"], drop=["ftable.cpp"], extra=["toolchain/shims/backend_board/ftable.cpp"]),
    # mutex.cpp + condvar.cpp: OURS copies fixing the static-initializer race (_InitOnce / _staticCondInit read the state
    "libpthread": dict(mmp=_OENV + "libpthread/group/libpthread.mmp", cdriver=True, allow_wsd=True, static_ctors=True, defs=["__SOFTFP="],
                       drop=["mutex.cpp", "condvar.cpp"], extra=["toolchain/shims/libpthread_board/mutex.cpp", "toolchain/shims/libpthread_board/condvar.cpp"]),
    # Open C++ (STLport; ossrv/stdcpp): the STL for release code that uses it -- first user the Symbian^4 system tone
    # server (std::map). Over P.I.P.S. as its mmp says
    "libstdcpp": dict(mmp="oss.FCL.sf.os.ossrv/stdcpp/group/libstdcpp.mmp", cdriver=True, allow_wsd=True,
                      static_ctors=True, rtti=True, defs=["__SOFTFP="], incs_first=STLPORT_X32,
                      # (-fkeep-inline-functions: its WSD accessors are `inline` AND exported -- RVCT/GCCE's dllexport
                      # emitted an out-of-line copy, ELF GCC only where not inlined -> unresolved in other TUs)
                      x32_flags=STLPORT_X32_FLAGS + ["-fkeep-inline-functions"], own_new=True,
                      extra=["toolchain/shims/stdcpp_board/stdcpp_x32.cpp"]),
    # Standard C++ (genericopenlibs/cppstdlib): stdnew.dll = the STDCPP target type's operator new/delete + std::exception,
    "stdnew": dict(mmp="oss.FCL.sf.os.ossrv/genericopenlibs/cppstdlib/group/stdnew.mmp", cdriver=True, rtti=True,
                   defs=["__SOFTFP="], own_new=True),   # (typeinfo std::bad_alloc, ordinal 19: not defined by its sources -- stub)
    # libstdcppv5.dll (UID3 0x20017610): the Standard C++ library (STLport 5) SDK apps built with the STDCPP plug-in import
    "libstdcppv5": dict(mmp="oss.FCL.sf.os.ossrv/genericopenlibs/cppstdlib/group/libstdcpp.mmp", cdriver=True, allow_wsd=True,
                        static_ctors=True, rtti=True, defs=["__SOFTFP="], own_new=True),
    "sqlite3": dict(mmp="oss.FCL.sf.os.persistentdata/persistentstorage/sqlite3api/GROUP/sqlite3.mmp", cdriver=True, allow_wsd=True,
                    defs=["__SOFTFP="]),
    "libc": dict(mmp=_OENV + "libc/group/libc.mmp", cdriver=True, allow_wsd=True, static_ctors=True, defs=["__SOFTFP="],
                 # a C library must not be "optimised" into calls to itself: the loop-idiom pass turned
                 # memset()'s own loop into a memset call (infinite recursion), and builtin folding
                 # turned calloc()'s malloc+memset body into a calloc call (infinite loop)
                 flags=["-fno-tree-loop-distribute-patterns", "-fno-builtin"],
                 # (ARM) getenv.c's `inline __findenv` is never emitted by GCC -> the libc export slot is an unimpl stub and
                 # setenv() dies EUSRNIMP: toolchain/shims/libc_findenv/getenv.c compiles it with `inline` defined away
                 drop=["getenv.c"], extra=["toolchain/shims/libc_findenv/getenv.c"],
                 x32_drop=["jmp_marm.cia", "vfscanf.c"],
                 x32_extra=["tools/phorepoc-x86/port/x64user/xu_setjmp.cpp",
                            "tools/phorepoc-x86/port/x64user/libc_vfscanf_x32.c"]),
    # the comms/time client DLLs backend + libc import (thin RSessionBase clients: their servers
    # are absent, so Connect() fails cleanly instead of hitting a loud unimpl stub)
    "rpipe": dict(mmp="oss.FCL.sf.os.kernelhwsrv/kernel/eka/euser/rpipe.mmp"),
    "esock": dict(mmp="oss.FCL.sf.os.commsfw/datacommsserver/esockserver/group/ESOCK.MMP",
                  flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/foff_builtin.h")]),
    "insock": dict(mmp="oss.FCL.sf.os.networkingsrv/esockapiextensions/internetsockets/group/insock.mmp"),
    "tzclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/tzservices/tzserver/group/TimeZoneClient.mmp"),
    # the time-zone server (zone rules + DST from tzdb.dbz) + the SQL server its user-data store needs:
    "sqlite": dict(mmp=f"{PDATA}/persistentstorage/sql/GROUP/sqlite.mmp", cdriver=True),
    "sqlsrv": dict(mmp=f"{PDATA}/persistentstorage/sql/GROUP/SqlSrv.mmp"),
    "sqldb": dict(mmp=f"{PDATA}/persistentstorage/sql/GROUP/SqlDb.mmp"),
    "tzserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/tzservices/tzserver/group/TimeZoneServer.mmp"),
    "ode": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/ode/group/ode.mmp", allow_wsd=True),   # odeStaticData + joint vtables
    "aknphysics": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/aknphysics/group/aknphysics.mmp"),
    "gfxtrans": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/lafagnosticuifoundation/graphicseffects/group/GfxTransEffect.mmp"),
    "tactilefeedbackresolver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.hapticsservices/tactilefeedback/tactilefeedbackresolver/group/tactilefeedbackresolver.mmp",
        # tree skew: the source uses KProEngSilenceMode but this snapshot's profiles-services
        # ProfileEngineInternalCRKeys.h predates it (later releases: 0x80000202, after
        # KProEngMasterVolume/Vibra). An absent key just reads KErrNotFound = "not silent".
        defs=["KProEngSilenceMode=0x80000202u"]),
    "touchfeedback": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.hapticsservices/tactilefeedback/tactilefeedbackclient/group/touchfeedback.mmp"),
    "aknlistloadertfx": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/AknListLoaderTfx/group/aknlistloadertfx.mmp"),
    "aknpictograph": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/pictographs/AknPictograph/group/AknPictograph.mmp"),
    "hlplch": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uiutils/hlplch/group/hlplch.mmp"),
    "numbergrouping": dict(mmp="oss.FCL.sf.os.textandloc/textandlocutils/numbergrouping/group/NumberGrouping.mmp"),
    "findutil": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uiutils/Findutil/group/FindUtil.mmp"),
    # its polymorphic implementation: CFindUtil::ConstructL RLibrary::Loads it -- without it every
    "findutilimpl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uiutils/Findutil/group/FindUtilImpl.mmp"),
    "jplangutil": dict(mmp="oss.FCL.sf.os.textandloc/textandlocutils/jplangutil/group/jplangutil.mmp"),
    "etul": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/lafagnosticuifoundation/uigraphicsutils/group/etul.mmp"),
    "eiksrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/commonuisupport/uikon/group/eiksrv.mmp"),
    "featmgr": dict(mmp="oss.FCL.sf.os.persistentdata/featuremgmt/featuremgr/group/featmgr.mmp"),
    # OURS: minimal native WSERV client (absolute mmp path = outside baseline/)
    "wsclient": dict(mmp=os.path.join(OS_ROOT, "src/wsclient/wsclient.mmp")),
    "wsblit": dict(mmp=os.path.join(OS_ROOT, "src/wsblit/wsblit.mmp")),
    # the first OpenGL ES 2.0 program (GPU plan G2/G4): an ordinary GL client of libEGL.dll + libGLESv2.dll (until
    "glestri": dict(mmp=os.path.join(OS_ROOT, "src/glestri/glestri.mmp"), vfp=True, test=True, defs=["__declspec(x)="]),
    # the OpenGL ES 1.1 (fixed-function) twin: libGLESv1_CM.dll
    "gles1tri": dict(mmp=os.path.join(OS_ROOT, "src/gles1tri/gles1tri.mmp"), vfp=True, test=True, defs=["__declspec(x)="]),
    # --- system servers the UI framework talks to ------------------------------------
    "baksrv": dict(mmp=f"{OSSRV}/lowlevellibsandfws/apputils/group/Baksrv.mmp"),
    "baksrvs": dict(mmp=f"{OSSRV}/lowlevellibsandfws/apputils/group/Baksrvs.mmp"),
    "eikbackupsrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/commonuisupport/uikon/group/EIKBACKUPSRV.MMP"),
    # S60 layout data: PRE-GENERATED CDL instances (aknlayout2/generated), as ECOM plug-ins
    "aknlayout2adaptation": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/aknlayout2/group/AknLayout2Adaptation.mmp"),
    "aknlayout2hierarchy": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/aknlayout2/group/aknlayout2hierarchy.mmp"),
    "layoutinst": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/aknlayout2/group/../generated/layoutinst.mmp", rom="101fe2aa.dll"),
    "layoutinst_nhd4": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/aknlayout2/group/../generated/Nhd4_akn_app/layoutinst_nhd4.mmp", rom="2001cb93.dll"),
    "cdlserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/layouts/cdl/CdlServer/group/CdlServer.mmp"),
    # WSERV-side animation plug-ins (RAnimDll::Load by name)
    "clocka": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/lafagnosticuifoundation/clockanim/group/CLOCKA.MMP"),
    "bmpansrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/lafagnosticuifoundation/bmpanimation/group/BmpAnSrv.mmp"),
    "aknanimdll": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/animdllsrc/AknAnimDll.mmp"),
    "eiksrvui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/EikStd/srvuisrc/EIKSRVUI.MMP"),
    "aknnotifyplugin2": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/group/AknNotifyPlugin2.mmp"),
    "aknnotifyplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/AknNotifyPlugin_dll/group/AknNotifyPlugin.mmp"),
    "platformver": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/PlatformEnv/platformver/group/platformver.mmp"),
    "platformenv": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/PlatformEnv/group/PlatformEnv.mmp",
                        x32_drop=["driveinfopatchdata.cpp"], x32_extra=["toolchain/shims/driveinfo_board/driveinfopatchdata.cpp"],
                        **{"def": "oss.FCL.sf.os.devicesrv/commonservices/PlatformEnv/EABI/PlatformEnvCenRepU.DEF"}),
    "disknotifyhandler": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/PlatformEnv/disknotifyhandler/group/disknotifyhandler.mmp"),
    # Clock died EUSRNIMP in CDRMHelper::NewL(CCoeEnv&))
    # repository server, and the standard key storage it loads
    "rightsserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/RightsServer.mmp"),
    "dcfrepsrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DcfRepSrv.mmp"),
    "drmstdkeystorage": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DrmStdKeyStorage.mmp"),
    "wmdrmfileserverclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/wmdrm/wmdrmengine/wmdrmfileserver/client/group/wmdrmfileserverclient.mmp"),
    "lbs": dict(mmp="oss.FCL.sf.os.lbs/locationmgmt/locationcore/LbsClient/LbsApi/group/Lbs.mmp"),
    "eposindicator": dict(mmp="oss.FCL.sf.os.lbs/locationmgmt/locationcore/lbslocindicatorlib/group/epos_locationindicatorlibrary.mmp"),
    # the DRM helper's server (CDRMHelper::ConstructL connects to it)
    "drmhelperserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmhelper/drmhelperserver/group/DRMHelperServer.mmp"),
    "drmhelper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmhelper/group/Drmhelper.mmp",
                      **{"def": "oss.FCL.sf.mw.drm/omadrm/drmhelper/EABI/DRMHelperU.DEF"},
                      # -- the DRM engine clients it needs are built; Clock's datetime view calls NewL
                      x32_mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmhelper/group/Drmhelper.mmp", x32_real=True),
    # the Uikon library plug-in (KUikonLibraryUid): installs the AppUi factory, dialog
    # factories, status-pane resource -- without it CEikonEnv has no iAppUiFactoryArray
    "akninit": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/initsrc/akninit.mmp"),
    # Symbian's OWN null transition-effects adapter (no TFX engine on this board)
    "gfxtransadapter": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/lafagnosticuifoundation/graphicseffects/group/GfxTransAdapterStub.mmp",
                            rom="gfxtransadapter.dll"),
    "remotegc": dict(mmp=f"{G}/windowing/windowserver/group/remotegc_nonnga.mmp", rom="remotegc.dll"),
    # Content Access Framework (the skin server asks it whether a skin file is DRM-protected;
    # with no agent plug-ins everything resolves to the built-in F32 agent = plain files)
    "cafutils": dict(mmp="oss.FCL.sf.os.security/contentmgmt/contentaccessfwfordrm/group/CafUtils.mmp"),
    "caf": dict(mmp="oss.FCL.sf.os.security/contentmgmt/contentaccessfwfordrm/group/caf.mmp"),
    "f32agentui": dict(mmp="oss.FCL.sf.os.security/contentmgmt/contentaccessfwfordrm/group/f32agentui.mmp"),
    "f32agent": dict(mmp="oss.FCL.sf.os.security/contentmgmt/contentaccessfwfordrm/group/f32agent.mmp"),
    # OURS: null provider (real ordinal table) -- see toolchain/shims/c32_null/
    # C32: the REAL client half (CC_CLI.CPP: RCommServ/RComm -- P.I.P.S. backend embeds an RCommServ)
    # + OURS null StartC32 (c32_null.cpp) in place of CC_UTL.CPP's root-server launch; the server
    # half stays out (no comms subsystem on this board), so Connect() fails cleanly KErrNotFound.
    "c32": dict(mmp="oss.FCL.sf.os.commsfw/serialserver/c32serialserver/group/C32.MMP",
                drop=["cc_utl.cpp", "cs_man.cpp", "cs_port.cpp", "cs_ses.cpp", "cs_svr.cpp", "cs_time.cpp",
                      "cs_utl.cpp", "cs_v02.cpp", "cs_roles.cpp", "cs_thread.cpp", "cs_glob.cpp", "cs_log.cpp"],
                extra=["toolchain/shims/c32_null/c32_null.cpp"],
                x32_real=True),
    "alarmshared": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/commonappservices/alarmserver/Group/AlarmShared.mmp"),
    "alarmclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/commonappservices/alarmserver/Group/AlarmClient.mmp"),
    # the alarm server itself: eiksrvs starts it once akncapserver is up (EIKSRVUI.CPP
    # DoAlarmServerStartupL) and User::Invariant()s if it cannot
    "backuprestorenotification": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/commonappservices/backuprestorenotification/group/BackupRestoreNotification.mmp"),
    "ssmadaptationcli": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstatemgr/sus/group/ssmadaptationcli.mmp"),
    "alarmserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/commonappservices/alarmserver/Group/AlarmServer.mmp"),
    # SSM utility server: hosts !SusAdaptionSrv (the alarm server needs its RTC adaptation) +
    # Symbian's OWN reference adaptation plug-ins (the loader's fallback when no vendor ones exist)
    "ssmsuscli": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstatemgr/sus/group/ssmsuscli.mmp"),
    "ssmutilsrv": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstatemgr/sus/group/ssmutilsrv.mmp"),
    # (estart.cpp TFSStartup::StartSystem), so its mere presence flips the boot. File set = the release's ibys:
    # ssma.iby, ssmpolicyplugins.iby, ssmcompatibility.iby, ssmcmdlists.iby, ssmcustcmds/ssmutilityplugins/...
    # excluded it when domaincli did not exist yet). make_phorepoc_rom.sh uses this image for BOOT=ssm.
    "e32strt": dict(mmp="oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/fileserver/estart/estart.mmp", ssm=True,
                    drop=["estart.cpp"], extra=["toolchain/shims/estart_board/estart.cpp"],
                    flags=["-iquote", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/fileserver/estart")]),
    "sysstatemgr": dict(mmp=f"{_SSM}/systemstatemgr/ssm/group/ssmserver.mmp", ssm=True),
    "ssmcmd": dict(mmp=f"{_SSM}/systemstatemgr/cmd/group/ssmcmd.mmp", rtti=True, ssm=True),   # dynamic_cast (command factory)
    "ssmclecli": dict(mmp=f"{_SSM}/systemstatemgr/cle/group/clecli.mmp", ssm=True),   # static libs of sysstatemgr
    "ssmclesrv": dict(mmp=f"{_SSM}/systemstatemgr/cle/group/clesrv.mmp", ssm=True),
    "ssmmapperutility": dict(mmp=f"{_SSM}/ssmmapperutility/group/ssmmapperutility.mmp"),   # shared (syslangutil, ssmuiproviderdll): every ROM
    "ssmstartsafe": dict(mmp=f"{_SSM}/systemstatemgr/ss/group/ssmstartsafe.mmp", ssm=True),
    "ssmpolicybase": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmpolicybase/group/ssmpolicybase.mmp", ssm=True),
    "ssmstatepolicybase": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmstatepolicybase/group/ssmstatepolicybase.mmp", ssm=True),
    "ssmswppolicybase": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmswppolicybase/group/ssmswppolicybase.mmp", ssm=True),
    "ssmstartuppolicy": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmstartuppolicy/group/ssmstartuppolicy.mmp", rom="ssm.state.policy.0000.dll", ssm=True),
    "ssmshutdownpolicy": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmshutdownpolicy/group/ssmshutdownpolicy.mmp", rom="ssm.state.policy.0002.dll", ssm=True),
    "ssmfailpolicy": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmfailpolicy/group/ssmfailpolicy.mmp", rom="ssm.state.policy.0003.dll", ssm=True),
    "ssmuiswppolicy": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmuiswppolicy/group/ssmuiswppolicy.mmp", ssm=True),
    "ssmnoncriticalswppolicy": dict(mmp=f"{_SSM}/ssmpolicyplugins/ssmnoncriticalswppolicy/group/ssmnoncriticalswppolicy.mmp", ssm=True),
    "gsastatepolicynormal": dict(mmp=f"{_SSM}/systemstateplugins/gsapolicy/group/gsastatepolicynormal.mmp", rom="ssm.state.policy.0001.dll", ssm=True),
    # the SAA compatibility layer: saa*adaptation.dll installed under the generic names SSM's adaptation server loads
    "saastateadaptation": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/saastateadaptation.mmp", rom="stateadaptation.dll", ssm=True),
    "saasimadaptation": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/saasimadaptation.mmp", rom="simadaptation.dll", ssm=True),
    "saartcadaptation": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/saartcadaptation.mmp", rom="rtcadaptation.dll", ssm=True),
    "saamiscadaptation": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/saamiscadaptation.mmp", rom="miscadaptation.dll", ssm=True),
    "saaemergencycallrfadaptation": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/saaemergencycallrfadaptation.mmp", rom="emergencycallrfadaptation.dll", ssm=True),
    "startupadaptationadapter": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/startupadaptationadapter.mmp", ssm=True),
    "startupadaptationstub": dict(mmp="oss.FCL.sf.adapt.stubs/systemswstubs/startupadaptationstub/group/startupadaptationstub.mmp", ssm=True,
                                  # -> ekern/xk_power.cpp), as a licensee adaptation does; the stub only answered
                                  drop=["startupadaptationstub.cpp"], extra=["toolchain/shims/sastub_board/StartupAdaptationStub.cpp"]),
    "ssmclayersup": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/ssmclayersup.mmp", ssm=True),
    "rfstatusswppolicy": dict(mmp=f"{_SSM}/systemstatereferenceplugins/clayer/group/rfstatusswppolicy.mmp", ssm=True),
    "customcmds": dict(mmp=f"{_SSM}/systemstatereferenceplugins/custcmd/group/customcmds.mmp", ssm=True,
                       rom_data=["private\\10202be9\\2001d2aa.txt=baseline/symbian-2010/" + _SSM + "/systemstatereferenceplugins/custcmd/group/2001d2aa.txt"]),   # ssmcompatibility.iby
    "swppolicy_simstatus": dict(mmp=f"{_SSM}/systemstatereferenceplugins/custcmd/group/swppolicy_simstatus.mmp", rom="ssm.swp.policy.simstatus.dll", ssm=True),
    # the command lists (resources only) + the custom-command / utility plug-ins they name
    # hbdevicedialogappserver, emcore, hsapplicationlauncher -- critical, unbuildable); RCL_3 starts ailaunch +
    "ssmcmdlists": dict(mmp="../symbian-rcl3/" + _SSM + "/ssmcmdlists/group/ssmcmdlists.mmp", rsc_only=True, ssm=True,
                        # its own tree's substate/SWP ids (master reaches them through its .epoc32 export)
                        rsc_incs=["baseline/symbian-rcl3/" + _SSM + "/ssmpolicyplugins/inc"],
                        rss_delta={"noncriticalcmdlist_hw.rss": "tools/phorepoc-x86/board/ssm/noncriticalcmdlist_hw.rss",
                                   "securitycheckcmdlist.rss": "tools/phorepoc-x86/board/ssm/securitycheckcmdlist.rss"}),
    "ssmsystemcmds": dict(mmp=f"{_SSM}/ssmcustcmds/ssmsystemcmds/group/ssmsystemcmds.mmp", ssm=True),
    "ssmactivitycmd": dict(mmp=f"{_SSM}/ssmcustcmds/ssmactivitycmd/group/ssmactivitycmd.mmp", ssm=True),
    "ssmdiskreserver": dict(mmp=f"{_SSM}/ssmcustcmds/ssmdiskreserver/group/ssmdiskreserver.mmp", ssm=True),
    "ssmlangselcmd": dict(mmp=f"{_SSM}/syslangutil/ssmlangselcmd/group/ssmlangselcmd.mmp", ssm=True),
    "cmncustomcmds": dict(mmp=f"{_SSM}/systemstateplugins/cmncustomcmd/group/cmncustomcmds.mmp", ssm=True),
    "conditionevaluator": dict(mmp=f"{_SSM}/systemstateplugins/conditionevaluator/group/conditionevaluator.mmp", ssm=True),
    "ssmpowersup": dict(mmp=f"{_SSM}/ssmutilityplugins/ssmpowersup/group/ssmpowersup.mmp", ssm=True),
    "ssmlocaleobserversup": dict(mmp=f"{_SSM}/ssmutilityplugins/ssmlocaleobserversup/group/ssmlocaleobserversup.mmp", ssm=True),
    # the System Health Management set SSM runs with (shma.iby; the systemstarter copies are the sysstart era):
    # sysmon + its client, startsafe, restartsys, startup properties, and loadsysmon.dll -- the proxy SSM loads
    # by name (without it every monitored start left KErrNotSupported: "sysmon lib is not installed")
    "sysmon": dict(mmp="oss.FCL.sf.os.devicesrv/systemhealthmanagement/systemhealthmgr/group/sysmonsrv.mmp", ssm=True),
    "sysmoncli": dict(mmp="oss.FCL.sf.os.devicesrv/systemhealthmanagement/systemhealthmgr/group/sysmoncli.mmp", ssm=True),
    "startupproperties": dict(mmp="oss.FCL.sf.os.devicesrv/systemhealthmanagement/systemhealthmgr/group/startupproperties.mmp"),
    "startsafe": dict(mmp="oss.FCL.sf.os.devicesrv/systemhealthmanagement/systemhealthmgr/group/startsafe.mmp", ssm=True),
    "restartsys": dict(mmp="oss.FCL.sf.os.devicesrv/systemhealthmanagement/systemhealthmgr/group/restartsys.mmp", ssm=True),
    # which starts every app registered in the DSC store (dscstore.dll, a secure DBMS database) -- S60's startup list
    # for installed apps (SWI's startuplistupdater registers them). amastart.iby; DBMS variant (bld.inf default).
    # The starter trio is SSM-only (it needs startsafe/sysmon); the DSC store is not gated -- dscadd uses it in any ROM
    "loadamastart": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/applaunchservices/aftermarketappstarter/group/loadamastart.mmp", ssm=True),
    "amastart": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/applaunchservices/aftermarketappstarter/group/amastartdll.mmp", ssm=True),
    "amastarter": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/applaunchservices/aftermarketappstarter/group/amastartexe.mmp", ssm=True),
    "dscstore": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/applaunchservices/aftermarketappstarter/group/dscstore.mmp",
                     rom_data=["private\\100012a5\\policy\\2000836D.spd=baseline/symbian-rcl3/oss.FCL.sf.mw.appsupport/applaunchservices/aftermarketappstarter/group/2000836D.spd"]),
    "loadsysmon": dict(mmp="oss.FCL.sf.os.devicesrv/systemhealthmanagement/systemhealthmgr/group/loadsysmon.mmp", ssm=True),
    # the S^3 lists' critical starts (RCL_3 ssmcmdlists): the accessory server, the S60 Startup application (+ its
    # fail-list custom command), the touch-screen calibration command (first boot, when the feature is on)
    "accserver": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/AccServer.mmp", ssm=True),
    "accsrvutil": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/AccSrvUtil.mmp", ssm=True),
    "acccontrol": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/AccControl.mmp", ssm=True),
    "asyproxy": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/ASYProxy.mmp", ssm=True),
    "asycomms": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/ASYComms.mmp", ssm=True),
    "startup": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/startupservices/Startup/group/startup.mmp", ssm=True),
    "syserrcmd": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/startupservices/Startup/syserrcmd/group/syserrcmd.mmp", ssm=True),
    "tsccustcmds": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.settingsuis/systemswuis/touchscreencalib/tsccustcmds/group/tsccustcmds.mmp", ssm=True),
    # the locale DLLs SSM's language selection loads (ssmlangselcmd; the S^3 split locale model): English (UK) --
    # the bootui boot never loaded any, euser's built-in locale stood in
    "elocl_language_en_gb": dict(mmp="oss.FCL.sf.mw.uiresources/locales/loce32/language/group/elocl_language_en_gb.mmp", rom="elocl_lan.001", ssm=True),
    "elocl_region_gb": dict(mmp="oss.FCL.sf.mw.uiresources/locales/loce32/region/group/elocl_region_gb.mmp", rom="elocl_reg.826", ssm=True,
                            flags=["-finput-charset=ISO-8859-1"]),   # a Latin-1 pound sign in a wide _LIT (RVCT read Latin-1)
    "elocl_collation_en": dict(mmp="oss.FCL.sf.mw.uiresources/locales/loce32/collation/group/elocl_collation_en.mmp", rom="elocl_col.001", ssm=True),
    # the apparc starter SSM's start-safe loads by name (WAIT_FOR_APPARC_INIT; missing -> KErrNotSupported)
    "apstart": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/apstart/apstart.mmp", ssm=True),
    # SysAp's key-sound publisher (a WSERV animation DLL, RAnimDll::Load by name in CSysApKeySndHandler)
    "nspswsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/NspsWsPlugin/group/NspsWsPlugin.mmp", ssm=True),   # SysAp (RAnimDll)
    # SSM's restart proxy (sysmon RLibrary::Loads "custrestartsys.dll", else "restartsys.dll", by ordinal 1/2): without it
    "custrestartsys": dict(mmp="../symbian-rcl3/oss.FCL.sf.os.devicesrv/sysstatemgmt/sysrestart/group/sysrestart.mmp", ssm=True,
                           **{"def": "../symbian-rcl3/oss.FCL.sf.os.devicesrv/systemhealthmanagement/systemhealthmgr/eabi/restartsysu.def"}),
    "keypublisherplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/mediakeys/KeyPublisherPlugin/group/KeyPublisherPlugin.mmp", ssm=True),
    "akncustcmds": dict(mmp=f"{CUI}/uifw/AknGlobalUI/akncustcmds/group/akncustcmds.mmp", ssm=True),
    # + its init script (Z:\private\100059C9\ScriptInit.txt, run by SSM's rfscustcmd at every UI start; a product file
    # runs only the RFS plug-ins' parts (PLUGINS), as the release's own script0/script1 do
    "rfsserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/Rfs/group/rfsMain.mmp", ssm=True,
                      rom_data=["private\\100059C9\\ScriptInit.txt=tools/phorepoc-x86/board/rfs/ScriptInit.txt"]),
    "mediatorpluginbase": dict(mmp="oss.FCL.sf.os.devicesrv/mediator/group/MediatorPluginBase.mmp", ssm=True),
    "mediatorserver": dict(mmp="oss.FCL.sf.os.devicesrv/mediator/group/MediatorServer.mmp", ssm=True),
    "rfscustcmd": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/Rfs/Plugins/rfscustcmd/group/rfscustcmd.mmp", ssm=True),
    "stateadaptationref": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstateplugins/adptplugin/group/stateadaptationref.mmp"),
    "rtcadaptationref": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstateplugins/adptplugin/group/rtcadaptationref.mmp"),
    "simadaptationref": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstateplugins/adptplugin/group/simadaptationref.mmp"),
    "miscadaptationref": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstateplugins/adptplugin/group/miscadaptationref.mmp"),
    "emergencycallrfadaptationref": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstateplugins/adptplugin/group/emergencycallrfadaptationref.mmp"),
    # the alarm alert control eiksrvs hands to the alarm server's alert session (NewAlarmL)
    "eikalert": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/EikStd/alertsrc/Eikalert.mmp"),
    # the alarm UI plug-in eikalert loads by name (organizer's thin wrapper -> AknAlarmService)
    "almalert": dict(mmp="oss.FCL.sf.app.organizer/alarmui/group/AlmAlert.mmp"),
    # the HW resource manager server: the status pane's digital clock does CHWRMLight::NewL
    # untrapped, so every Avkon app with a status pane needs !HWRMServer to exist
    # (featureuids.h is GENERATED by the real build from featuredatabase.xml; the release's
    # Public_API export tree carries the generated copy -> searched LAST)
    "hwrmserver": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwresourcesmgr/server/group/HWRMServer.mmp",
                       flags=["-idirafter", os.path.join(SYMBIAN, "symbian-2010/oss.API_REF.Public_API/epoc32/include")]),
    # Symbian's OWN stub HW adaptation (sf.adapt.stubs): light/vibra/power-state/fmtx ECom plug-ins
    # the light TARGET MODIFIER plug-in: HWRM's ESystemTarget = its GetFullSystemTarget() & the supported targets.
    # Without it the full system target is 0 -- SysAp's "system lights on" lit nothing and Active Idle never saw the
    # control logic will be broken if the plugin is dropped from the image"
    "hwrmdefaultlighttargetmodifierplugin": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwrmtargetmodifierplugin/group/hwrmdefaultlighttargetmodifierplugin.mmp"),
    # silent on boards without it, as the stub's): src/hwrm/PowerStatePlugin.cpp
    "hwrmstubplugins": dict(mmp="oss.FCL.sf.adapt.stubs/systemswstubs/hwrmstubplugins/group/HWRMStubPlugins.mmp",
                            # + the light plug-in = OURS: the primary display through the display HAL (backlight
                            # on/off + brightness): src/hwrm/LightPlugin.cpp
                            drop=["PowerStatePlugin.cpp", "LightPlugin.cpp"],
                            extra=["src/hwrm/PowerStatePlugin.cpp", "src/hwrm/LightPlugin.cpp"],
                            libs=["hal.lib", "centralrepository.lib"]),   # (the boot guard timeout, developer menu key 3)
    # key-sound anim plug-in deps: tactile click plug-in + HWRM light client (client side only)
    "tactilearearegistry": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.hapticsservices/tactilefeedback/tactilearearegistry/group/tactilearearegistry.mmp"),
    "tactileclickplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.hapticsservices/tactilefeedback/tactileclickplugin/group/tactileclickplugin.mmp"),
    "hwrmclient": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwresourcesmgr/client/group/HWRMClient.mmp"),
    "hwrmcommon": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwresourcesmgr/common/group/HWRMCommon.mmp"),
    "hwrmextendedlightclient": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwresourcesmgr/extendedlight/group/HWRMExtendedLightClient.mmp"),
    "hwrmlightclient": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwresourcesmgr/light/group/HWRMLightClient.mmp"),
    # the vibra client (Symbian^4's QtSystemInfo: CHWRMVibra -- a link stub until now); the server's vibra side is
    # hwrmstubplugins' VibraPlugin (no motor on this board: requests succeed, nothing moves)
    "hwrmvibraclient": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwresourcesmgr/vibra/group/HWRMVibraClient.mmp"),
    # the sensor framework (client, utilities, server; no sensor plug-ins = no channels found): Orbit's orientation
    # orientation SSY advertises the rotation channel only); + orientationssy and its compensator library
    "sensrvclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.os.devicesrv/sensorservices/sensorserver/group/sensrvclient.mmp"),
    "sensrvutil": dict(mmp="../symbian-rcl3/oss.FCL.sf.os.devicesrv/sensorservices/sensorserver/group/sensrvutil.mmp"),
    "sensordatacompensator": dict(mmp="../symbian-rcl3/oss.FCL.sf.os.devicesrv/sensorservices/sensordatacompensator/group/sensordatacompensator.mmp"),
    "orientationssy": dict(mmp="../symbian-rcl3/oss.FCL.sf.os.devicesrv/sensorservices/orientationssy/group/OrientationSsy.mmp"),
    "sensrvserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.os.devicesrv/sensorservices/sensorserver/group/sensrvserver.mmp",
                         rom="sensorserver.exe"),   # KSensrvExeName
    # Symbian Foundation's no-audio stem (incubator.stemkit/breakdeps) + our one-hunk tone-player fix stood in
    # (toolchain/shims/stems/mmfclienttoneplayer.cpp, retired with it)
    "mediaclientaudio": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/client/MediaClientAudio.mmp",
                             rom="mediaclientaudio.dll"),
    # the common notifier app server (app UID 0x10281EF2): hosts CAknCommonNotifierWrapper plug-ins (the S60
    # connection dialogs, ...). EikSrvUi starts it through apparc once the app list knows it (_reg.rsc); missing
    "aknnfysrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/AknNfySrv/group/AknNfySrv.mmp"),
    "aknnotifierwrapper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/NotifierWrapper/group/aknnotifierwrapper.mmp"),
    "akndynamicsoftnoteeventmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/AknDynamicSoftNote/group/AknDynamicSoftNoteEventManager.mmp"),
    # akncapserver's own notifier library (CAknCapServer ctor: AppendNotifierLibNameL): every
    # global note/query/soft-note. Missing => LoadNotifiersL leaves -1 => "System error." note
    "aknoldstylenotif": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/OldStyleNotif/group/AknOldStyleNotif.mmp"),
    # real providers for what avkon/aknoldstylenotif had as panicking stubs (CAknEcsDetector ->
    # CPhCltEmergencyCall::NewL, the keylock notifier -> CKeyLockPolicyApi / CKeyguardAccessApi)
    "phoneclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/phoneclientserver/phoneclient/Group/PhoneClient.mmp"),
    # telephony T1: the Phone Server's emergency-number policy libraries (CPhSrvEmergencyNumberManager::ConstructL loads one; without
    # it PhoneServer's CreateAll leaves -1 and the Phone engine's client services never construct)
    # telephony T1: the Phone engine's device-mode handler library (CPEDeviceModeHandler::LoadLibraryL)
    "telephonydevicemode": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneuis/TelephonyDeviceMode/group/TelephonyDeviceMode.mmp"),
    "dosenpolicy": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/phoneclientserver/EnPolicy/Group/DOSENPolicy.mmp"),
    "sosenpolicy": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/phoneclientserver/EnPolicy/Group/SOSENPolicy.mmp"),
    "servicehandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/applicationinterworkingfw/ServiceHandler/group/ServiceHandler.mmp"),
    "lockclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/securitydialogs/lockclient/group/lockclient.mmp"),
    # the keyguard/devicelock server SSM starts at boot; it defines + sets KCoreAppUIsAutolockStatus = EAutolockOff.
    # Without it the key stays 0 ("uninitialized" = locked to ConnectionDialogsNotifBase::AutolockOn) and every
    "lockapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/securitydialogs/lockapp/group/lockapp.mmp"),
    "autolock": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/securitydialogs/Autolock/group/Autolock.mmp"),
    # wave 1 NOT built: the EAP family (eapauthserver/eapgeneric/eapgeneralsettings: the 2010 EAP framework is newer than
    # the exported headers; eaptools/eaptrace not built) -- one task with the WPA work; cmpluginvpn (abstract class vs our
    # cmmanager), hswidgetpublisher (Open C time.h __SOFTFP), locsettingsui* (missing internal header), hsbrowser (Qt)
    "accfwuinotifier": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/accfwuinotifier/group/accfwuinotifier.mmp"),
    "unitconverterrfsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/rfsplugins/UnitconverterRFSPlugin/group/UnitconverterRFSPlugin.mmp"),
    "msgcentrerfsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/rfsplugins/msgcentrerfsplugin/group/msgcentrerfsplugin.mmp"),
    "mmkeybearer": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/mediakeys/MMKeyBearer/group/MMKeyBearer.mmp"),
    "splashscreen": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/startupservices/SplashScreen/group/splashscreen.mmp"),
    "sanimsvgplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/startupservices/startupanimation/sanimsvgplugin/group/sanimsvgplugin.mmp"),
    "oodmonitor": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/sysresmonitoring/oodmonitor/group/OODMonitor.mmp"),
    "gsaccessoryplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/systemsettings/GSAccessoryPlugin/group/GSAccessoryPlugin.mmp"),
    "gssensorplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/systemsettings/gssensorplugin/group/gssensorplugin.mmp",
                           libs=["eikdlg.lib"]),
    # OURS: Control Panel > Device > "Power key" (short / long press action, CenRep 0xE70157B0 read by SysAp); after GSAutoKeyguardPlugin
    "phepowerkeyplugin": dict(mmp=os.path.join(OS_ROOT, "src/phepowerkeyplugin/group/phepowerkeyplugin.mmp")),
    "gsadminplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSAdminPlugin/GSAdminPlugin.mmp"),
    "gscallbarringplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSCallBarringPlugin/GSCallBarringPlugin.mmp"),
    "gscalldivertplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSCallDivertPlugin/GSCallDivertPlugin.mmp"),
    "gsdatacallplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSDataCallPlugin/GSDataCallPlugin.mmp"),
    "gsindicatorplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSIndicatorPlugin/group/GSindicatorplugin.mmp"),
    "gsinstallationsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSInstallationsPlugin/GSInstallationsPlugin.mmp"),
    "gsopticaljoystickplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSOpticalJoystickPlugin/GSOpticalJoystickPlugin.mmp"),
    "gspdataaccesspointplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSPDataAccessPointPlugin/GSPDataAccessPointPlugin.mmp"),
    "gspdataconnectionplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSPDataConnectionPlugin/GSPDataConnectionPlugin.mmp"),
    "gspdatahsdpaplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSPDataHSDPAPlugin/GSPDataHSDPAPlugin.mmp"),
    "gspdataplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSPDataPlugin/GSPDataPlugin.mmp"),
    "gspowersavingqueryplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSPowerSavingQueryPlugin/GSPowerSavingQueryPlugin.mmp"),
    "gssynchronisationplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSSynchronisationPlugin/GSSynchronisationPlugin.mmp"),
    "profilescheduleevent": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/profilesservices/ProfileEngine/group/ProfileScheduleEvent.mmp"),
    "ringingtone3dplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/profilesservices/RingingTone3DPlugin/group/ringingtone3dplugin.mmp"),
    "menuinterface": dict(mmp="oss.FCL.sf.mw.homescreensrv/contentstorage/casatinterface/group/MenuInterface.mmp"),
    "gspeninputplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/gsplugin/gspeninputplugin/group/gspeninputplugin.mmp"),
    "peninputsetting": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/gsplugin/peninputsetting/group/peninputsetting.mmp"),
    "dataconnectionlogger": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionmonitoring/connmon/dataconnectionlogger/group/dcl.mmp"),
    "flextimerclient": dict(mmp="oss.FCL.sf.mw.ipconnmgmt/keepalive/flextimer/client/group/flextimerclient.mmp"),
    "flextimerengine": dict(mmp="oss.FCL.sf.mw.ipconnmgmt/keepalive/flextimer/server/engine/group/flextimerengine.mmp"),
    "flextimerserver": dict(mmp="oss.FCL.sf.mw.ipconnmgmt/keepalive/flextimer/server/group/flextimerserver.mmp"),
    "netcfgextnhotspot": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/hotspotfw/hsclient/group/NetCfgExtnHotSpot.mmp"),
    "hotspotclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/hotspotfw/hsclient/group/hotspotclient.mmp"),
    "hssmgmtimpl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/hotspotfw/hsclient/group/hssmgmtimpl.mmp"),
    "wlanagthotspotimpl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/hotspotfw/hspluginforagent/group/wlanagthotspotimpl.mmp"),
    "hotspotserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/hotspotfw/hsserver/group/hotspotserver.mmp"),
    "ictsclientinterface": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/hotspotfw/internetconnectivitytestservice/group/ictsclientinterface.mmp"),
    "gsdasplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.settingsuis/defaultapplicationsettings/gsplugin/group/gsdasplugin.mmp"),
    "defaultappserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.settingsuis/defaultapplicationsettings/server/group/das_server.mmp"),
    "defaultappclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.settingsuis/defaultapplicationsettings/service/group/das_service.mmp"),
    "servicesdb": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.settingsuis/defaultapplicationsettings/services_db/group/services_db.mmp"),
    "gsrfsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/Rfs/Plugins/GSRFSPlugin/group/GSRFSPlugin.mmp"),
    "favouritesrfsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/rfsplugins/FavouritesRFSPlugin/group/FavouritesRFSPlugin.mmp"),
    "contentharvesterclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentharvester/contentharvesterclient/group/contentharvesterclient.mmp"),
    "chswiplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentharvester/contentharvesterswiplugin/group/chswiplugin.mmp"),
    "gslangplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/GSLangPlugin/GSLangPlugin.mmp"),
    "apcontrollistplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/accesspointcontrol/apcontrollistplugin/group/apcontrollistplugin.mmp"),
    "apsettingsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/apengine/apsettingsplugin/group/ApSettingsPlugin.mmp"),
    "connectionmonitorplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionmonitoring/connectionmonitorplugin/group/connectionmonitorplugin.mmp"),
    "connectionmonitorui": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionmonitoring/connectionmonitorui/group/ConnectionMonitorUI.mmp"),
    # wave 1 providers (their users linked stubs without them)
    "remconbearerplugin": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/bearerplugin/group/remconbearerplugin.mmp"),
    "environmentalreverbutility": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effectspresets/EnvironmentalReverbUtility/group/EnvironmentalReverbUtility.mmp"),
    "serviceregistry": dict(mmp="oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group/ServiceRegistry.mmp"),
    "gsserverengine": dict(mmp="oss.FCL.sf.mw.appsupport/coreapplicationuis/GSServerEngine/group/GSServerEngine.mmp"),
    "agendapluginengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/organizerplugin/aiagendapluginengine/group/AgendaPluginEngine.mmp"),
    "aicalendarplugin2": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/organizerplugin/aiagendaplugin2/group/aicalendarplugin2.mmp"),
    "cseschedulerclient": dict(mmp="oss.FCL.sf.mw.videoutils/videoscheduler/SchedulerClient/group/CseSchedulerClient.mmp"),
    "power_save_display_mode": dict(mmp="oss.FCL.sf.adapt.stubs/power_save_display_mode_stub/group/power_save_display_mode.mmp"),
    "bsengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/backsteppingsrv/group/bsengine.mmp"),
    "cseschedulerserver": dict(mmp="oss.FCL.sf.mw.videoutils/videoscheduler/SchedulerServer/group/ScheduleServer.mmp"),
    "dbrecovery": dict(mmp="oss.FCL.sf.mw.appsupport/coreapplicationuis/DbRecovery/Group/DbRecovery.mmp"),
    # ncnlist (the Network Communication Notifier: SMS first-boot -- it fetches the service centres from the SIM via smum's
    "ncnlist": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/ncnlist/group/Ncnlist.mmp"),
    "predefinedcontacts": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/contactextensions/predefinedcontacts/group/predefinedcontacts.mmp"),
    "profilesettingsmonitor": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/profilesettingsmonitor/group/ProfileSettingsMonitor.mmp"),
    "rsfwbootmounter": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotestorage/remotestoragefw/mountmanager/group/rsfwbootmounter.mmp"),
    # NOT built: satserver (a shell over satengine.dll = the SIM toolkit, needs the modem stack), dmutilserver (the SyncML
    # DM stack), epos_omasupllistener (the SUPL stack), iaupdatebg (the IAD client), lbtserver (location triggering),
    "schexe": dict(mmp="oss.FCL.sf.os.ossrv/genericservices/taskscheduler/group/SCHEXE.MMP"),
    "screensaver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.screensaver/group/screensaver.mmp",
                        flags=["-I" + os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.adapt.stubs/power_save_display_mode_stub/inc")]),
    "keylockpolicyapi": dict(mmp="oss.FCL.sf.mw.securitysrv/securitydialogs/keylockpolicyapi/group/keylockpolicyapi.mmp"),
    "mediatorclient": dict(mmp="oss.FCL.sf.os.devicesrv/mediator/group/MediatorClient.mmp"),
    "tacticon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.hapticsservices/tacticonserver/client/group/tacticon.mmp"),
    "ecompluginnotifier": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/AknCapServer/group/EComPluginNotifier.mmp"),
    "syslangutil": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/syslangutil/group/syslangutil.mmp"),
    "akncapserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AknGlobalUI/AknCapServer/group/AknCapServer.mmp",
                         drop=["AknEikSgcs.cpp"], extra=["src/phorepoc/akncapserver_trace/AknEikSgcs.cpp"]),
    "apsexe": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/appfw/apparchitecture/group/APSEXE.MMP"),
    "featmgrserver": dict(mmp="oss.FCL.sf.os.persistentdata/featuremgmt/featuremgr/group/featmgrserver.mmp"),
    "aknskinsrvexe": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/AknSkinSrvMain.mmp",
                          rom="aknskinsrv.exe"),
    "akniconsrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/uigraphics/AknIcon/group/AknIconSrvMain.mmp"),
    "eiksrvs": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/commonuisupport/uikon/group/EIKSRVS.MMP"),
    "oommonitor": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/sysresmonitoring/oommonitor/group/oommonitor.mmp",
                       rom_data=["private\\10207218\\oomconfig.xml=baseline/symbian-rcl3/oss.FCL.sf.mw.appsupport/sysresmonitoring/oommonitor/data/oomconfig.xml"]),
    # OURS: null provider (real ordinal table) -- see toolchain/shims/goommonitor_null/
    # OURS: null Orbit core -- eiksrvui reports a dead app through CHbDeviceDialogSymbian (S^4
    # migration); as unimpl stubs that turned any app crash into an eiksrvs panic + kernel fault
    "hbcore": dict(mmp=os.path.join(OS_ROOT, "toolchain/shims/hbcore_null/hbcore_null.mmp"),
                   **{"def": "oss.FCL.sf.mw.hb/src/hbcore/defs/eabi/HbCoreu.def"}),
    "goommonitor": dict(mmp=os.path.join(OS_ROOT, "toolchain/shims/goommonitor_null/goommonitor_null.mmp"),
                        **{"def": "oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/goommonitor/eabi/goommonitoru.DEF"}),
    "bitmaptransforms": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/BitmapTransform/MmpFiles/BitmapTransforms.mmp"),
    "imageconversion": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/group/ImageConversion.mmp"),
    "jpegcodec": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/group/JPEGCodec.mmp"),   # the root
    "pngcodec": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/group/PNGCodec.mmp"),   # the root
    "gifcodec": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/group/GIFCodec.mmp"),   # the root
    "bmpcodec": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/group/BMPCodec.mmp"),   # the root
    "mbmcodec": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/group/MBMCodec.mmp"),   # the root
    "wbmpcodec": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/group/WBMPCodec.mmp"),   # the root
    "icocodec": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/group/ICOCodec.mmp"),   # the root
    "bitmaptransformsrefplugin": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingplugins/bitmaptransform/refplugin/group/BitmapTransformsRefPlugin.mmp"),   # the root
    "imagedisplay": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/ImageDisplay/MmpFiles/ImageDisplay.mmp"),   # the root
    # them -- missing, every lookup (FileNewL by content, by MIME, by type) left KErrNotFound in ECom
    "imageconversionresolver": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/group/ImageConversionResolver.mmp"),
    "imagetransformresolver": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/ImageTransform/MmpFiles/ImageTransformResolver.mmp"),
    "imagedisplayresolver": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/ImageDisplay/MmpFiles/ImageDisplayResolver.mmp"),   # the root
    "iclwrapperimagedisplay": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/ImageDisplay/MmpFiles/plugins/geniclwrapper.mmp"),   # the root
    "imageprocessorperf": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/group/ImageProcessorPerf.mmp"),   # <- ImageConversion.mmp
    "exiflibstatic": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/group/ExifLibStatic.mmp"),   # <- JPEGCodec.mmp
    "gifscaler": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/GifScaler/MmpFiles/GifScaler.mmp"),   # <- GIFCodec.mmp
    "exifutility": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/ExifUtility/mmpfiles/ExifUtility.mmp"),   # <- geniclwrapper.mmp
    # predictive text engine: akncapserver owns the key-mapping P&S keys and creates one at start
    # (no language cores/keymap data in the ROM yet -> it comes up with zero languages)
    # the ECom FEP 0x101FD65A that the FEP framework repository names as default: the REAL AknFep
    # stack below -- UniEditor, and any touch text entry, needs the pen-input server
    "avkonfep": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/fep/aknfep/group/AknFep.mmp", defs=UNRELEASED_FEATURES),
    "ptiengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/ptienginev2/group/ptienginev2.mmp"),
    # keymap DATA plug-in (English): without one CPtiDefaultCore::InitializeL leaves KErrCorrupt
    # ("cannot function without keymapping data") -> akncapserver raises the "System error." note
    "ptikeymappings_itut_01": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/ptienginev2/languages/PtiKeymappings_ITUT_01.mmp",
                                   drop=["ptikeymappings_itut_01.cpp"], extra=["toolchain/shims/ptikeymap_board/PtiKeymappings_ITUT_01.cpp"]),
    # its WSERV render plug-in (a CWsGraphicDrawer, ECOM; the server's CWsGraphic::BaseConstructL left -1 without it)
    "peninputcrp": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/src/peninputcrp/peninputcrp.mmp", rom="2000b5e7.dll"),
    "aknfepuiavkonplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/fep/aknfep/UiPlugins/AknFepUiInterface/AvkonImpl/group/AknFepUiAvkonPlugin.mmp"),   # the root
    "peninputserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/src/peninputserverapp/peninputserver.mmp"),   # the root
    "peninputanim": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/src/peninputanim/peninputanim.mmp"),   # the root
    "peninputgenericvkb": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputgenericvkb/group/peninputgenericvkb.mmp", defs=UNRELEASED_FEATURES,
                               # GCC delta (default args on a function-pointer typedef): toolchain/shims/peninputgenericvkb_gcc
                               drop=["peninputgenericlafdatamgr.cpp"],
                               extra=["toolchain/shims/peninputgenericvkb_gcc/peninputgenericlafdatamgr.cpp"]),   # the root
    "peninputgenericitut": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputgenericitut/group/peninputgenericitut.mmp"),   # the root
    # switch point -- GetInputMethodUiL maps a pen-input mode to a layout implementation UID; the shim asks the input-method
    # registry (phimreg) first, so phkbd (or the stock keyboard) is picked by the user's active choice. A whole-file copy of
    # peninputimeplugingeneric.cpp with three marked "PHOREPOC delta" hunks (the registry call in both GetInputMethodUiL
    # overloads, LayoutUiImplId); everything else is the 2010 source unchanged.
    "peninputimeplugingeneric": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/fep/aknfep/peninputplugins/peninputimeplugingeneric/group/peninputimeplugingeneric.mmp", defs=UNRELEASED_FEATURES,
                                     drop=["peninputimeplugingeneric.cpp"],
                                     extra=["toolchain/shims/peninputimeplugingeneric_phim/peninputimeplugingeneric.cpp"],
                                     flags=["-iquote", os.path.join(OS_ROOT, "src/phorepoc/input/phimreg")],
                                     libs=["phimreg.lib"], allow_wsd=True),   # the root (allow_wsd: gPhImLayoutId)
    # OURS (keyboard framework): the input-method registry DLL -- installed keyboards (ECom entries + built-in stock ones),
    # the active choice and phkbd's customisation in CenRep E7015830
    "phimreg": dict(mmp=os.path.join(OS_ROOT, "src/phorepoc/input/phimreg/phimreg.mmp"),
                    **{"def": os.path.join(OS_ROOT, "src/phorepoc/input/phimreg/phimreg.def")}),
    # OURS (keyboard framework): phkbd, the N9-style QWERTY -- a pen input server layout plug-in registered with the
    # OURS (keyboard framework): the Settings (GS) entry -- default keyboard + phkbd's customisation (src/phorepoc/input/phgskbd)
    "phgskbd": dict(mmp=os.path.join(OS_ROOT, "src/phorepoc/input/phgskbd/phgskbd.mmp")),
    "phkbd": dict(mmp=os.path.join(OS_ROOT, "src/phorepoc/input/phkbd/phkbd.mmp"), allow_wsd=True,
                  flags=["-I" + os.path.join(EPOC32_ROOT, "tree/_up/symbian-rcl3/oss.fcl.sf.mw.inputmethods/inc")]),
    "peninputimepluginitut": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/fep/aknfep/peninputplugins/peninputimepluginitut/group/peninputimepluginitut.mmp"),   # the root
    "aknfepuiinterface": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/fep/aknfep/UiPlugins/AknFepUiInterface/group/AknFepUiInterface.mmp"),   # <- AknFep.mmp
    "peninputlayouteng": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/src/peninputlayouteng/peninputlayouteng.mmp",
                              # BUG FIX delta: the IME finder's RArray key (iLanguage, a PADDING byte, iMode) was
                              # built on the stack with the padding uninitialised -> lookups never matched, no IME
                              drop=["peninputimefinder.cpp"],
                              extra=["toolchain/shims/peninputlayouteng_keyfix/peninputimefinder.cpp"]),   # <- peninputserver.mmp
    "peninputlayoutinterface": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/src/peninputlayoutinterface/peninputlayoutinterface.mmp"),   # <- peninputlayouteng
    "peninputlayoutcontrol": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/src/peninputlayoutcontrol/peninputlayoutcontrol.mmp", defs=UNRELEASED_FEATURES),   # <- peninputgenericvkb.mmp
    "peninputpluginutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/utils/group/peninputpluginutils.mmp"),   # <- peninputlayoutcontrol
    "peninputcommonctrls": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputcommonctrls/group/peninputcommonctrls.mmp"),   # <- peninputgenericvkb.mmp
    "peninputcommonlayout": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputcommonlayout/group/peninputcommonlayout.mmp"),   # <- peninputgenericvkb.mmp
    # OURS: file-server semantics probe (create/replace/rename/delete on C:)
    "fstest": dict(mmp=os.path.join(OS_ROOT, "src/fstest/fstest.mmp")),
    "dsatest": dict(mmp=os.path.join(OS_ROOT, "src/dsatest/dsatest.mmp"), test=True),
    # opens BrowserNG on a URL at boot (SSM_TEST=phebrlaunch): the phone has no C:ootui.txt cycle
    "phebrlaunch": dict(mmp=os.path.join(OS_ROOT, "src/phebrlaunch/phebrlaunch.mmp"), test=True),
    "phetelprobe": dict(mmp=os.path.join(OS_ROOT, "src/phetelprobe/phetelprobe.mmp")),
    "phesmsprobe": dict(mmp=os.path.join(OS_ROOT, "src/phesmsprobe/phesmsprobe.mmp")),   # OURS: telephony T1, the SMS stack over the fake modem
    # OURS (VDU V4): the MMF video play controller for MP4/3GP (ECOM 0x101F4F47; mp4demux -> CMMFDevVideoPlay -> phevdec ->
    # NV12 -> DSA); phevidtest plays .vde-test/*.mp4 through CVideoPlayerUtility (tools/vdu/make_vde_test.sh makes them).
    # SSM_TEST=phevidtest ROM_TAIL="mmcommonutils,devvideo,puresolverutils,codecapiresolver,phevdec,pheffmpeg,phevidctl,phevidtest" (+ the MMF
    # client libs the ROM lacks: mediaclientvideo, mmfcontrollerframework, mmfstdsourceandsinkplugin, mmfaudiooutput ...)
    "phevidctl": dict(mmp=os.path.join(OS_ROOT, "src/phevidctl/phevidctl.mmp")),
    # the FFmpeg DLL (pheffmpeg) and its test moved to the codecpack repository (LGPL; installed separately)
    "phevidtest": dict(mmp=os.path.join(OS_ROOT, "src/phevidtest/phevidtest.mmp"), test=True,
                       rom_data=["resource\\phevde\\vmain.mp4=.vde-test/vmain.mp4",
                                 "resource\\phevde\\vbase.mp4=.vde-test/vbase.mp4"]),
    "svgtest": dict(mmp=os.path.join(OS_ROOT, "src/svgtest/svgtest.mmp"), test=True),
    # OURS: software-OpenVG stack probe (VGI surface -> clear/fill -> bitmap -> pixels on serial)
    "libcprobe": dict(mmp=os.path.join(OS_ROOT, "src/libcprobe/libcprobe.mmp"), defs=["__SOFTFP="]),
    "xmlprobe": dict(mmp=os.path.join(OS_ROOT, "src/xmlprobe/xmlprobe.mmp"), defs=["__SOFTFP=", "__declspec(x)="]),
    "vgprobe": dict(mmp=os.path.join(OS_ROOT, "src/vgprobe/vgprobe.mmp"), defs=["__declspec(x)="]),
    # O1: libEGL (EGL_OPENVG_API) + libOpenVG/libOpenVGU vs the reference implementation, "PH: vg" lines
    "vgegltest": dict(mmp=os.path.join(OS_ROOT, "src/vgegltest/vgegltest.mmp"), test=True, defs=["__declspec(x)="]),
    # OURS: minimal system starter (stands in for SSM during UI bring-up)
    "bootui": dict(mmp=os.path.join(OS_ROOT, "src/bootui/bootui.mmp")),
    "nettest": dict(mmp=os.path.join(OS_ROOT, "src/nettest/nettest.mmp")),
    "commsdatcreator": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/dbcreator/commsdatcreator/group/cdccommsdatcreator.mmp", flags=["-finput-charset=ISO-8859-1"]),   # Latin-1 _LIT16 literals (cdcreaderxml.cpp)   # dbcreator: CreateTablesL = every S60 CommsDat table (cmsetup)
    "sigtest": dict(mmp=os.path.join(OS_ROOT, "src/sigtest/sigtest.mmp")),
    "sistestdll": dict(mmp=os.path.join(OS_ROOT, "src/sistest/sistestdll.mmp"), sis=True, allow_wsd=True),
    "sistest": dict(mmp=os.path.join(OS_ROOT, "src/sistest/sistest.mmp"), sis=True),
    "rscdump": dict(mmp=os.path.join(OS_ROOT, "src/rscdump/rscdump.mmp")),
    "dscadd": dict(mmp=os.path.join(OS_ROOT, "src/dscadd/dscadd.mmp"),
                   flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.appsupport/applaunchservices/aftermarketappstarter/inc")]),
    "cmsetup": dict(mmp=os.path.join(OS_ROOT, "src/cmsetup/cmsetup.mmp"), flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/dbcreator/inc"), "-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/database/inc")]),
    "ntpsync": dict(mmp=os.path.join(OS_ROOT, "src/ntpsync/ntpsync.mmp")),
    "sisinst": dict(mmp=os.path.join(OS_ROOT, "src/sisinst/sisinst.mmp")),
    "tonetest": dict(mmp=os.path.join(OS_ROOT, "src/tonetest/tonetest.mmp"), test=True),
    # --- vector graphics: what the default skin's backgrounds + every real icon need (next: vgrenderer,
    "svgengine": dict(mmp="oss.FCL.sf.mw.svgt/svgtopt/SVG/SVGEngine/group/SVGEngine.mmp",
                      # __int64 = an RVCT/MSVC keyword (gfx2d Gfxmath.h); imcvcodc.h (imut, the base64
                      # codec for embedded images) is exported by app.messaging, not part of our tree
                      # vgcontext.h (libVGI) marks its API __declspec(dllimport) -- RVCT/MSVC again
                      defs=["__int64=long long", "__declspec(x)="],
                      cdriver=True,       # gfx2d GfxMath.c: the fixed-point primitives (svgScalarDiv...)
                      drop=["svgmemorymanager.cpp"], extra=["toolchain/shims/svg_fix/SVGMemoryManager.cpp"],
                      flags=["-I" + os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.app.messaging/email/pop3andsmtpmtm", d)
                             for d in ("servermtmutils/inc", "clientmtms/inc")]),
    "vgrenderer": dict(mmp="oss.FCL.sf.mw.svgt/svgtopt/VGRenderer/group/VGRenderer.mmp"),
    "swvg": dict(mmp="oss.FCL.sf.mw.svgt/svgtopt/VGRenderer/SWVG/group/SWVG.mmp",
                 defs=["__int64=long long", "__declspec(x)="]),
    # the TLV renderer of vgrenderer (ESVGRendererTLV): AknIcon (_NGATESTING) sends every SVG/SVG-T icon to
    # CAknNGAFormatHandler, whose engine loads PseudoVG.dll -- without it every 3rd-party app icon and theme
    "pseudovg": dict(mmp="oss.FCL.sf.mw.svgt/svgtopt/VGRenderer/PseudoVG/group/PseudoVG.mmp",
                     defs=["__int64=long long", "__declspec(x)="]),
    # the software OpenVG = the Khronos reference implementation (sfopenvg). It wants P.I.P.S.
    # libc/libm/libstdcpp but only uses math + mem/str -> estlib provides them (`libs`). Its
    # try/catch(std::bad_alloc) OOM paths are compiled out: we build -fno-exceptions and
    # USE_NEW_ELEAVE makes allocation failure a (setjmp) leave anyway.
    # riEGLOS.cpp = OURS two-hunk fix of the snapshot's file (toolchain/shims/stems): it mapped
    # EColor16MA/MU to sRGBA/sRGBX -- Symbian's pixel words are sARGB/sXRGB.
    "sflibopenvgrefimpl": dict(mmp=_SFVG + "sfopenvg.mmp", libs=["estlib.lib"], allow_wsd=True, vfp=True,
                               # x32: riDefs.h wants the COMPILER's <float.h>, which -nostdinc hides
                               x32_flags=["-idirafter", "/usr/lib/gcc/x86_64-linux-gnu/13/include"],
                               # the reference 0.75-px Gaussian left edge pixels 89% opaque: a line at every skin-frame slice join
                               drop=["rieglos.cpp", "rirasterizer.cpp", "riimage.cpp", "ripixelpipe.cpp"], extra=["toolchain/shims/stems/riEGLOS.cpp", "toolchain/shims/sfopenvg_board/riRasterizer.cpp", "toolchain/shims/sfopenvg_board/riImage.cpp", "toolchain/shims/sfopenvg_board/riPixelPipe.cpp", "toolchain/shims/sfopenvg_board/riPhvg.cpp"],
                               noinc=["stdapis"], flags=["-iquote", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.graphics/openvg/openvgrefimplementation/sfopenvg/sfopenvg"),
                                                         "-I" + os.path.join(OS_ROOT, "toolchain/shims/sfopenvg_inc"),
                                                         "-I" + os.path.join(OS_ROOT, "src/phvg"),
                                                         "-I" + os.path.join(EPOC32_ROOT, "include/libc")],
                               defs=["try=if(1)", "catch(x)=else if(0)", "throw=for(User::Leave(-4);;)",
                                     "__declspec(x)="]),
    # O1 (openvg-plan.md): the real Symbian^3 names, still software (the same facades over sflibopenvgrefimpl)
    "libopenvg": dict(mmp=os.path.join(OS_ROOT, "src/openvg/libopenvg.mmp"), rom="libOpenVG.dll", defs=["__declspec(x)="],
                      **{"def": "oss.FCL.sf.os.graphics/openvg/openvginterface/eabi/libopenvg11u.def"}),
    "libopenvgu": dict(mmp=os.path.join(OS_ROOT, "src/openvg/libopenvgu.mmp"), rom="libOpenVGU.dll", defs=["__declspec(x)="],
                       **{"def": "oss.FCL.sf.os.graphics/openvg/openvginterface/eabi/libopenvgu11u.def"}),
    # O2 (openvg-plan.md): the GPU engine (OpenVG on GLES2). Loaded with RLibrary by the RI (riPhvg.cpp) only when the
    # mode property is 1; the board deltas riRasterizer/riImage/riPixelPipe/riPhvg of sflibopenvgrefimpl are its hooks.
    # nga=True: it runs on GLES2 through libEGL, which only an NGA ROM carries
    "phvg": dict(mmp=os.path.join(OS_ROOT, "src/phvg/phvg.mmp"), nga=True, vfp=True, allow_wsd=True, rom="phvg.dll",
                 defs=["__declspec(x)="], **{"def": os.path.join(OS_ROOT, "src/phvg/phvg.def")}),
    # O2 test: a clear + solid-path scene through phvg and through the reference implementation, "PH: vg2" lines
    "vgprobe2": dict(mmp=os.path.join(OS_ROOT, "src/vgprobe2/vgprobe2.mmp"), test=True, defs=["__declspec(x)="],
                     flags=["-I" + os.path.join(OS_ROOT, "src/phvg")]),
    "libopenvg_sw": dict(mmp=_SFVG + "openvg.mmp", defs=["__declspec(x)="],
                         **{"def": "oss.FCL.sf.os.graphics/openvg/openvginterface/eabi/libopenvg11u.def"}),
    "libopenvgu_sw": dict(mmp=_SFVG + "openvgu.mmp", defs=["__declspec(x)="],
                          **{"def": "oss.FCL.sf.os.graphics/openvg/openvginterface/eabi/libopenvgu11u.def"}),
    "libegl_sw": dict(mmp=_SFVG + "egl.mmp", defs=["__declspec(x)="],
                      **{"def": "oss.FCL.sf.os.graphics/egl/eglinterface/eabi/libegl13U.def"}),
    "libvgi": dict(mmp=_SFVG + "vgi.mmp", defs=["__declspec(x)="]),
    # NVG (the binary SVG-Tiny that svgtbinencode/mifconv emit) -> OpenVG. _sw = what the FBS
    # rasterizer links; the plain one (AknIcon: ContentDimensions) is pointed at the same software VG
    "nvgdecoder_sw": dict(mmp="oss.FCL.sf.mw.svgt/svgtopt/nvgdecoder/group/NVGDecoder_SW.mmp",
                          libs=["estlib.lib"], noinc=["stdapis"], defs=["__declspec(x)="],
                          flags=["-I" + os.path.join(EPOC32_ROOT, "include/libc")]),
    "nvgdecoder": dict(mmp="oss.FCL.sf.mw.svgt/svgtopt/nvgdecoder/group/nvgdecoder.mmp",
                       libs=["estlib.lib", "libopenvg_sw.lib", "libopenvgu_sw.lib"], noinc=["stdapis"],
                       defs=["__declspec(x)="],
                       flags=["-I" + os.path.join(EPOC32_ROOT, "include/libc")]),
    # imut = the messaging MTM utilities; svgengine only wants its base64 codec (embedded
    # data: images) -> build the codec TU alone, the other 251 ordinals stay unimpl stubs
    "imut": dict(mmp="oss.FCL.sf.app.messaging/email/pop3andsmtpmtm/servermtmutils/group/IMUT.mmp",
                 drop=["imutdll.cpp", "imcvutil.cpp", "imcvsend.cpp", "imcvrecv.cpp", "imsk.cpp",
                       "imskscr.cpp", "imutcon.cpp", "cimsocketidletimer.cpp", "cimcaf.cpp",
                       "cmsvplainbodytextentry.cpp", "cimmobilitymanager.cpp"]),
    "libzcore": dict(mmp="oss.FCL.sf.os.ossrv/compressionlibs/ziplib/group/libz/libzcore.mmp", cdriver=True),
    "ezlib": dict(mmp="oss.FCL.sf.os.ossrv/compressionlibs/ziplib/group/ezlib.mmp"),
    "cxmlparser": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/cxmllibrary/group/cXmlParser.mmp", cdriver=True),
    # libxml2 (on the REAL P.I.P.S. libc) + the Symbian DOM/XPath API over it -- matrixmenu's suite XML
    # x32: C sources are gnu99 -> trio_nan() takes its C99 branch, nan(""), which -fno-builtin leaves as a
    "xmlengine": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/libxml2libs/group/xmlengine.mmp", allow_wsd=True, cdriver=True,
                      defs=["__SOFTFP=", "__declspec(x)="], x32_flags=["-Dnan(x)=__builtin_nan(x)"]),
    "xmlengineutils": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/libxml2libs/group/xmlengineutils.mmp", defs=["__SOFTFP=", "__declspec(x)="]),
    "xmlenginedom": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/xmldomandxpath/group/xmlenginedom.mmp", defs=["__SOFTFP=", "__declspec(x)="]),
    "flogger": dict(mmp="oss.FCL.sf.os.persistentdata/loggingservices/filelogger/group/FLOGGER.MMP"),
    "inetprotutil": dict(mmp="oss.FCL.sf.os.ossrv/genericservices/httputils/group/InetProtUtil.mmp"),
    "aknswallpaperutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.uiresources/skins/AknSkins/group/AknsWallpaperUtils.mmp"),
    "senxml": dict(mmp="oss.FCL.sf.mw.websrv/webservices/wsxml/group/senxml.mmp", flags=[_KEEPNULL]),
    "xcfw": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/xcfw/group/xcfw.mmp", flags=[_KEEPNULL]),
    "rtsecmgrclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.serviceapifw/rtsecuritymanager/rtsecuritymanagerclient/group/rtsecmgrclient.mmp", flags=[_KEEPNULL]),
    # xmlframework's parser plug-ins: expat (the default "text/xml" parser -- LIW provider metadata)
    "xmlparserplugin": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/xmlexpatparser/group/xmlparserplugin.mmp", cdriver=True),
    "xmlcustomresolver": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/xmlfw/group/xmlcustomresolver.mmp"),   # how xmlframework picks a parser
    # the libxml2 SAX parser plug-in (variant "libxml2"): HSPS asks for exactly that variant -- with only the
    "xmlenginesaxplugin": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/xmllibxml2parser/group/xmlenginesaxplugin.mmp", cdriver=True,
                               defs=["__SOFTFP=", "__declspec(x)="]),
    "xmlframework": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/xmlfw/group/xmlframework.mmp"),
    "xmldom": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/legacyminidomparser/XMLDom/GROUP/XMLDOM.mmp"),
    "xmlparser": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/legacyminidomparser/XMLParser/GROUP/xmlparser.mmp"),
    "rtsecmgrutil": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.serviceapifw/rtsecuritymanager/rtsecuritymanagerutil/group/RTSecMgrUtil.mmp", flags=[_KEEPNULL]),
    # LIW's custom ECOM resolver: the service handler finds providers (e.g. mcsprovider) only through it
    "liwresolver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.serviceapifw/languageinterworkingfw/servicehandler/group/liwresolver.mmp",
                        flags=[_KEEPNULL]),
    "liwservicehandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.serviceapifw/languageinterworkingfw/servicehandler/group/servicehandler.mmp", flags=[_KEEPNULL]),
    # the menu framework (hierarchy navigator + matrix menu UI), RCL_3. _XDOM: S^3 code includes the
    # XML Engine DOM headers flat (<xmlengdom.h>); the 2010 xmlsrv exports them under xml/dom/
    "hnutilities": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/hierarchynavigator/hnutilities/group/hnutilities.mmp", **_XDOM),
    "hnmetadatamodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/hierarchynavigator/hnmetadatamodel/group/hnmetadatamodel.mmp", **_XDOM,
                            # OURS one-line stem: a TLex built from a non-NUL-terminated HBufC::Ptr()
                            drop=["hnxmlmodelprovider.cpp"], extra=["toolchain/shims/stems/hnxmlmodelprovider.cpp"]),
    "hnpresentationmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/hierarchynavigator/hnpresentationmodel/group/hnpresentationmodel.mmp", **_XDOM),
    "hnengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/hierarchynavigator/hnengine/group/hnengine.mmp", **_XDOM),
    "mmextensionmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/menufwui/mmextensions/mmextensionmanager/group/mmextensionmanager.mmp", **_XDOM),
    "mmwidgets": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/menufwui/mmwidgets/group/mmwidgets.mmp", defs=_XDOM["defs"],
                      flags=_XDOM["flags"]),
    "matrixmenu": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/menufwui/matrixmenu/group/matrixmenu.mmp", **_XDOM,
                       # no EPOCSTACKSIZE = the 8K default: enough under RVCT, but our g++ -O2 frames
                       # overflow it in CAknToolbar layout during AppUi construction
                       stack=0x10000),
    # OURS: null ALF clients (real mmps + ordinal tables; no ALF server on this board) -- toolchain/shims/alf_null
    "alfclient": dict(mmp="oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/Client/src/alfclient.mmp",
                      nosrc=True, extra=["toolchain/shims/alf_null/alfclient_null.cpp"]),
    "alfdecoderserverclient": dict(mmp="oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/AlfDecoderServerClient/src/alfdecoderserverclient.mmp",
                                   nosrc=True, extra=["toolchain/shims/alf_null/alfdecoderserverclient_null.cpp"]),
    # *_real twins are staged under their names (rom_as). The default image is unchanged.
    "ftokenclient": dict(mmp=_ALF + "TokenServer/ClientSrc/ftokenclient.mmp", alf=2),
    "hitchcock": dict(mmp=_ALF + "coretoolkit/group/huitk_base.mmp", alf=2, libs=["estor.lib"],
                  **({"drop": ["huienv.cpp", "huidisplay.cpp", "huicanvaswspainter.cpp", "huirosterimpl.cpp"] + (["huivisual.cpp"] if _ALFRC in ("1", "v") else []) + (["huicanvasvisual.cpp"] if _ALFRC in ("1", "c") else []), "extra": [_AG + "HuiEnv_ph.cpp", _AG + "HuiDisplay_ph.cpp", _AG + "huicanvaswspainter_ph.cpp", _AG + "HuiRosterImpl_ph.cpp"] + ([_AG + "HuiVisual_ph.cpp"] if _ALFRC in ("1", "v") else []) + ([_AG + "HuiCanvasVisual_ph.cpp"] if _ALFRC in ("1", "c") else [])} if _ALFTRACE else {})),
    "alfclient_real": dict(mmp=_ALF + "Client/src/alfclient.mmp", alf=2, rom="alfclient_real.dll", rom_as="alfclient.dll"),
    "alfdecoderserverclient_real": dict(mmp=_ALF + "AlfDecoderServerClient/src/alfdecoderserverclient.mmp", alf=2, allow_wsd=_ALFTRACE, flags=_ALFT_FLAGS, **_ALFLOG,
                                        **({"defs": ["PH_ALF_PHVG"]} if os.environ.get("ALF_PHVG") == "1" and not _ALFLOG else {}),
                                        **({"drop": ["alfdecoderserverclient.cpp"], "extra": ["toolchain/shims/alf_fix/alfdecoderserverclient_ph.cpp"]} if _ALFTRACE else {}),
                                        rom="alfdecoderserverclient_real.dll", rom_as="alfdecoderserverclient.dll"),
    "alfappservercore": dict(mmp=_ALF + "ServerCore/Src/alfappservercore.mmp", alf=2, allow_wsd=_ALFTRACE, flags=_ALFT_FLAGS, **_ALFLOG,
                             drop=["alfstreamerserver.cpp"] + (["alfappui.cpp", "alfbridge.cpp", "alfscreen.cpp"] if _ALFTRACE else []),
                             extra=["toolchain/shims/alf_fix/gen/alfstreamerserver_ph.cpp"] + ([_AG + "alfappui_ph.cpp", _AG + "alfbridge_ph.cpp", _AG + "alfscreen_ph.cpp"] if _ALFTRACE else []) + [
                                                    "toolchain/shims/alf_fix/huitexturehandle_local.cpp"]),
    "inlinetext": dict(mmp="oss.FCL.sf.os.textandloc/textandlocutils/inlinetext/group/InlineText.mmp", alf=2),   # (alfclient.mmp links it)
    # the WSERV render stage (ECOM 200184B5/200184B6, wsini [alf]) -> streams the window draw commands to the alf server
    # build-only (alf="build": never in a ROM): its alfcommanddescriptions.rsg is included by alfrssendbuffer.cpp
    "alfcommanddebug": dict(mmp=_ALF + "AlfCommandDebug/group/alfcommanddebug.mmp", alf="build"),
    "alfrenderstage": dict(mmp=_ALF + "AlfRenderStage/group/alfrenderstageplugin.mmp", alf=2, allow_wsd=_ALFTRACE, flags=_ALFT_FLAGS,
                     **({"drop": ["alfrssendbuffer.cpp"], "extra": ["toolchain/shims/alf_fix/alfrssendbuffer_ph.cpp"]} if _ALFTRACE else {}), **_ALFLOG),
    # the Hitchcock VG renderer (ECOM 200184B7) over libEGL + libOpenVG (the O1-O4 stack)
    # KAlfCompositionControllerIfUid, which alfrenderstage resolves through the next stage). Board copy of
    # openwfcwrapper.cpp = the NGA stage's rotated-UI-surface delta (toolchain/shims/wsstdplugin_board) re-applied.
    # the null GOOM monitor + the session calls alfrenderstage makes (Connect(TRequestStatus&) was an unimpl stub): twin staged as
    # goommonitor.dll at ALF>=s2 (replaced by goommonitor_real at s3). The default ROM's null is untouched.
    "goommonitor_alfnull": dict(mmp=os.path.join(OS_ROOT, "toolchain/shims/goommonitor_null/goommonitor_null.mmp"), alf=2, defs=["PH_ALF_GOOM"],
                                rom="goommonitor_alfnull.dll", rom_as="goommonitor.dll",
                                **{"def": "oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/goommonitor/eabi/goommonitoru.DEF"}),
    # libEGL.dll with the "PH: alf: libEGL ..." trace lines on the OpenVG route (src/phgl/libegl_front.cpp -DPH_ALF_TRACE); staged as
    # libEGL.dll at ALF>=s2 in place of the default one, which is left untouched
    "libegl_alf": dict(mmp=os.path.join(OS_ROOT, "src/phgl/libegl.mmp"), nga=True, alf=2, rom="libEGL_alf.dll", rom_as="libEGL.dll",
                       defs=["__declspec(x)="] + (["PH_ALF_TRACE"] if _ALFTRACE else []),
                       **{"def": "oss.FCL.sf.os.graphics/egl/eglinterface/eabi/libegl14U.def"}),
    "alfopenwfrs": dict(mmp=_ALF + "plugins/openwfcrs/group/alfopenwfrs.mmp", alf=1, nga=True, allow_wsd=_ALFTRACE,
                        drop=["openwfcwrapper.cpp"] + (["displayrenderstage.cpp", "displayrenderstagefactory.cpp"] if _ALFTRACE else []),
                        extra=["toolchain/shims/alfopenwfrs_board/openwfcwrapper.cpp"] + (
                            ["toolchain/shims/alf_fix/gen/displayrenderstage_ph.cpp", "toolchain/shims/alf_fix/displayrenderstagefactory_ph.cpp"] if _ALFTRACE else [])),
    # goommonitor, the window-transition effects)
    "alfhintplugin": dict(mmp=_ALF + "AlfHintPlugin/group/AlfHintPlugin.mmp", alf=3),
    "alfhintplugindrawer": dict(mmp=_ALF + "AlfHintPlugin/group/AlfHintPluginDrawer.mmp", alf=3),
    "alfoogmplugin": dict(mmp=_ALF + "plugins/alfoogmplugin/group/alfoogmplugin.mmp", alf=3),
    "alfcrpplugin": dict(mmp=_ALF + "plugins/alfcrpplugin/group/alfcrpplugin.mmp", alf=3),
    "alfappfwproxy": dict(mmp=_ALF + "ServerStub/Proxy_src/alfappfwproxy.mmp", alf=3),
    "goommonitor_real": dict(mmp=_ALF + "goommonitor/group/goommonitor.mmp", alf=3, rom="goommonitor_real.dll", rom_as="goommonitor.dll"),
    "alftranseffectplugin": dict(mmp=_ALF + "plugins/alftranseffect/alftranseffectplugin/group/alftranseffectplugin.mmp", alf=3),
    "tfxsrvplugin": dict(mmp=_ALF + "plugins/alftranseffect/alftfxserverplugin/group/AlfTfxSrvPlugin.mmp", alf=3),
    "huirenderpluginvg": dict(mmp=_ALF + "coretoolkit/rendervg10/group/huirendervg10.mmp", alf=2, vfp=True, defs=["__SOFTFP="],
                          allow_wsd=True, libs=["surfacemanager.lib", "surfaceupdateclient.lib", "hal.lib"],   # phvgwindow.h (the alf window surface)
                          extra=["toolchain/shims/alf_fix/huitexturehandle_local.cpp"] + ([_AG + "HuiVg10RenderPlugin_ph.cpp", _AG + "HuiVg10RenderSurface_ph.cpp", _AG + "huivg10canvasrenderbuffer_ph.cpp"] if _ALFTRACE else []),
                          drop=["huivg10renderplugin.cpp", "huivg10rendersurface.cpp", "huivg10canvasrenderbuffer.cpp"] if _ALFTRACE else []),
    # library it links; the wrtharvester CPS plug-in publishes installed widgets to the homescreen.
    "widgetregistryclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/widgetregistry/group/WidgetRegistryClient.mmp"),
    # (both #include <libxml2_*.h> bare: an S60 platform build had stdapis/libxml2 on its system path)
    "widgetregistry": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/widgetregistry/group/WidgetRegistryServer.mmp", **_WRTXML),
    "widgetinstaller": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/widgetinstaller/group/WidgetInstaller.mmp", **_WRTXML),
    "wrtharvester": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/wrtharvester/group/wrtharvester.mmp"),
    # the WRT runtime: WidgetUi (the widget host app, on the S60 WebKit engine), the widget JS objects
    # (widgetengine: `widget.*`, `menu.*`), the launcher apparc runs for a widget's UID, and the first-boot
    # pre-installer (z:\data\WidgetPreInstall\<bundle id>\<root>\ -> c:\private\10282822\, once)
    "widgetui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/widgets/widgetapp/group/WidgetUi.mmp", x32_flags=WEBKIT_NOT_UNIX),
    "widgetengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/widgetengine/group/WidgetEngine.mmp", x32_flags=WEBKIT_NOT_UNIX),
    "jsdevice": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/device/group/Device.mmp", x32_flags=WEBKIT_NOT_UNIX),
    "widgetlauncher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/widgets/widgetlauncher/group/WidgetLauncher.mmp"),
    # Indication/Loading messages in Messaging's "Service messages" (PushMtm.iby, PushMtmResources.iby)
    # the Symbian WAP-push framework under it (wappush.iby: pushwatcher is a messaging watcher; the app handler,
    # security and SI/SL content handlers are ECom plug-ins no import reaches) + what the push MTM links
    "miscpushmsgutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/MiscPushMsgUtils/group/Miscpushmsgutils.mmp"),
    "wapmsgcli": dict(mmp="oss.FCL.sf.os.cellularsrv/wapstack/wapmessageapi/group/wapmsgcli.mmp"),
    "watcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/watcherfw/group/watcher.mmp"),   # debugging the watchers: add drop=["watcherlog.cpp"], extra=["toolchain/shims/watcher_dbg/watcherlog.cpp"] (their log -> the serial log, "[wlog]")
    "wappushutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/pushutils/group/pushutils.mmp"),
    "pushmsgentry": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/PushMsgEntry/group/Pushmsgentry.mmp"),
    "sislpushmsgutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/SISLPushMsgUtils/group/SISLPushMsgUtils.mmp"),
    "pushwatcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/pushwatcher/group/pushwatcher.mmp"),
    # and File Manager's AddSendOptionL then SetItemSpecific(EFileManagerSend) -> EIKCOCTL 8 (EEikPanicNoSuchMenuItem) when a file is
    # resolver, which fills the central-repository cache SendUI reads.
    "senduiwatcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/senduiservices/watcher/group/SendUiWatcher.mmp"),
    "senduiserviceresolver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/senduiservices/serviceresolver/group/senduiserviceresolver.mmp"),
    "senduilauncher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/senduiservices/launcher/group/senduilauncher.mmp"),   # runs the chosen send service (RSendUiServiceClient::LaunchSendUiL)
    "genericmtmplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/sendui/genericmtmplugin/group/GenericMtmPlugin.mmp"),   # the sending-service provider (interface 0x101F87F3): every installed MTM (SMS, MMS, BT, e-mail) as a SendUI service
    "pushapphandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/plugins/group/PushAppHandler.mmp"),
    "pushsecurity": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/plugins/group/PushSecurity.mmp"),
    "pushcontenthandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/plugins/group/PushContentHandler.mmp"),
    "sislcontenthandlers": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/SISLContentHandlers/group/Sislcontenthandlers.mmp"),
    "sl1_0stringdict00": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/wappushfw/plugins/group/Sl1_0StringDict00.mmp"),
    "httpresponse": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerprotocols/wappushsupport/mmpfiles/HTTPResponse.mmp"),
    "provisioningengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/omaprovisioning/provisioning/ProvisioningEngine/Group/ProvisioningEngine.mmp"),
    "provisioningparser": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/omaprovisioning/provisioning/ProvisioningParser/Group/ProvisioningParser.mmp"),
    # Messaging's error watcher (SMS/MMS error notes, message-store garbage collection -> the Push MTM's
    "msgerrorwatcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgerrorwatcher/group/MsgErrorWatcher.mmp"),
    "pushmtmclisrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/pushmtm/Group/PushMtmCliSrv.mmp"),
    "pushmtmutil": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/pushmtm/Group/PushMtmUtil.mmp"),
    "pushmtmui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/pushmtm/Group/PushMtmUi.mmp"),
    "pushviewer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/pushmtm/Group/PushViewer.mmp"),
    "pushmtmpushcontenthandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/pushmtm/Plugins/Group/PushMtmPushContentHandler.mmp"),
    "pushmtmwhitelistadapter": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/pushmtm/Plugins/Group/PushMtmWhiteListAdapter.mmp"),
    "widgetstartup": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/widgets/widgetstartup/Group/WidgetStartup.mmp"),
    "widgetpreinstaller": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/widgets/widgetpreinstaller/Group/WidgetPreInstaller.mmp",
                               # its USERINCLUDE ../../WidgetInstaller/inc (widgets/) lacks WidgetInstaller.h: webengine's has it
                               flags=["-idirafter", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.web/webengine/widgetinstaller/Inc")],
                               # the release's own sample widget (examples.WebRT, EPL), unpacked where the pre-installer looks
                               rom_data=["data\\WidgetPreInstall\\org.symbian.eggtimer\\Eggtimer\\" + f.replace("/", "\\") + "=" +
                                         "baseline/symbian-2010/oss.FCL.examples.WebRT/Eggtimer/" + f
                                         for f in ("Info.plist", "index.html", "basic.css", "basic.js", "Icon.png",
                                                   "images/hourglass_black_semi.png")]),
    # Menu Content Service (the menu's data source): client, engine, server, utils, handler plug-in,
    # and its LIW (SAPI) provider + service -- hnengine talks to MCS only through LIW
    "mcsmenuutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/menucontentsrv/group/mcsmenuutils.mmp", flags=[_KEEPNULL]),
    "mcsmenu": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/menucontentsrv/group/mcsmenu.mmp", flags=[_KEEPNULL]),
    "mcsmenueng": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/menucontentsrv/group/mcsmenueng.mmp", flags=[_KEEPNULL]),
    # SAT (SIM Application Toolkit): the REAL client library (RCL_3 phonesrv) + OURS null handler in
    # place of mcssathandler.cpp (it hard-requires a SAT server) -- toolchain/shims/mcssathandler_null
    "satclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/satengine/SatServer/SatClient/group/SatClient.mmp"),
    "etelsat": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserver/etelsimtoolkit/group/etelsat.mmp"),   # RSat (the SIM toolkit ETel API), <- satengine
    "satshellcntrl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/satui/satapp/Group/SatShellCntrl.mmp"),   # the SAT server loads it (its UI-client handler), <- satengine
    "satserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/satengine/SatServer/SatServer/group/SatServer.mmp"),
    "satengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/satengine/SatServer/Engine/group/SatEngine.mmp"),
    "sateventmonitors": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/satengine/SatServer/EventMonitors/group/EventMonitors.mmp"),
    "satsystemstate": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/satengine/SatServer/SystemState/group/SatSystemState.mmp"),
    "mcssathandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/menucontentsrv/group/mcssathandler.mmp", flags=[_KEEPNULL], drop=["mcssathandler.cpp"],
                          extra=["toolchain/shims/mcssathandler_null/mcssathandler_null.cpp"]),
    "mcsextendedmenu": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/menucontentsrv/group/mcsextendedmenu.mmp", flags=[_KEEPNULL]),
    "mcsmenusrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/menucontentsrv/group/mcsmenusrv.mmp", flags=[_KEEPNULL],
                       rom="mcsmenuserver.exe"),   # RMenu starts it by its TARGET name
    # tree skew: S^3's widget registry added the EMimeType property; the 2010 sf.mw.web enum predates
    # it (widget UNINSTALL path only; no widget registry here) -> the registry's own "invalid id"
    "mcsmenuhandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/menucontentsrv/group/mcsmenuhandler.mmp",
                           flags=[_KEEPNULL]),
    "mcsservice": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_menucontent/mcsservice/group/mcsservice.mmp", flags=[_KEEPNULL]),
    "mcsprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_menucontent/group/mcsprovider.mmp", flags=[_KEEPNULL]),
    "mmfolderuiextensionplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/menufwui/mmextensions/mmfolderuiextension/group/mmfolderuiextensionplugin.mmp", **_XDOM),
    "foldersuite": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/menufw/menusuites/foldersuite/group/foldersuite.mmp", rsc_only=True),   # TARGETTYPE none: data only
    "xmlinterface": dict(mmp="oss.FCL.sf.os.xmlsrv/xml/xmlparser/group/XMLInterface.mmp"),
    # DBMS (Notepad keeps its notes in a DBMS database): client + server
    "edbms": dict(mmp=f"{PDATA}/persistentstorage/dbms/bmake/EDBMS.MMP",
                  x32_flags=["-DPORT_FOFF_DYNAMIC"]),      # D32DBAS.INL: _FOFF(TDbBlob, iInline[IsInline() ? Size() : 0])
    "edbsrv": dict(mmp=f"{PDATA}/persistentstorage/dbms/bmake/EDBSRV.MMP"),
    # AIW's custom ECOM resolver: without it every CAiwServiceHandler::AttachL leaves -1 (app exits at start)
    "aiwresolver": dict(mmp=f"{CUI}/applicationinterworkingfw/ServiceHandler/group/AiwResolver.mmp"),
    # CTextResolver's error texts (CAknAppUi::HandleErrorL opens them; resources only)
    "errorresgt": dict(mmp=f"{CUI}/commonuisupport/errorresolverdata/group/ErrorResGt.mmp", rsc_only=True),
    # CommonUI's find-item menu (every S60 text editor/viewer) builds its "Call" sub-menu data at construction
    "aiwdialdata": dict(mmp="oss.FCL.sf.mw.phonesrv/phoneclientserver/aiwdialdata/group/aiwdialdata.mmp"),
    "cenrepdatabase": dict(mmp="oss.FCL.sf.mw.phonesrv/convergedcallengine/cenrepdatabase/group/cenrepdatabase.mmp"),
    "serviceprovidersettings": dict(mmp="oss.FCL.sf.mw.phonesrv/convergedcallengine/spsettings/group/serviceprovidersettings.mmp"),
    "commonui": dict(mmp=f"{CUI}/commonuis/CommonUi/group/CommonUI.mmp"),
    "sendui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/senduiservices/group/Sendui.mmp"),
    # organizer = the BRANCH_RCL_3 tip worktree (baseline/symbian-rcl3); no pin needed since Avkon itself
    "npdlib": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/notepad/notepad1/group/NpdLib.mmp",
                   flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),
    "notepad": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/notepad/notepad1/group/Notepad.mmp"),
    "calcsoft": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.utils/extras/calcsoft/group/Calcsoft.mmp"),
    # Help (the context-sensitive help viewer) + its engine and the Symbian help-file model
    # (its HTML topic view includes the browser control's headers, whose <ApEngineConsts.h> only the
    # API reference tree carries -- as a fallback after every real include dir)
    "cshelp": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.app.helps/csxhelp/group/cshelp.mmp",
                   flags=["-idirafter", os.path.join(SYMBIAN, "symbian-2010/oss.API_REF.Public_API/epoc32/include/mw")],
                   # one GCC delta (default args on a function-pointer typedef): toolchain/shims/cshelp_gcc
                   drop=["csxhhtmltopiccontainer.cpp"],
                   extra=["toolchain/shims/cshelp_gcc/CSXHHtmlTopicContainer.cpp"]),
    "helpengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.helps/csxhelp/group/HelpEngine.mmp"),
    "hlpmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.helps/symhelp/helpmodel/group/HLPMODEL.MMP"),
    "ezip": dict(mmp=f"{OSSRV}/compressionlibs/ziplib/group/ezip.mmp"),
    # Notes' read-only viewer (opens a memo/text file handed over by apparc)
    "npdviewer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/notepad/notepad1/group/NpdViewer.mmp"),
    # the alarm editor (Clock's "new alarm", as its own application)
    "alarmeditorapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockui/alarmeditorapp/group/alarmeditorapp.mmp"),
    # GCC delta header, force-included (its guard wins): CurrentValueTextLC(){} returns nothing (return census)
    "clockalarmeditor": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockui/uilayer/clockalarmeditor/group/clockalarmeditor.mmp",
                             flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/clockalarmeditor_gcc/clockalarmeditorimpl.h")]),
    "timezonelocalization": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/tzservices/tzloc/group/TimeZoneLocalization.mmp"),
    # Search: the UI app + the search framework client
    "srchuiapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchui/searchapp/srchuiapp/group/srchuiapp.mmp"),
    "srchuigenui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchui/searchapp/srchuigenui/group/srchuigenui.mmp"),
    "srchuicommon": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchui/searchapp/srchuicommon/group/srchuicommon.mmp"),
    "searchclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchfw/coresearchfw/group/searchclient.mmp"),
    "searchutilities": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchfw/coresearchfw/group/searchutilities.mmp"),
    # the search server (searchclient's RSessionBase: without it the Search UI panics KERN-EXEC 0) and
    # its content plug-ins (ECOM) + the UI's service / result-viewer plug-ins
    "searchserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchfw/coresearchfw/group/searchserver.mmp"),
    "notessearchplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchfw/plugins/notessearchplugin/group/notessearchplugin.mmp"),
    "filessearchplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchfw/plugins/filesplugin/group/filesplugin.mmp"),
    "srchuiresviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchui/searchapp/plugins/resultviewerplugin/group/srchuiresviewplugin.mmp"),
    # About (version + legal texts)
    # registered for their resources first (clock.rsg / FileManager.rsg / srchuiindevice.rsg are
    # included by clockalarmeditor / srchuigenui); their own builds are the next apps in the queue
    # Clock's server (RClkSrvInterface starts it: Clock exited -15 without) + its time-source interface,
    # the time-zone resolver and the NITZ plug-in (network time from telephony; the stub modem sends none)
    "clockserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockengines/clockserver/server/group/clockserver.mmp"),
    "clocktimesourceinterface": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockengines/clocktimesourceinterface/group/clocktimesourceinterface.mmp"),
    "clocktimezoneresolver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockengines/clocktimezoneresolver/group/clocktimezoneresolver.mmp"),
    "clocknitzplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockengines/clocknitzplugin/group/clocknitzplugin.mmp"),
    # (key clockapp: "clock" is classicui's clock.dll, the clock control library uiklaf imports)
    "clockapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockui/uilayer/group/clock.mmp", rom="clock.exe"),
    "filemanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.files/filemanager/group/FileManager.mmp"),
    "srchuiindeviceserviceplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/searchui/searchapp/plugins/indevicesearch/group/srchuiindeviceserviceplugin.mmp"),
    # Profiles (the profile list + settings) -- on the profiles engine (mw.gsprofilesrv)
    "profileapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.profile/profilesapplication/Profiles/ProfileApp/group/ProfileApp.mmp"),
    "profilesettingsview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.profile/profilesapplication/Profiles/ProfileApp/group/ProfileSettingsView.mmp"),
    # profiles engine + FileList, MDS client, MMF client + A3F DevSound, the DRM engine clients, OBEX,
    # the messaging test stub; A3F mmfdevsound, SYMBIAN_MULTIMEDIA_A3FDEVSOUND as the Syborg ROM)
    "clkuimodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockui/uimodel/group/clkuimodel.mmp"),   # <- clockapp;
    "clockserverclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockengines/clockserver/client/group/clockserverclient.mmp"),   # <- clkuimodel;
    "clkdatetimeview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockui/uilayer/clkdatetimeview/group/clkdatetimeview.mmp"),   # <- clockapp;
    "filelist": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/profilesservices/FileList/group/FileList.mmp"),   # <- clkdatetimeview;
    "profileeng": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/profilesservices/ProfileEngine/group/ProfileEng.mmp"),   # <- filelist
    # the S^3 contacts model (BRANCH_RCL_3 app.contacts worktree; 2010 master moved it under qtmobility)
    # groupsql/ = the SQLite build S^3 shipped (cntmodel.mmh #define BUILD_SQL); group/ is the legacy
    # engine, whose view-iterator proxy needs a TItemAtLParams no released header defines
    # the contacts model's default phone-number parser (ECom 0x102035FC of interface 0x102035FA): VPbkEng's phone-number matcher (what Messaging's
    "cntphone": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/contactsmodel/cntphone/cntphone.mmp"),
    "cntmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/contactsmodel/groupsql/CNTMODEL.MMP",
                     defs=["__declspec(x)="]),   # ARM (__EABI__): NONSHARED = RVCT __declspec(notshared)
    "cntsrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/contactsmodel/groupsql/CNTSRV.MMP"),
    "cntplsql": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/contactsmodel/groupsql/cntplsql.mmp"),
    "pbkeng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook/group/PbkEng.mmp"),   # <- profileeng
    "cntview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/contactsmodel/cntview/cntview.mmp"),   # <- pbkeng;
    "diskspacereserver": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/commonengine/diskspacereserver/group/diskspacereserver.mmp"),   # <- pbkeng
    "dsclient": dict(mmp="oss.FCL.sf.os.devicesrv/dosservices/dosserver/group/dsclient.mmp"),   # <- diskspacereserver
    # the DOS server behind it (RSharedDataClient::Connect -> RDosServer; Phonebook2's contact store
    # leaves KErrCouldNotConnect without it) + its reference DSY (named by z:/private/101f6efa/dosserver.ini)
    # + the disk-space reservation extension -- the set the Syborg ROM ships
    "dossrv": dict(mmp="oss.FCL.sf.os.devicesrv/dosservices/dosserver/group/dosserverdll.mmp"),
    "dosserver": dict(mmp="oss.FCL.sf.os.devicesrv/dosservices/dosserver/group/dosserverexe.mmp"),
    # OURS: null DSY (the real CommonDSY's service-logic headers were never released) -- toolchain/shims/commondsy_null
    "commondsy": dict(mmp=os.path.join(OS_ROOT, "toolchain/shims/commondsy_null/commondsy_null.mmp"),
                      **{"def": "oss.FCL.sf.os.devicesrv/dosservices/commondsy/EABI/commondsyU.DEF"}),
    "diskspacereservationplugin": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/commonengine/diskspacereservationplugin/group/diskspacereservationplugin.mmp"),
    "bluetooth": dict(mmp="oss.FCL.sf.os.bt/bluetoothmgmt/bluetoothclientlib/btlib/bluetooth.mmp"),   # <- dsclient
    "btdevice": dict(mmp="oss.FCL.sf.os.bt/bluetoothmgmt/btmgr/BTDevice/BTDevice.mmp"),   # <- bluetooth
    "eirclient": dict(mmp="oss.FCL.sf.os.bt/bluetoothmgmt/btmgr/eirclient/group/eirclient.mmp"),   # <- bluetooth
    "hciserverclient": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/hciserverclient/group/hciserverclient.mmp"),   # <- bluetooth
    "btlogger": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btlogger/generic/group/logger.MMP"),   # <- hciserverclient
    # Symbian^3 RCL_3 worktrees of os.wlan + accesssec (master is the Qt line). The engine (wlmserver inside
    # wlmserverexe), its CommsDat interface, the agent + NIF, the management ECOM plug-ins, EAPOL (the WPA2 supplicant).
    # Kernel side (wlanldd.ldd + klibs, OURS wlanpdd) comes through link_ekern.sh, not here.
    **{k: dict(mmp="../symbian-rcl3/oss.FCL.sf.os.wlan/wlan_bearer/" + p, **kw) for k, p, kw in [
        ("wlmserverexe", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlmserverexe.mmp", {}),
        ("wlmserver", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlmserver.mmp", {}),
        ("wlmserversrv", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlmserversrv.mmp", {"defs": ["_DEBUG"]}),   # _DEBUG = the engine's DEBUG() RDebug traces (am_debug.h) while the stack is brought up
        ("wlanscanlist", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlanscanlist.mmp", {}),
        ("wlmplatform", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlmplatform.mmp", {}),
        ("wlandevicesettings", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlandevicesettings.mmp", {}),
        ("wlanmgmtimpl", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlanmgmtimpl.mmp", {}),
        ("wlancontrolimpl", "wlanengine/wlan_symbian/wlanengine_symbian_3.1/group/wlancontrolimpl.mmp", {}),
        ("wlanpsmplugin", "wlanpsmplugin/group/wlanpsmplugin.mmp", {}),
        ("wlandbif", "wlandbif_cd/group/wlandbif.mmp", {}),
        ("wlanagt", "wlanagent/group/wlanagt.mmp", dict(rom="wlanagt.agt")),
        ("wlannif", "wlannwif/group/wlanproto.mmp", dict(rom="wlannif.nif")),
        ("wlannwif", "wlannwif/group/wlannwif.mmp", {}),
    ]},
    "eapolwpxstub": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.accesssec/eapol/eapol_framework/eapol_symbian/group/eapolwpxstub.mmp"),
    # eap_am_tools_symbian.cpp brace-initialises a va_list (GCC: cannot convert int to __va_list) -> the shim copy
    "eapol": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.accesssec/eapol/eapol_framework/eapol_symbian/group/eapol.mmp",
                  drop=["eap_am_tools_symbian.cpp"], extra=["toolchain/shims/eapol_gcc/eap_am_tools_symbian.cpp"],
                  # GCC lowers string code to strlen calls (RVCT inlined them): unresolved, it was a link stub -> the WPA2
                  libs=["estlib.lib"],
                  defs=["USE_EAP_TRACE", "USE_EAP_DEBUG_TRACE", "USE_EAP_HARDWARE_TRACE"] if os.environ.get("EAPOL_TRACE", "0") == "1" else []),
    "wlaneapolif": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.accesssec/eapol/eapol_framework/eapol_symbian/group/wlaneapolif.mmp"),
    # bluetooth.iby + hci.iby). bt_v2.prt is the ESock_Bt worker's protocol module (bt.bt_v2.esk); the HCIv2 framework
    # loads the CoreHCI / HCTL / initialisor / QDP ECOM plug-ins named in z:\private\101f7989\bluetooth\*.ini
    "bt_v2": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btstack/common/bt_v2.mmp", rom="bt_v2.prt"),
    "hcishared": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btstack/HCIShared/group/hcishared.mmp"),
    "bluetoothav": dict(mmp="oss.FCL.sf.os.bt/bluetoothmgmt/bluetoothclientlib/avlib/bluetoothAV.mmp", rom="bluetoothav.dll"),
    "a2dpoptimisation": dict(mmp="oss.FCL.sf.os.bt/bluetoothmgmt/bluetoothclientlib/a2dpoptimisation/a2dpoptimisation.mmp"),
    "btpowercontrol": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/btpowercontrol/group/btpowercontrol.mmp"),
    "corehci_base": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/corehci/group/corehci_base.mmp"),
    "hctl_base": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/hctl/group/hctl_base.mmp"),
    "initialisor_base": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/initialisor/group/initialisor_base.mmp"),
    "qdp_base": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/qdp/group/qdp_base.mmp"),
    "hciutil": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/hciutil/group/hciutil.mmp"),
    "hcicmdq": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/hcicmdq/group/HCICmdQ.mmp"),
    # the command/event classes: the base mmp is an IMPLIB, the Symbian implementation is the DLL (its DEFFILE points
    # at an /epoc32 export that our tree does not stage -> the source def)
    "commandsevents": dict(mmp="oss.FCL.sf.os.bt/bthci/hci2implementations/CommandsEvents/symbian/group/commandsevents_symbian.mmp",
                           **{"def": "oss.FCL.sf.os.bt/bthci/bthci2/CommandsEvents/eabi/commandseventsU.def"}),
    "corehci_symbian": dict(mmp="oss.FCL.sf.os.bt/bthci/hci2implementations/corehcis/symbian/group/corehci_symbian.mmp"),
    "qdp_symbian": dict(mmp="oss.FCL.sf.os.bt/bthci/hci2implementations/qdps/symbian/group/qdp_symbian.mmp"),
    "initialisor_symbian": dict(mmp="oss.FCL.sf.os.bt/bthci/hci2implementations/initialisors/symbian/group/initialisor_symbian.mmp"),
    "btmanserver": dict(mmp="oss.FCL.sf.os.bt/bluetoothmgmt/btmgr/BTManServer/BTManServer.mmp"),
    "sdpdatabase": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btsdp/database/sdpdatabase.mmp"),
    "sdpagent": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btsdp/agent/sdpagent.mmp"),
    "sdpserver": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btsdp/server/sdpserver.mmp"),
    "btextnotifiers": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btextnotifiers/src/BTExtNotifiers.mmp"),
    # the btsnoop HCI frame logger the HCTLs' HCI_LOG_FRAME hooks load by name (writes c:\logs\bt\hci.log... when present)
    "hciframelogger": dict(mmp="oss.FCL.sf.os.bt/bluetooth/btlogger/btsnoophcilogger/group/hciloggerbtsnoop.mmp"),
    # Communication Launcher client (the search result viewer opens contacts through it)
    "ccaclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/ccapplication/ccapp/ccaclient/group/ccaclient.mmp"),
    "conarc": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/filehandling/fileconverterfw/group/CONARC.MMP"),   # <- cntmodel;
    "versit": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/pimappsupport/vcardandvcal/group/VERSIT.MMP"),   # <- cntmodel;
    "sssettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/cellular/SSSettings/group/SSSettings.mmp"),   # <- profileeng;
    "customapi": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserverplugins/common_tsy/customapi/group/CustomAPI.mmp"),   # <- sssettings
    "schsvr": dict(mmp="oss.FCL.sf.os.ossrv/genericservices/taskscheduler/group/SCHSVR.MMP"),   # <- profileeng
    "contentlistingframework": dict(mmp="oss.FCL.sf.mw.mds/clfwrapper/group/contentlistingframework.mmp"),   # <- filelist
    "mdeclient": dict(mmp="oss.FCL.sf.mw.mds/metadataengine/client/group/mdeclient.mmp"),   # <- contentlistingframework
    "mdccommon": dict(mmp="oss.FCL.sf.mw.mds/metadataengine/common/group/mdccommon.mmp"),   # <- mdeclient
    "harvesterclient": dict(mmp="oss.FCL.sf.mw.mds/harvester/client/group/harvesterclient.mmp"),   # <- contentlistingframework
    "mediaclientvideo": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/client/MediaClientVideo.mmp"),   # <- filelist
    "mediaclientutility": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/client/MediaClientUtility.mmp"),   # <- mediaclientvideo
    "mmfcontrollerframework": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/ControllerFramework/MmfControllerFramework.mmp"),   # <- mediaclientutility
    "mmcommonutils": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmutilitylib/mmpfiles/MMCommonUtils.mmp"),   # <- mmfcontrollerframework
    # CFbsBitmaps; ECOM 0x1020707F, ROM-only resolver) + the S60 test app that shows its viewfinder. x86 first; nothing below ECam exists in the release.
    "ecam": dict(mmp="oss.FCL.sf.os.mm/imagingandcamerafws/camerafw/mmpfiles/ECam.mmp"),
    "ecampluginsupport": dict(mmp="oss.FCL.sf.os.mm/imagingandcamerafws/camerafw/mmpfiles/ECamPluginSupport.mmp"),
    "ecamtestplugin": dict(mmp="oss.FCL.sf.os.mm/mmplugins/cameraplugins/mmpfiles/TestCamera.mmp"),
    # DevVideo (CMMFDevVideoPlay + the hw-device base classes + ECOM plugin glue): VDU V4 -- phevdec is a decoder hw device
    "devvideo": dict(mmp="oss.FCL.sf.os.mm/mmhais/videohai/devvideo/mmpfiles/DevVideo/DevVideo.mmp"),
    "puresolverutils": dict(mmp="oss.FCL.sf.os.mm/mmdevicefw/mdf/mmpfiles/puresolverutils.mmp"),   # <- devvideo (FindDecodersL asks the codec-API resolver for PUs first)
    "codecapiresolver": dict(mmp="oss.FCL.sf.os.mm/mmdevicefw/mdf/mmpfiles/codecapiresolver.mmp"),   # the ECOM resolver those queries use
    "mmfdrmpluginserverproxy": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/SecureDRM/MMPFiles/Client/MmfDrmPluginServerProxy.mmp"),   # <- mmfcontrollerframework
    "mmfstandardcustomcommands": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/ControllerFramework/MmfStandardCustomCommands.mmp"),   # <- mediaclientutility
    "mmcommon": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmutilitylib/mmpfiles/MMCommon.mmp"),   # <- mediaclientutility
    "mediaclient": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/client/MediaClient.mmp"),   # <- mediaclientvideo
    "drmcommon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DRMCommon.mmp"),   # <- filelist
    "drmdcf": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DrmDcf.mmp"),   # <- drmcommon
    # CMLA: the build's own CryptoPrep.flm copies stub/CmlaCryptoStub.cpp to build/cmlacrypto.cpp when
    "drmcrypto": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DrmCrypto.mmp", drop=["cmlacrypto.cpp", "omacrypto.cpp"],
                      extra=["baseline/symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/drmcrypto/stub/CmlaCryptoStub.cpp",
                             "baseline/symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/drmcrypto/src/OmaCrypto.cpp"]),
    "drmkeystorage": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DrmKeyStorage.mmp"),   # <- drmcrypto
    "bnf": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerprotocols/wapbase/mmpfiles/bnf.mmp"),   # <- drmdcf
    "wnode": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerprotocols/wapbase/mmpfiles/wnode.mmp"),   # <- bnf
    "wutil": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerprotocols/wapbase/mmpfiles/wutil.mmp"),   # <- bnf
    "drmrights": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DrmRights.mmp"),   # <- drmcommon
    "drmserverinterfaces": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DrmServerInterfaces.mmp"),   # <- drmcommon
    "drmutility": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/group/DRMUtility.mmp"),   # <- filelist
    "drmaudioplayutility": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/drmaudioplayer/DRMPlayUtility/group/DRMPlayUtility.mmp"),   # <- filelist
    "mmfdevsound": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/mmfdevsound.mmp"),   # <- drmaudioplayutility;
    "mmfdevsoundproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/mmfdevsoundproxy.mmp"),   # <- mmfdevsound (A3F)
    # mdasoundadapter (RMdaDevSound -> RSoundSc -> the AC'97 PDD). The set of stemkit's syborg.tmp1.oby.
    "mmfaudioserverstart": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/devsound/a3fsrvstart/mmpfiles/mmfaudioserverstart.mmp"),
    "mmfaudioserver": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/mmfaudioserver.mmp"),
    "mmfaudioserverfactory": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/mmfaudioserverfactory.mmp"),
    "mmfaudioserverproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/mmfaudioserverproxy.mmp"),
    "mmfdevsoundserver": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/mmfdevsoundserver.mmp"),
    "mmfdevsoundadaptor": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/devsoundadaptation.mmp"),
    "a3ffourcclookup": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/a3ffourcclookup.mmp"),
    "a3ffourccconvertorplugin": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3fdevsound/mmpfiles/a3fFourCCConvertorPlugin.mmp"),
    "a3f_trace_heap": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3ftrace/mmpfiles/a3f_trace_heap.mmp"),
    "tonedata": dict(mmp="oss.FCL.sf.os.mmaudio/devsound/a3facf/mmpfiles/tonedata.mmp"),
    "mmfserverbaseclasses": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/server/mmfserverbaseclasses.mmp"),
    "audioutils": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/utils/audioutils.mmp"),
    "mmfcodeccommon": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/codecs/MmpFiles/MMFCodecCommon.mmp"),
    "devsoundadaptationinfo": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3fdevsoundcustomisation/mmpfiles/devsoundadaptationinfo.mmp"),
    "devsoundadaptationinfoconsts": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3fdevsoundcustomisation/mmpfiles/devsoundadaptationinfoconsts.mmp"),
    "audiocontextfactory": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/audiocontextfactory.mmp"),
    "audiocomponentframework": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/audiocomponentframework.mmp",
                                    **_a0("SYMBIAN_MULTIMEDIA_ENABLE_LOGGING")),
    "logicalaudiocodec": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/logicalaudiocodec.mmp"),
    "logicalaudiodevicesink": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/logicalaudiodevicesink.mmp"),
    "logicalaudiodevicesource": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/logicalaudiodevicesource.mmp"),
    "logicalaudiogaincontrol": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/logicalaudiogaincontrol.mmp"),
    "logicalaudiostream": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/logicalaudiostream.mmp"),
    "logicalbuffersink": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/logicalbuffersink.mmp"),
    "logicalbuffersource": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/a3facl/mmpfiles/logicalbuffersource.mmp"),
    "mmrcclient": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmresourcemgmt/mmresctrl/mmpfiles/mmrcclient.mmp"),
    "mmrcserver": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmresourcemgmt/mmresctrl/mmpfiles/mmrcserver.mmp",
                       **_MMRC_MIX, **_a0("SYMBIAN_MULTIMEDIA_ENABLE_LOGGING")),
    "audiocodec": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/audiocodec.mmp",
                       # halved every gain, so DevSound's maximum reached the sound driver as -6.5 dB
                       drop=["audiocodec.cpp"], extra=["toolchain/shims/audiocodec_gainfix/audiocodec.cpp"]),
    "audiodevicesink": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/audiodevicesink.mmp"),
    "audiodevicesource": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/audiodevicesource.mmp"),
    "audiogaincontrol": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/audiogaincontrol.mmp"),
    "audiostream": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/audiostream.mmp"),
    "buffersink": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/buffersink.mmp"),
    "buffersource": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/buffersource.mmp"),
    "tonehwdevice": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmhais/refacladapt/mmpfiles/tonehwdevice.mmp",
                         drop=["tonehwdevice.cpp"], extra=["toolchain/shims/tonehwdevice_board/tonehwdevice.cpp",
                                                           "toolchain/shims/tonehwdevice_board/nokiatoneseq.cpp",
                                                           # U5: .rng ringing tones -- the release's Smart Messaging parser
                                                           "toolchain/shims/tonehwdevice_board/ringbcconverter.cpp"],
                         flags=["-I", os.path.join(_resolve_src(_RINGBC), "inc"), "-I", os.path.join(_resolve_src(_RINGBC), "src")]),
    # Q0 (see _MIXQ0 above): the userland mixer behind the adapter's RSoundSc; exports (mdasoundadapter.def) unchanged
    "mdasoundadapter": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/mmdevicefw/mdf/mmpfiles/audio/mdasoundadapter.mmp",
                            **_MDASND_MIX, **_a0("SYMBIAN_SOUNDADAPTER_DEBUG", "PHMIX_TRACE")),
    "mmfswcodecwrapper": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/devsound/devsoundrefplugin/mmpfiles/swcodecwrapper/mmfswcodecwrapper.mmp",
                              **_a0("_SCW_DEBUG")),
    "mmfswaudiodevice": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/devsound/devsoundrefplugin/mmpfiles/swcodecwrapper/mmfswaudiodevice.mmp"),
    "mmfaudiohwdevices": dict(mmp="oss.FCL.sf.adapt.NokiaMMAudioReference/devsound/devsoundrefplugin/mmpfiles/plugin/mmfAudioHwDevices.mmp"),
    # ships) + its custom-interface/routing deps; the release's own stubs where the real part stayed closed
    "mmfaudiocontroller": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/wavplaycontrollerplugin/group/MmfAudioController.mmp"),
    # (board delta: the file source stays primed after the tone data is read -- CMMFFile::SourceStopL deleted a
    # by-name source's file and Prime's DRM check then left KErrNotReady)
    "mmfaudiotonecontroller": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiotonecontrollerplugin/group/mmfaudiotonecontroller.mmp",
                                   drop=["mmfaudiotonecontroller.cpp"],
                                   extra=["toolchain/shims/tonecontroller_board/mmfaudiotonecontroller.cpp"],
                                   flags=["-I", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiotonecontrollerplugin/src")]),
    "recrt": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiotonecontrollerplugin/ringtonerecognizer/group/recrt.mmp"),
    "custominterfacebuilder": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmextfw/custominterfaceutility/CustomInterfaceBuilder/Group/CustomInterfaceBuilder.mmp"),
    "messagehandlerfactory": dict(mmp="oss.FCL.sf.adapt.stubs/audiostubs/devsoundextensions_stubs/audioenhancementfactorystubs/MessageHandlerFactory_stub/group/MessageHandlerFactory_stub.mmp"),
    "configurationcomponentsfactory": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/configurationcomponentsfactory/group/ConfigurationComponentsFactory.mmp"),
    "audiooutputcontrolutility": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/audiooutputcontrolutility/AudioOutputControlUtilityBase/group/AudioOutputControlUtility.mmp"),
    "audiooutputcontrolutilityproxy": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/audiooutputcontrolutility/AudioOutputControlUtilityProxy/Group/AudioOutputControlUtilityProxy.mmp"),
    # the MMF's standard plug-ins every controller is handed: the file/descriptor source+sink, the DevSound audio
    # output/input, the format plug-ins ("All format support comes from Format Plugins"), the linear codecs
    "mmfformatbaseclasses": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/plugin_common/MMFFormatBaseClasses.mmp"),
    "mmfstdsourceandsinkplugin": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/plugin_common/MmfStdSourceAndSinkPlugin.mmp"),
    "mmfaudiooutput": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/plugin_common/mmfaudiooutput.mmp"),
    "mmfaudioinput": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/plugin_common/mmfaudioinput.mmp"),
    "mmfwavformat": dict(mmp="oss.FCL.sf.os.mmaudio/mmplugins/mmfwplugins/MMPFiles/Plugin/MmfWAVFormat.mmp"),
    "audiocodecs": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/plugin_common/MMFLinearAudioCodecs.mmp"),
    "playtest": dict(mmp=os.path.join(OS_ROOT, "src/playtest/playtest.mmp")),   # OURS (U4): plays a file, audio test
    # OURS (audio A0/Q0): two concurrent clients -- CMdaAudioOutputStream 440 Hz + CMdaAudioPlayerUtility 880 Hz WAV
    # OURS (Applications grid, first open): brings the already-started launcher forward once at boot so WSERV rasterises its icons
    "phemenuwarm": dict(mmp=os.path.join(OS_ROOT, "src/phemenuwarm/phemenuwarm.mmp")),
    "pheshellsel": dict(mmp=os.path.join(OS_ROOT, "src/pheshellsel/pheshellsel.mmp")),   # UI shell selector at boot (S^3 / S^4, dev menu key 1/6)
    "mixtest": dict(mmp=os.path.join(OS_ROOT, "src/mixtest/mixtest.mmp"), test=True),
    # OURS (audio V0): volume keys -> master volume (P&S KPhAudioMasterVolume, applied by the mixer on the real channel)
    "phvolume": dict(mmp=os.path.join(OS_ROOT, "src/phvolume/phvolume.mmp")),
    # (tools/phorepoc-x86/cxxrt/: build_libstdcxx.sh, build_cxxtest.sh -> thirdparty/x86/cxxrt/*.a). eh=True: x32_exe_eh.ld keeps
    # .eh_frame + the LSDAs in the code segment (eh_register.cpp hands them to libgcc's unwinder).
    "cxx17test": dict(mmp=os.path.join(OS_ROOT, "tools/phorepoc-x86/cxxrt/group/cxx17test.mmp"), allow_wsd=True, static_ctors=True,
                      eh=os.environ.get("CXX_EH", "0") == "1", rtti=True, defs=["__SOFTFP="], stack=0x14000,
                      x32_archives=[f"thirdparty/x86/cxxrt/{a}.a" for a in ("cxx17test", "libstdc++", "libgcc_eh", "libgcc")]),
    # tools/phorepoc-x86/cxxrt/{build_jsc,package_jsc,compat/build_phcompat}.sh + the Symbian shim phsym.cpp (compiled here).
    "jsc": dict(mmp=os.path.join(OS_ROOT, "tools/phorepoc-x86/cxxrt/group/jsc.mmp"), allow_wsd=True, static_ctors=True,
                rtti=False, defs=["__SOFTFP="], stack=0x100000,   # flexible-model PP::MaxUserThreadStack = 1 MB (a bigger one: Create -40 KErrTooBig)
                x32_archives=[f"thirdparty/x86/cxxrt/{a}.a" for a in ("libjscmain", "libJavaScriptCore", "libWTF", "libbmalloc",
                              "libicui18n", "libicuuc", "libicudata", "libphcompat", "libstdc++", "libgcc_eh", "libgcc")]),
    # P1 page test (browser-plan.md section 9): WebCore + PAL + JSC + Skia + deps, prebuilt by build_webcore.sh + package_web.sh
    "phwebtest": dict(mmp=os.path.join(OS_ROOT, "tools/phorepoc-x86/cxxrt/group/phwebtest.mmp"), allow_wsd=True, static_ctors=True,
                      rtti=False, defs=["__SOFTFP="], stack=0x100000,
                      # curl's CA bundle (CurlSSLHandleSymbian.cpp: z:/resource/ssl/ca-bundle.crt)
                      rom_data=["resource\\ssl\\ca-bundle.crt=thirdparty/x86/res/ca-bundle.crt"],
                      x32_archives=[f"thirdparty/x86/cxxrt/web/lib{a}.a" for a in (
                          "phwebmain", "WebCore", "PAL", "JavaScriptCore", "WTF", "bmalloc", "Skia", "freetype", "harfbuzz-icu", "harfbuzz",
                          "xslt", "xml2", "sqlite3", "webpdemux", "webp", "sharpyuv", "jpeg", "curl", "ssl", "crypto", "icui18n", "icuuc", "icudata")]
                      + [f"thirdparty/x86/lib/{a}.a" for a in ("libpng", "libnspsl")]
                      + [f"thirdparty/x86/cxxrt/web/libz.a"]
                      + [f"thirdparty/x86/cxxrt/{a}.a" for a in ("libphcompat", "libstdc++", "libgcc_eh", "libgcc")]),
    # P2 (browser-p2-brctl-map.md): the BrCtl facade over WebKit -- OURS, built to the old engine's ROM name browserengine.dll
    # (BrowserNG / Messaging / Help load it by name, CreateBrowserControlL = ordinal 1). `ph component build phebrctl` REPLACES the old
    # engine's image (ph component build browserengine restores it). Engine archives: build_webcore.sh phengine + package_web.sh.
    "phebrctl": dict(mmp=os.path.join(OS_ROOT, "src/phebrctl/group/phebrctl.mmp"), rom="browserengine.dll",
                     **{"def": os.path.join(OS_ROOT, "src/phebrctl/eabi/browserengineu.def")},
                     allow_wsd=True, static_ctors=True, rtti=False, defs=["__SOFTFP="],
                     x32_archives=[f"thirdparty/x86/cxxrt/web/lib{a}.a" for a in (
                         "phengine", "WebCore", "PAL", "JavaScriptCore", "WTF", "bmalloc", "Skia", "freetype", "harfbuzz-icu", "harfbuzz",
                         "xslt", "xml2", "sqlite3", "webpdemux", "webp", "sharpyuv", "jpeg", "curl", "ssl", "crypto", "icui18n", "icuuc", "icudata")]
                     + [f"thirdparty/x86/lib/{a}.a" for a in ("libpng", "libnspsl")]
                     + ["thirdparty/x86/cxxrt/web/libz.a"]
                     + [f"thirdparty/x86/cxxrt/{a}.a" for a in ("libphcompat", "libstdc++", "libgcc_eh", "libgcc")]),
    # JSC with the JIT; tools/phorepoc-x86/cxxrt/drun-arm.sh builds every archive into thirdparty/arm/)
    "phwebtest_arm": dict(mmp=os.path.join(OS_ROOT, "tools/phorepoc-x86/cxxrt/group/phwebtest.mmp"), rom="phwebtest.exe", device=True,
                          allow_wsd=True, static_ctors=True, rtti=False, defs=["__SOFTFP="], stack=0x100000,
                          rom_data=["resource\\ssl\\ca-bundle.crt=thirdparty/x86/res/ca-bundle.crt"],
                          arm_archives=[f"thirdparty/arm/cxxrt/web/lib{a}.a" for a in (
                              "phwebmain", "WebCore", "PAL", "JavaScriptCore", "WTF", "bmalloc", "Skia", "freetype", "harfbuzz-icu", "harfbuzz",
                              "xslt", "xml2", "sqlite3", "webpdemux", "webp", "sharpyuv", "jpeg", "curl", "ssl", "crypto", "icui18n", "icuuc", "icudata")]
                          + [f"thirdparty/arm/lib/{a}.a" for a in ("libpng", "libnspsl")]
                          + ["thirdparty/arm/cxxrt/web/libz.a"]
                          + [f"thirdparty/arm/cxxrt/{a}.a" for a in ("libphcompat", "libstdc++")]),   # (libgcc: the device link's own -- the toolchain's v7 one has MOVW relocations)
    # drun-arm.sh). rom=browserengine.dll REPLACES the old engine in an ROM_ARCH=arm ROM; `device_rom` = it ships in every ARM
    # ROM (a plain `device` key only ships as the SSM_TEST boot test). Carries the curl CA bundle itself (phwebtest_arm is not in the ROM).
    "phebrctl_arm": dict(mmp=os.path.join(OS_ROOT, "src/phebrctl/group/phebrctl.mmp"), rom="browserengine.dll", device=True, device_rom=True,
                         **{"def": os.path.join(OS_ROOT, "src/phebrctl/eabi/browserengineu.def")},
                         allow_wsd=True, static_ctors=True, rtti=False, defs=["__SOFTFP="],
                         rom_data=["resource\\ssl\\ca-bundle.crt=thirdparty/x86/res/ca-bundle.crt"],
                         arm_archives=[f"thirdparty/arm/cxxrt/web/lib{a}.a" for a in (
                             "phengine", "WebCore", "PAL", "JavaScriptCore", "WTF", "bmalloc", "Skia", "freetype", "harfbuzz-icu", "harfbuzz",
                             "xslt", "xml2", "sqlite3", "webpdemux", "webp", "sharpyuv", "jpeg", "curl", "ssl", "crypto", "icui18n", "icuuc", "icudata")]
                         + [f"thirdparty/arm/lib/{a}.a" for a in ("libpng", "libnspsl")]
                         + ["thirdparty/arm/cxxrt/web/libz.a"]
                         + [f"thirdparty/arm/cxxrt/{a}.a" for a in ("libphcompat", "libstdc++")]),
    # --- MP3 (audio.md "Codec policy"): the S60 MP3 controller (advanced audio controller family) -> DevSound " MP3"
    # -> OUR MP3 HW device -> the release's Aricent MP3 plug-in -> OUR decoder library (minimp3, CC0)
    "mp3audioplaycontrollerplugin": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant/mp3audioplaycontroller/Group/MP3AudioPlayControllerPlugin.mmp"),
    "mp3audioplaycontrollerrecognizer": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant/mp3audioplaycontroller/Group/MP3AudioPlayControllerRecognizer.mmp"),
    "advancedaudiocontroller": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant/AdvancedAudioController/Group/AdvancedAudioController.mmp",
                                    x32_flags=["-Dabs(x)=__builtin_abs(x)"]),   # RVCT inlined abs()
    "mp3audiocontrollerutility": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant/AdvancedAudioControllerUtility/Group/MP3AudioControllerUtility.mmp"),
    "devsoundaudiooutput": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant/DevSoundAudioOutput/Group/DevSoundAudioOutput.mmp"),
    "fileaudiooutput": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant/FileAudioOutput/Group/FileAudioOutput.mmp"),
    "datasourceadapter": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant/DataSourceAdapter/Group/DataSourceAdapter.mmp"),
    "mmdatasourcefactory": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/enhancedmediaclient/Plugins/MultimediaDataSource/DataSourceFactory/group/MultimediaDataSourceFactory.mmp"),
    "streamcontrolcustomcommands": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/enhancedmediaclient/Plugins/StreamControlCustomCommands/group/StreamControlCustomCommands.mmp"),
    # RealAudio metadata needs Helix (not in the release): the mmp's own "RA off" variant (MetaDataParserRADummy)
    "metadatautility": dict(mmp="oss.FCL.sf.mw.mmmw/mmserv/metadatautility/Group/MetaDataUtility.mmp",
                            defs=["__METADATA_UTILITY_DISABLE_RA_SUPPORT"], drop=["MetaDataParserRA.cpp"],
                            extra=["baseline/symbian-2010/oss.FCL.sf.mw.mmmw/mmserv/metadatautility/Src/MetaDataParserRADummy.cpp"]),
    "3gpmp4lib": dict(mmp="oss.FCL.sf.os.mm/mmplugins/lib3gp/impl/group/3gpmp4lib.mmp"),
    "cachesource": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/enhancedmediaclient/Plugins/CacheSource/group/CacheSource.mmp"),
    "datasourceconfigintfc": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/enhancedmediaclient/Plugins/DataSourceConfigIntfc/group/DataSourceConfigIntfc.mmp"),
    "databuffersource": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/enhancedmediaclient/Plugins/DataBufferSource/group/DataBufferSource.mmp"),
    "descriptordatasource": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/enhancedmediaclient/Plugins/DescriptorSource/group/DescriptorDataSource.mmp"),
    "filesource": dict(mmp="oss.FCL.sf.mw.mmmw/mmfenh/enhancedmediaclient/Plugins/FileSource/group/FileSource.mmp"),
    # the release ships FrameTable (seek tables) as a stub, installed under the real name
    "frametable": dict(mmp="oss.FCL.sf.os.mmaudio/frametable_stub/group/frametable_stub.mmp", rom="frametable.dll"),
    "arimp3decmmfcodec": dict(mmp="oss.FCL.sf.adapt.codecs.aricent/mp3_dec/arimp3decmmfcodec/group/arimp3decmmfcodec.mmp"),
    "arimp3decwrapper": dict(mmp=os.path.join(OS_ROOT, "src/codecs/arimp3decwrapper/group/arimp3decwrapper.mmp")),   # OURS (minimp3)
    "mp3hwdevice": dict(mmp=os.path.join(OS_ROOT, "src/codecs/mp3hwdevice/group/mp3hwdevice.mmp")),   # OURS
    "starterclient": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstatereferenceplugins/clayer/group/starterclient.mmp"),   # <- clkdatetimeview
    # ssmuiproviderdll.dll) -- the implib mmp registered here before built an image with NO exports (0/31 real):
    # every CSsmUiSpecific call (SysAp's starter client, SSM, the policies) was an unimpl stub
    "ssmuiproviderdll": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/sysuiprovider/group/sysuiprovider.mmp", rom="ssmuiproviderdll.dll",
                             **{"def": "oss.FCL.sf.os.devicesrv/sysstatemgmt/systemstatereferenceplugins/custcmd/eabi/ssmuiproviderdllu.def"}),   # <- starterclient;
    "featdiscovery": dict(mmp="oss.FCL.sf.os.persistentdata/featuremgmt/featuremgr/group/featdiscovery.mmp"),   # <- clkdatetimeview
    "clockecomlistener": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/clock2/clockengines/utilities/clockecomlistener/group/clockecomlistener.mmp"),   # <- clkdatetimeview;
    "mediafilelist": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/profilesservices/MediaFileList/group/MediaFileList.mmp"),   # <- clkdatetimeview
    "peninputclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/peninputarc/src/peninputclient/peninputclient.mmp"),   # <- mediafilelist;
    "mgfetch": dict(mmp="oss.FCL.sf.mw.metadatasrv/mediamanagementutilities/mediafetch/group/mediafetch.mmp"),   # <- clockapp
    "psmclient": dict(mmp="oss.FCL.sf.os.devicesrv/psmservices/psmserver/group/psmclient.mmp"),
    "psmserver": dict(mmp="oss.FCL.sf.os.devicesrv/psmservices/psmserver/group/psmserver.mmp"),   # <- clockapp
    "iaupdateapi": dict(mmp="oss.FCL.sf.mw.appinstall/iaupdateapi_stub/group/iaupdateapi.mmp"),   # <- clockapp
    "thumbnailmanager": dict(mmp="oss.FCL.sf.mw.imghandling/imagehandlingutilities/thumbnailmanager/thumbnailclient/group/thumbnailmanager.mmp", flags=OST_OFF),   # <- clockapp
    "ihl": dict(mmp="oss.FCL.sf.mw.imghandling/imagehandlinglib/Group/IHL.mmp"),   # <- thumbnailserver
    "iclextjpegapi": dict(mmp="oss.FCL.sf.os.mmimaging/imagingextensions/imageadaptationextensions/iclextjpegapi/group/IclExtJpegApi.mmp"),   # <- ihl
    "jpegimageframeplugin": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/group/JPEGImageFramePlugin.mmp"),   # <- iclextjpegapi
    "jpegexifplugin": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/group/JPEGExifPlugin.mmp"),   # <- jpegimageframeplugin
    "imagetransform": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/ImageTransform/MmpFiles/ImageTransform.mmp"),   # <- jpegexifplugin
    "etel3rdparty": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyutils/etel3rdpartyapi/Group/Etel3rdParty.mmp"),   # <- thumbnailserver
    # the thumbnail server (CThumbnailManagerImpl connects at construction: Clock exited -1)
    "thumbnailserver": dict(mmp="oss.FCL.sf.mw.imghandling/imagehandlingutilities/thumbnailmanager/thumbnailserver/group/thumbnailserver.mmp", flags=OST_OFF,
                         drop=["thumbnailserver.cpp"], extra=["toolchain/shims/thumbnailserver_board/thumbnailserver.cpp"]),
    "filemanagerview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.files/filemanager/group/FileManagerView.mmp"),   # <- filemanager
    "filemanagerengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.files/filemanager/group/FileManagerEngine.mmp"),   # <- filemanagerview
    "gflm": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.files/filemanager/group/GFLM.mmp"),   # <- filemanagerengine
    "directorylocalizer": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uiutils/Localizer/group/DirectoryLocalizer.mmp"),   # <- gflm;
    "irobex": dict(mmp="oss.FCL.sf.mw.remoteconn/obex/obexprotocol/obex/group/irobex.mmp"),   # <- filemanagerengine;
    "irda": dict(mmp="oss.FCL.sf.os.bt/irda/irdastack/group/IRDA.MMP"),   # <- irobex
    "filemanagerbkupengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.files/filemanager/group/filemanagerbkupengine.mmp"),   # <- filemanagerengine
    "sbeclient": dict(mmp="oss.FCL.sf.mw.remoteconn/backupandrestore/backupengine/group/sbeclient.mmp"),   # <- filemanagerbkupengine
    "rsfwmountman": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotestorage/remotestoragefw/mountmanager/group/rsfwmountman.mmp"),   # <- filemanagerengine
    "rsfwmountstore": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotestorage/remotestoragefw/mountstore/group/rsfwmountstore.mmp"),   # <- rsfwmountman
    "rsfwmountutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotestorage/remotestoragefw/mountmanager/group/rsfwmountutils.mmp"),   # <- rsfwmountman
    "rsfwcontrol": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotestorage/remotestoragefw/remotefileengine/group/rsfwcontrol.mmp"),   # <- rsfwmountman
    "memstatepopup": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.files/filesystemuis/memstatepopup/group/MemStatePopup.mmp"),   # <- filemanagerview
    "memscanclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.files/filesystemuis/memscaneng/group/memscanclient.mmp"),   # <- memstatepopup
    "drmuihandling": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/commondrm/drmutility/group/DRMUiHandling.mmp"),   # <- filemanagerview
    "commondialogs": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/commonuis/CommonDialogs/group/CommonDialogs.mmp"),   # <- filemanager;
    "aknmemorycardui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/memorycardui/group/AknMemoryCardUi.mmp"),   # <- commondialogs;
    "phonebook2": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Phonebook2.mmp"),   # the root
    "logs": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/logsui/group/Logs.mmp"),   # the root
    "vpbkeng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/VirtualPhonebook/group/VPbkEng.mmp"),   # Phonebook2.mmp
    "vpbkengutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/VirtualPhonebook/group/VPbkEngUtils.mmp"),   # vpbkeng
    "pbk2uicontrols": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2UIControls.mmp",
                           flags=UNRELEASED_INC + ["-idirafter", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.app.commonemail/email_plat/email_client_api/inc")]),   # mailbox check: the Email client API (inline ECOM factory; no mail plug-in here)   # Phonebook2.mmp
    # OURS: null VoIP profile registry (never released; every lookup is empty) -- toolchain/shims/rcse_null
    "rcse": dict(mmp=os.path.join(OS_ROOT, "toolchain/shims/rcse_null/rcse_null.mmp"),
                 **{"def": os.path.join(OS_ROOT, "toolchain/shims/rcse_null/rcseu.def")}),   # pbk2uicontrols
    "aknjapanesereading": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uiutils/AknJapaneseReading/group/AknJapaneseReading.mmp"),   # pbk2uicontrols
    "drmparsers": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DrmParsers.mmp"),   # pbk2uicontrols
    "dcfrep": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/DcfRep.mmp"),   # drmparsers
    "pbk2presentation": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2Presentation.mmp"),   # pbk2uicontrols
    "psserverclientapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/predictivesearch/PcsServerClientAPI/group/PsServerClientAPI.mmp"),   # pbk2presentation
    "pcsutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/predictivesearch/PcsUtils/group/PcsUtils.mmp"),   # psserverclientapi
    "bsclient": dict(mmp="oss.FCL.sf.mw.imsrv/brandingserver/group/bsclient.mmp"),   # pbk2presentation
    "bsserver": dict(mmp="oss.FCL.sf.mw.imsrv/brandingserver/group/bsserver.mmp"),   # started by bsclient (CBSFactory::NewL <- contact presence)
    "pbk2uiext": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2UIExtensionManager.mmp"),   # pbk2uicontrols
    "pbk2commonui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2CommonUI.mmp", flags=UNRELEASED_INC),   # pbk2uiext
    "pbk2commands": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2Commands.mmp"),   # Phonebook2.mmp
    # Phonebook2 run-time set: the store plug-in, UI policy + extensions, the predictive-search server
    # Phonebook 2 KERN-EXEC 3 at every switch-off)
    "vpbkcntmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/VirtualPhonebook/group/VPbkCntModel.mmp",
                         drop=["cfilteredcontactview.cpp", "cfilteredgroupview.cpp"],
                         extra=["toolchain/shims/vpbkcntmodel_board/CFilteredContactView.cpp",
                                "toolchain/shims/vpbkcntmodel_board/CFilteredGroupView.cpp"]),
    "vpbkvcardeng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookengines/VirtualPhonebook/group/VPbkVCardEng.mmp"),
    "pbk2uipolicy": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2UIPolicy.mmp"),
    "pbk2exnameslistplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2NamesListExtension.mmp"),
    "pbk2cmdext": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2CommandsExtension.mmp"),
    "pbk2commonutility": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2CommonUtility.mmp"),
    "pbk2uiservices": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2UIServices.mmp"),
    "pbk2groupui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2GroupExtension.mmp"),
    "pbk2mmcui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2MMCExtension.mmp"),
    "pbk2migrationsupport": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2MigrationSupport.mmp"),
    "xspviewservices": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/xSPViewServices.mmp"),
    "pbk2serverapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/Pbk2ServerApp.mmp"),
    "pcsserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/predictivesearch/PcsServer/group/PcsServer.mmp"),
    "pcsalgorithm1": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/predictivesearch/PcsAlgorithm/Algorithm1/group/PcsAlgorithm1.mmp"),
    "pcsalgorithm2": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/predictivesearch/PcsAlgorithm/Algorithm2/group/PcsAlgorithm2.mmp"),
    "ccappcommlauncherplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/ccapplication/ccacommlauncherplugin/group/ccappcommlauncherplugin.mmp"),
    "ccappdetailsviewplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/ccapplication/ccadetailsviewplugin/group/ccappdetailsviewplugin.mmp"),   # the root
    "ccacontactorservice": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/ccapplication/ccacontactorservice/group/ccacontactorservice.mmp"),   # the root
    "ccappmycardplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/ccapplication/ccamycardplugin/group/ccappmycardplugin.mmp"),   # the root
    "ccaapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/ccapplication/ccapp/group/ccaapp.mmp"),   # the root
    "cmsserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/ccservices/cmsservices/cmsengine/Server/group/cmsserver.mmp", flags=UNRELEASED_INC),   # the root
    "speeddial": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Speeddial/group/SpeedDial.mmp"),   # the root
    "speeddialgsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Speeddial/speeddialgsplugin/group/speeddialgsplugin.mmp"),   # the root
    "cmsclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/ccservices/cmsservices/cmsengine/Client/group/cmsclient.mmp"),   # <- ccaapp.mmp
    "ccapputil": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/ccapplication/ccapp/ccapputil/group/ccapputil.mmp"),   # <- ccaapp.mmp
    "vcard": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/pimappsupport/vcardandvcal/group/VCard.MMP"),   # <- vpbkvcardeng; ALSO: oss.FCL.sf.app.organizer/pimappsupport/vcardandvcal/group/VCard.MMP
    "sortutil": dict(mmp="oss.FCL.sf.os.textandloc/textandlocutils/sortutil/group/SortUtil.mmp"),   # <- pbk2uipolicy
    "pbk2rclsetting": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/remotecontactlookup/settingplugin/group/pbk2rclsetting.mmp"),   # its .rsg <- pbk2rclengine
    "pbk2rclengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/remotecontactlookup/engine/group/pbk2rclengine.mmp", flags=UNRELEASED_INC),   # <- pbk2exnameslistplugin
    "pbkview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook/group/PbkView.mmp", flags=UNRELEASED_INC),   # <- pbk2rclengine
    # aknlayout: NO mmp in the checked-out packages (needed by pbkview)
    "pbkext": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook/group/PbkExt.mmp"),   # <- pbkview
    "bcardeng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook/group/BCardEng.mmp"),   # <- pbk2rclengine
    "pbk2spbcontentprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Phonebook2/group/pbk2spbcontentprovider.mmp"),   # <- pbk2exnameslistplugin
    "presencecacheclient2": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/presencecache/presencecacheclient2/group/presencecacheclient2.mmp"),   # <- pbk2spbcontentprovider; ALSO: oss.FCL.sf.app.contacts/presencecache/presencecachesymbian/presencecacheclient2/group/presencecacheclient2.mmp
    "presencecacheutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/presencecache/presencecacheutils/group/presencecacheutils.mmp"),   # <- presencecacheclient2; ALSO: oss.FCL.sf.app.contacts/presencecache/presencecachesymbian/presencecacheutils/group/presencecacheutils.mmp
    "contactpresence": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/ccservices/cmsservices/contactpresence/group/contactpresence.mmp"),   # <- pbk2exnameslistplugin
    "presencetrafficlights": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/ccservices/cmsservices/presencetrafficlights/group/presencetrafficlights.mmp"),   # <- contactpresence
    "bpas": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/ccservices/cmsservices/bpas/group/bpas.mmp"),   # <- presencetrafficlights
    # browserlauncher: NO mmp in the checked-out packages (needed by pbk2cmdext)
    # GCC delta (toolchain/shims/speeddial_x32): CreateIndexIconsL falls off the end of a TInt function;
    "spdctrl": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Speeddial/group/SpdCtrl.mmp",
                    flags=["-iquote", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Speeddial/VPbkControlSrc"),
                           # ControlInc/SpdiaIndexData.h quotes its .inl from ControlSrc (RVCT searched the source's dir)
                           "-iquote", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.contacts/phonebookui/Speeddial/ControlSrc")],
                    drop=["speeddialprivate.cpp"], extra=["toolchain/shims/speeddial_x32/speeddialprivate.cpp"]),   # <- pbk2cmdext
    "vmbx": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/engines/vmbxengine/group/vmbxengine.mmp",
                 flags=["-idirafter", os.path.join(OS_ROOT, "toolchain/shims/phonecnt_inc")]),   # <- spdctrl; the RCL_3 PhoneCntFinder API (forwarders)
    "phonecntfinder": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/PhoneCntFinder/Group/phonecntfinder.mmp",
                           # its SYSTEMINCLUDEs name the package's in-tree export dirs (never populated here)
                           flags=["-idirafter", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/phonemodel/inc"),
                                  "-idirafter", os.path.join(OS_ROOT, "toolchain/shims/phonecnt_inc")]),   # <- vmbx (new RCL_3 worktree app.phone)
    "rfs": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/Rfs/group/rfs.mmp"),   # <- phonecntfinder
    "logseng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/logsui/group/LogsEng.mmp"),   # Logs.mmp
    "aocctview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.contacts/logsui/group/AocCtView.mmp"),   # Logs.mmp
    "secui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/securitydialogs/SecUi/group/SecUi.mmp"),   # aocctview; ALSO: oss.FCL.sf.mw.securitysrv/securitydialogs/S
    "rlocksettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/remotelock/RemotelockSettings/Group/RemoteLockSettings.mmp"),   # secui; ALSO: oss.FCL.sf.mw.securitysrv/remotelock/Remotelock
    "scpclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/terminalsecurity/SCP/SCPClient/group/SCPClient.mmp"),   # secui
    # (its scptimestamppluginlang.rsg comes from the SCP timestamp plug-in's resources)
    "scptimestampplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/terminalsecurity/SCP/SCPTimestampPlugin/group/SCPTimestampPlugin.mmp"),
    "gsecomplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSFramework/GSEcomPlugin.mmp"),   # Logs.mmp
    # securesocket) + its token provider/cache. The 2010 TLS is TLS 1.0-era; Goal U's modern-TLS ssl.dll
    "securesocket": dict(mmp="oss.FCL.sf.os.networkingsrv/networksecurity/tls/group/SecureSocket.mmp"),
    "ssl_ph": dict(mmp=os.path.join(OS_ROOT, "shim/phorepoc/ssl_mbed.mmp"), rom="ssl.dll",
                   cdriver=True, allow_wsd=True,
                   **{"def": "oss.FCL.sf.os.networkingsrv/networksecurity/tls/eabi/sslU.def"},
                   defs=["MBEDTLS_CONFIG_FILE=<phorepoc_mbedtls_config.h>", "__SOFTFP=", "MBEDTLS_PLATFORM_MS_TIME_ALT"],
                   rom_data=["resource\\ssl\\ca-bundle.crt=thirdparty/x86/res/ca-bundle.crt"],
                   extra=sorted(os.path.relpath(p, OS_ROOT).replace(os.sep, "/") for p in __import__("glob").glob(
                       os.path.join(OS_ROOT, "shim/build/_deps/mbedtls-src/library/*.c")))),
    "libssl_ph": dict(mmp=os.path.join(OS_ROOT, "shim/phorepoc/libssl_mbed/libssl_mbed.mmp"), rom="libssl.dll",
                      cdriver=True, allow_wsd=True,
                      **{"def": "oss.FCL.sf.os.ossrv/ssl/libssl/eabi/libsslu.def"},
                      defs=["MBEDTLS_CONFIG_FILE=<phorepoc_mbedtls_config.h>", "__SOFTFP=", "MBEDTLS_PLATFORM_MS_TIME_ALT",
                            "PHOREPOC_LIBSSL_TRACE"],
                      extra=sorted(os.path.relpath(p, OS_ROOT).replace(os.sep, "/") for p in __import__("glob").glob(
                          os.path.join(OS_ROOT, "shim/build/_deps/mbedtls-src/library/*.c")))),
    "ssl": dict(mmp="oss.FCL.sf.os.networkingsrv/networksecurity/tls/group/SSL.MMP",
                **({} if os.environ.get("SSL_2010") else {
                    "x32_mmp": os.path.join(OS_ROOT, "shim/phorepoc/ssl_mbed.mmp"),
                    "def": "oss.FCL.sf.os.networkingsrv/networksecurity/tls/eabi/sslU.def",
                    "x32_archives": ["thirdparty/x86/lib/libmbedtls.a"],
                    "rom_data": ["resource\\ssl\\ca-bundle.crt=thirdparty/x86/res/ca-bundle.crt"]})),
    # Goal U: Open C's libssl.dll (OpenSSL 0.9.8g ABI: QtNetwork, javassl, Internet Radio) with TLS 1.2/1.3 from
    # `ssl` entry puts in the ROM (\resource\ssl\ca-bundle.crt). Clients name it `LIBRARY libssl_mbed.lib`.
    "libssl_mbed": dict(mmp=os.path.join(OS_ROOT, "shim/phorepoc/libssl_mbed/libssl_mbed.mmp"), rom="libssl.dll",
                        cdriver=True, allow_wsd=True,
                        **{"def": "oss.FCL.sf.os.ossrv/ssl/libssl/eabi/libsslu.def"},
                        defs=["__SOFTFP=", "PHOREPOC_LIBSSL_TRACE"],
                        x32_archives=["thirdparty/x86/lib/libmbedtls.a"]),
    # OURS: the OpenSSL 0.9.8 API smoke test over libssl_mbed (TCP + TLS to example.com:443, "[ssltest]" lines)
    "ssltest": dict(mmp=os.path.join(OS_ROOT, "src/ssltest/ssltest.mmp"), defs=["__SOFTFP="]),
    # tlsprovider.h includes tlsprovider_log.h, which lives beside the sources (abld put the source dir on the include
    "tlsprovider": dict(mmp="oss.FCL.sf.os.networkingsrv/networksecurity/tlsprovider/group/tlsprovider.mmp",
                        flags=["-I", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.networkingsrv/networksecurity/tlsprovider/source/tlsprovider")]),
    "tlscacheclient": dict(mmp="oss.FCL.sf.os.networkingsrv/networksecurity/tlsprovider/group/tlscacheclient.mmp"),
    "tlscacheserver": dict(mmp="oss.FCL.sf.os.networkingsrv/networksecurity/tlsprovider/group/tlscacheserver.mmp"),
    "tlsproviderconfig": dict(mmp="oss.FCL.sf.os.networkingsrv/networksecurity/tlsprovider/group/tlsproviderconfig.mmp", rsc_only=True),   # the TLS provider's resources (.rsg inputs)
    "swtlstokentypeplugin": dict(mmp="oss.FCL.sf.os.networkingsrv/networksecurity/tlsprovider/group/swtlstokentypeplugin.mmp"),   # the software TLS token (ECOM)
    "sysap": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/SysAp/Group/SysAp.mmp", defs=UNRELEASED_FEATURES,
                  # GCC delta (a case body declaring a variable without a scope): toolchain/shims/sysap_gcc
                  drop=["sysapdefaultkeyhandler.cpp", "SysApLightsController.cpp", "SysApAppUi.cpp"], extra=["toolchain/shims/sysap_gcc/sysapdefaultkeyhandler.cpp", "toolchain/shims/sysap_gcc/SysApLightsController.cpp", "toolchain/shims/sysap_gcc/SysApAppUi.cpp"],   # + the lights controller with TRACES on (backlight hunt)
                  flags=["-iquote", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/SysAp/Src")]),   # the root
    "accclient": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/AccClient.mmp"),   # <- SysAp.mmp
    "accpolicy": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/AccPolicy.mmp"),   # <- accclient
    # accmonitor.dll (UID3 0x0B1773D3): the accessory monitoring API (CAccMonitor); QuasarMX imports NewLC. The real component over accclient/accpolicy
    "accmonitor": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessorymonitor/group/accessorymonitoringapi.mmp", rtti=True),
    "accpolcom": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/AccPolCom.mmp", flags=["-include", os.path.join(EPOC32_ROOT, "include/publicruntimeids.hrh")]),   # <- accclient
    "accpolicydb": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryserver/group/AccPolicyDB.mmp"),   # <- accpolcom
    "usbman": dict(mmp="oss.FCL.sf.os.usb/usbmgmt/usbmgr/usbman/client/group/Usbman.mmp"),   # <- SysAp.mmp
    # + btui.iby of the RCL_3 tree
    "btengdevman": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/btengdevman.mmp"),   # <- SysAp.mmp
    "btmanclient": dict(mmp="oss.FCL.sf.os.bt/bluetoothmgmt/btmgr/BTManClient/BTManClient.mmp"),   # <- btengdevman
    "btengsettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/btengsettings.mmp"),   # <- SysAp.mmp
    "btengconnman": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/btengconnman.mmp"),   # <- SysAp.mmp
    "btengdiscovery": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/btengdiscovery.mmp"),
    "btfeatures": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/btfeatures/btfeatures.mmp"),
    "btengsrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/bteng.mmp"),
    "bttoggle": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/bttoggle.mmp"),
    "btbearer": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/btbearer.mmp"),
    "btrfsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/btrfsplugin.mmp"),
    "btpsmplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/bteng/group/btpsmplugin.mmp"),
    "dutmode": dict(mmp="oss.FCL.sf.os.bt/bthci/bthci2/dutmode/group/dutmode.mmp"),   # <- btengsrv
    "btnotif": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/btnotif/group/BTNotif.mmp"),
    # resources only: btnotif's enterprise-IT notifier includes dcmo.rsg (the Device Control MO server's strings)
    "dcmoserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/dcmofw/dcmoserver/group/dcmoserver.mmp", rsc_only=True),
    "btnotifwrapper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/btnotif/ecom/BtnotifWrapper.mmp"),
    "btdevmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/btui/devmodel/group/btdevmodel.mmp"),
    "btuiplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/btui/Ecom/group/BtuiPlugin.mmp"),   # the Settings > Connectivity > Bluetooth view
    "btui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.btservices/bluetoothengine/btui/group/BTUI.mmp"),
    "activitymanager": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/activitymanager/group/activitymanager.mmp"),   # <- SysAp.mmp
    "sanimctrl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/startupservices/startupanimation/sanimctrl/group/sanimctrl.mmp"),   # <- SysAp.mmp
    "sanimengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/startupservices/startupanimation/sanimengine/group/sanimengine.mmp"),   # <- sanimctrl
    "hwrmfmtxclient": dict(mmp="oss.FCL.sf.os.devicesrv/resourceinterfaces/fmtransmittercontrol/group/HWRMFmTxClient.mmp"),   # <- SysAp.mmp
    "telephonyaudiorouting": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/telephonyaudiorouting/TelephonyAudioRoutingBase/group/TelephonyAudioRouting.mmp"),   # <- SysAp.mmp
    "telephonyaudioroutingsession": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/telephonyaudiorouting/Session/Group/TelephonyAudioRoutingSession.mmp"),   # <- telephonyaudiorouting
    # telephony T1: the audio routing SERVER (+ its manager session / manager): the Phone engine's audio data needs it to
    # construct; phetelaudio (OURS) hosts it with an accept-all policy until the real route (T7)
    "telephonyaudioroutingserver": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/telephonyaudiorouting/Server/Group/TelephonyAudioRoutingServer.mmp"),
    "telephonyaudioroutingmanagersession": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/telephonyaudiorouting/Session/Group/TelephonyAudioRoutingManagerSession.mmp"),
    "telephonyaudioroutingmanager": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/telephonyaudiorouting/TelephonyAudioRoutingManager/group/TelephonyAudioRoutingManager.mmp"),
    "phetelaudio": dict(mmp=os.path.join(OS_ROOT, "src/phetelaudio/phetelaudio.mmp")),
    "bsutil": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/powersaveutilities/group/BSUtil.mmp"),   # <- SysAp.mmp
    "batterypopupcontrol": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/coreapplicationuis/powersaveutilities/group/batterypopupcontrol.mmp"),   # <- SysAp.mmp
    # .rsg inputs of the homescreen closure: the widget installer UI (widgetmanager), the GS network plug-in
    # (the native UI controller) -- resources only for now
    "widgetinstallerui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/widgets/widgetinstaller/group/WidgetInstallerUI.mmp", **_WRTXML),
    "gsnetworkplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSNetworkPlugin/GSNetworkPlugin.mmp", rsc_only=True),
    # the homescreen's LIW service providers (HSPS configuration API -- CHomescreenSettings needs it; action
    # handler, back-stepping)
    "actionhandlerplugins": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_actionhandler/actionhandlerplugins/group/ahplugins.mmp"),   # the root
    "actionhandlerservice": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_actionhandler/actionhandlerservice/group/actionhandlerservice.mmp"),   # the root
    "bsservice": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_backstepping/group/bsservice.mmp"),   # the root
    "hspsprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_homescreenplugin/group/hspsprovider.mmp"),   # the root
    "hspsservice": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_homescreenplugin/hspsservice/group/hspsservice.mmp"),   # the root
    # WebKit's WTF Platform.h tests __unix__ / linux BEFORE Symbian: the container GCC predefines them (x86_64
    # Linux target), so every WebKit TU took PLATFORM(UNIX) (collector.cpp: sys/mman.h). Undefine them for the
    # (RCL_3 mw.web: WebCore, JavaScriptCore, the browser control) + download manager, connection manager ...
    # rsc_incs: BrowserNG.rss includes <iaupdate.loc>, which no bld.inf in the release exports (a localisation
    # pack did): read it from the appinstall package
    "browserng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/browserui/browser/Group/BrowserNG.mmp", rsc_incs=["src/browserng-loc", "baseline/symbian-2010/oss.FCL.sf.mw.appinstall/iaupdate/IAD/loc"]),   # the root
    "browserlauncher": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.webuis/browserui/browser/Launcher2/group/BrowserLauncher.mmp"),   # the root
    # resolved by the S60 MCPR + MPM; plain Symbian ESOCK picked IAP 0 -> TierManagerUtils 4
    "mpmclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/bearermanagement/mpm/group/mpmclient.mmp"),   # the Mobility Policy Manager client
    "mpmserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/bearermanagement/mpm/group/mpmserver.mmp"),   # MPM: picks the IAP for a TExtendedConnPref/SNAP
    "mpmdefaultconnectionclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/bearermanagement/mpm/group/mpmdefaultconnectionclient.mmp"),   # MPM default-connection client
    "s60mcpr": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/bearermanagement/S60MCPR/group/s60mcpr.mmp"),   # the S60 network-tier MCPR (ECom): hands selection to MPM
    "extendedconnpref": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/bearermanagement/extendedconnpref/group/extendedconnpref.mmp"),   # <- connectionmanager (TExtendedConnPref)
    "extendedconnprefecom": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/bearermanagement/extendedconnpref/group/extendedconnprefecom.mmp"),   # its ECom factory: ESOCK decodes the pref by it
    "connectionmanager": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/connectionmanager/Group/ConnectionManager.mmp",   # <- BrowserNG.mmp
                              **(dict(drop=["ConnManActiveConnector.cpp"], extra=["toolchain/shims/connman_diag/ConnManActiveConnector.cpp"])
                                 if os.environ.get("CONNMAN_DIAG") == "1" else {})),
    "recenturlstore": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/recenturlstore/group/RecentUrlStore.mmp"),   # <- BrowserNG.mmp
    "cookiemanager": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/cookie/Group/CookieManager.mmp",
        drop=["CookieArray.cpp", "CookieClientData.cpp"],   # sliced ?: fix
        extra=["toolchain/shims/cookiemanager_board/CookieArray.cpp", "toolchain/shims/cookiemanager_board/CookieClientData.cpp"]),   # the HTTP cookie filter's manager (regsuggest had picked the httpexamples one: same TARGET)   # <- BrowserNG.mmp
    "browserdialogsprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/browserdialogsprovider/Group/browserdialogsprovider.mmp"),   # <- BrowserNG.mmp
    "feedsserverapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/feedsengine/FeedsServer/Api/group/FeedsServerApi.mmp"),   # <- BrowserNG.mmp
    "browserengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/browserengine/browserengine.mmp", flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/npapi_gcc/npapi_force.h")], x32_flags=WEBKIT_NOT_UNIX),   # <- BrowserNG.mmp
    # the S60 WebKit browserengine, shipped NEXT TO phebrctl under another name: WidgetUi needs its CreateWidgetExtensionL (the WRT
    "browserengine_s60": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/browserengine/browserengine.mmp", rom="browserengine_s60.dll", flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/npapi_gcc/npapi_force.h")], x32_flags=WEBKIT_NOT_UNIX),
    "webkitutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/webkitutils/group/WebKitUtils.mmp", cdriver=True,   # SqliteSymbian/*.c are C   # <- browserengine
                        # the sliced ?: in HistoryEntry.h (toolchain/shims/webkitutils_gcc: the header + its two users)
                        flags=["-iquote", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.web/webengine/webkitutils/HistoryProvider")],
                        drop=["HistoryController.cpp", "HistoryView.cpp"],
                        extra=["toolchain/shims/webkitutils_gcc/HistoryController.cpp", "toolchain/shims/webkitutils_gcc/HistoryView.cpp"]),
    "pagescaler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/pagescaler/group/minimap.mmp"),   # <- browserengine
    "javascriptcore": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/JavaScriptCore/JavaScriptCore.mmp", x32_flags=WEBKIT_NOT_UNIX,
                           # GCC delta: static definitions after non-static declarations (toolchain/shims/jsc_gcc)
                           drop=["snprintf.c"], extra=["toolchain/shims/jsc_gcc/snprintf.c"]),   # <- browserengine
    # by name, so the closure never found them; without them RHTTPSession::OpenL -> KEComErrNoInterfaceIdentified
    "httpmessage": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerprotocols/httptransportfw/group/httpmessage.mmp"),   # <- httpclient (HTTP message parser/composer)
    "httpclient": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerpluginsandutils/httpprotocolplugins/group/httpclient.mmp"),   # the HTTP/1.1 protocol handler (ECom)
    "httpclientcodec": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerpluginsandutils/httpprotocolplugins/group/httpclientcodec.mmp"),   # its header codec (ECom)
    "corefilters": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerpluginsandutils/httpprotocolplugins/group/corefilters.mmp"),   # redirect/auth/validation filters (ECom)
    "httptransporthandler": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerpluginsandutils/httptransportplugins/group/httptransporthandler.mmp"),   # the socket transport (ECom)
    "httpsecuresocket": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerpluginsandutils/httptransportplugins/group/httpsecuresocket.mmp"),   # TLS transport (ECom)
    "cookiefilter": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/cookie/Group/CookieFilter.mmp"),   # browser HTTP filter (ECom)
    "cookieserver": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/cookie/Group/CookieServer.mmp"),   # the cookie store server
    "deflatefilter": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/deflatefilter/group/DeflateFilter.mmp"),   # browser HTTP filter (ECom)
    "httpfilteracceptheader": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/httpfilteracceptheader/group/HttpFilterAcceptHeader.mmp"),   # browser HTTP filter (ECom)
    "httpfilterauthentication": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/httpfilterauthentication/group/HttpFilterAuthentication.mmp"),   # browser HTTP filter (ECom)
    "httpfilterconnhandler": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/httpfilterconnhandler/group/httpfilterConnHandler.mmp"),   # starts the browser connection (ECom)
    "httpfilteriop": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/httpfilteriop/group/httpfilterIop.mmp"),   # browser HTTP filter (ECom)
    "httpfilterproxy": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/httpfilterproxy/group/httpfilterproxy.mmp"),   # browser HTTP filter (ECom)
    "uaproffilter": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/uaproffilter/group/uaproffilter.mmp"),   # browser HTTP filter (ECom)
    "httpfiltercommon": dict(mmp="oss.FCL.sf.mw.netprotocols/httpfilters/httpfiltercommon/group/HttpFilterCommon.mmp"),   # <- browserengine
    "webutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/webutils/group/webutils.mmp"),   # <- browserengine
    "multipartparser": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/multipartparser/group/MultipartParser.mmp"),   # <- browserengine
    "downloadmgr": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/downloadmgr/DownloadMgrClntSrv/group/DownloadMgrClntSrv.mmp"),   # <- browserengine
    "downloadmgrserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/downloadmgr/DownloadMgrClntSrv/group/DownloadMgrServer.mmp"),   # <- downloadmgr (RHttpDownloadMgr starts it)
    "httpdmserveng": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/downloadmgr/DownloadMgrServEng/Group/HttpDMServEng.mmp"),   # <- downloadmgr
    "fotaengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/fotaapplication/fotaserver/FotaEngine/group/fotaengine.mmp"),   # <- httpdmserveng
    "coddownload": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/codhandler/coddownload/group/CodDownload.mmp"),   # <- downloadmgr
    "codui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/codhandler/codui/group/CodUi.mmp"),   # <- coddownload
    "codeng": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/codhandler/codeng/group/CodEng.mmp"),   # <- codui
    "roaphandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmengine/group/ROAPHandler.mmp"),   # <- codeng
    "drmroapwbxmlparser": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmroapwbxmlparser/group/drmroapwbxmlparser.mmp"),   # <- roaphandler
    "downloadmgruilib": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/downloadmgr/DownloadMgrUiLib/Group/DownloadMgrUiLib.mmp"),   # <- browserengine
    # GCC deltas (`::##x` token paste, member-fn pointers without &Class::): toolchain/shims/stmgesturefw_gcc
    "stmgesturefw": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/webkitutils/stmgesturefw/group/stmgesturefw.mmp",
                         flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.web/webengine/webkitutils/stmgesturefw/src")],
                         drop=["stateengine.cpp", "statemachine.cpp"],
                         extra=["toolchain/shims/stmgesturefw_gcc/stateengine.cpp",
                                "toolchain/shims/stmgesturefw_gcc/statemachine.cpp"]),   # <- browserengine (RCL_3 replaced rt_gesturehelper)
    "browsercache": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/cache/group/BrowserCache.mmp"),   # <- browserengine
    "webkit": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webkit/webkit.mmp", flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/npapi_gcc/npapi_force.h")], x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorederivedsources": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/DerivedSources/DerivedSources.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorebindings": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/bindings/bindings.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorecss": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/webcorecss/webcorecss.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcoredom": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/dom/dom.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcoreediting": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/editing/editing.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorehistory": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/history/history.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorehtml": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/html/html.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcoreloader": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/loader/loader.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorepage": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/page/page.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorerendering": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/rendering/rendering.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorexml": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/xml/xml.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcorebridge": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/bridge/bridge.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcoreplatform": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/webcore/platform/platform.mmp", flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/npapi_gcc/npapi_force.h"), "-iquote", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebCore/platform/symbian"), "-iquote", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebCore/platform/network/symbian")],
                            # deltas (toolchain/shims/webcore_gcc): a member-fn pointer without &Class::, a backslash include,
                            # HttpConnUtils: the sliced ?: (toolchain/shims/aifw_board)
                            drop=["FormFillController.cpp", "StaticObjectsContainer.cpp", "HttpConnUtils.cpp"],
                            extra=["toolchain/shims/webcore_gcc/FormFillController.cpp", "toolchain/shims/webcore_gcc/StaticObjectsContainer.cpp", "toolchain/shims/webcore_gcc/HttpConnUtils.cpp"], x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "webcore_xmlparser": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/libxml2/libxml2.mmp", x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    "plugins": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/WebKit/s60/group/plugins/plugins.mmp", flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/npapi_gcc/npapi_force.h")], x32_flags=WEBKIT_NOT_UNIX),   # <- browserengine
    # content control, widget manager (RCL_3 app.homescreen + mw.homescreensrv)
    "aicontainer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/exths/group/aicontainer.mmp"),   # the root
    "hscontentcontrol": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/hscontentcontrol/group/hscontentcontrol.mmp"),   # the root
    "ai3natiui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/nativeuicontroller/group/ainatiui.mmp"),   # the root
    "cpswrapper": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/sapiwrapper/cpswrapper/group/cpswrapper.mmp"),   # the root
    "hspswrapper": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/sapiwrapper/hspswrapper/group/hspswrapper.mmp"),   # the root
    "widgetmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/widgetmanager/group/widgetmanager.mmp"),   # the root
    "ai3xmluimain": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluicontroller/group/aixmluimain.mmp"),   # the root
    "xn3domdocument": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/dom/group/xndomdocument.mmp"),   # the root
    "xn3domvisitor": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/ecomelement/xndomvisitor/group/xndomvisitor.mmp"),   # the root
    "xn3ecomelementproxy": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/ecomelement/xnecomelementproxy/group/xnecomelementproxy.mmp"),   # the root
    "xn3odt": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/odt/group/xnodt.mmp"),   # the root
    "extrenderingplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/extrenderingplugin/group/extrenderingplugin.mmp"),   # the root
    "xn3animationfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnanimationfactory/group/xnanimationfactory.mmp"),   # the root
    "xn3bitmapfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnbitmapfactory/group/xnbitmapfactory.mmp"),   # the root
    "xn3clockfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnclockfactory/group/xnclockfactory.mmp"),   # the root
    "xn3menufactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnmenufactory/group/xnmenufactory.mmp"),   # the root
    "xn3newstickerfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnnewstickerfactory/group/xnnewstickerfactory.mmp"),   # the root
    # (xn3nppluginfactory: the Netscape-plug-in host needs the browser engine's npapi.h -- with the browser)
    "xn3popupfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnpopupfactory/group/xnpopupfactory.mmp"),   # the root
    "xn3texteditorfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xntexteditorfactory/group/xntexteditorfactory.mmp"),   # the root
    "xn3textfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xntextfactory/group/xntextfactory.mmp"),   # the root
    "xn3viewfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnviewfactory/group/xnviewfactory.mmp"),   # the root
    "xn3volumecontrolfactory": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/renderingplugins/xnvolumecontrolfactory/group/xnvolumecontrolfactory.mmp"),   # the root
    "xn3layoutengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/uiengine/group/xnlayoutengine.mmp"),   # the root
    "xn3resource": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/uiresource/group/xnresource.mmp"),   # the root
    "xn3utils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/idlehomescreen/xmluirendering/utils/group/xnutils.mmp"),   # the root
    "aifw": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/group/aifw.mmp",   # the root
                 drop=["aieventhandler.cpp"], extra=["toolchain/shims/aifw_board/aieventhandler.cpp"]),   # sliced ?: fix
    "aiidleint": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/group/aiidleint.mmp"),   # the root
    "aiutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/group/aiutils.mmp"),   # the root
    "homescreen": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/group/homescreen.mmp"),   # the root
    "ailaunch": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/hslaunch/group/hslaunch.mmp"),   # the root
    "aidevstaplg": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/devicestatus/group/aidevstaplg.mmp"),   # the root
    "mcsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/mcsplugin/group/mcsplugin.mmp"),   # the root
    "mcspluginhandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/mcsplugin/group/mcspluginhandler.mmp"),   # the root
    "mcspluginsettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/mcsplugin/group/mcspluginsettings.mmp"),   # the root
    "aiprofileplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/profileplugin/group/aiprofileplugin.mmp"),   # the root
    "sapidataplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/sapidataplugin/group/sapidataplugin.mmp"),   # the root
    "wrtdataplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/wrtdataplugin/group/wrtdataplugin.mmp"),   # the root
    "aiwsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/idlefw/plugins/wsplugin/group/aiwsplugin.mmp", flags=["-idirafter", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.phone/phoneuis/easydialing/inc")]),   # the root
    "hspluginsettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/homescreen_settings_api/group/hspluginsettings.mmp"),   # the root
    "hspsdefinitionengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsdefinitionengine/hspsdefinitionengine/group/hspsdefinitionengine.mmp"),   # the root
    "hspsdefinitionengineinterface": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsdefinitionengine/hspsdefinitionengineinterface/group/hspsdefinitionengineinterface.mmp"),   # the root
    "hspsdomdocument": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsdom/group/hspsdomdocument.mmp"),   # the root
    "hspsclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsmanager/group/hspsclient.mmp"),   # the root
    "hspsclientsession": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsmanager/group/hspsclientsession.mmp"),   # the root
    "hspsrequestclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsmanager/group/hspsrequestclient.mmp"),   # the root
    "hspsthemeserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsmanager/group/hspsthemeserver.mmp"),   # the root
    "hspsodt": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsodt/group/hspsodt.mmp"),   # the root
    "hspsdefrep": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspspluginregistry/group/hspsdefinitionrepository.mmp"),   # the root
    "hspsresource": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsresource/group/hspsresource.mmp"),   # the root
    "hspsresult": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspsresult/group/hspsresult.mmp"),   # the root
    "hspstools": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreenpluginsrv/hspstools/group/hspstools.mmp"),   # the root
    "ccclientsession": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentcontrolsrv/ccclientsession/group/ccclientsession.mmp"),   # the root
    "ccserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentcontrolsrv/ccsrv/group/ccsrv.mmp"),   # the root
    "ccsrvapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentcontrolsrv/ccsrvapi/group/ccsrvapi.mmp"),   # the root
    "hsccapiclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentcontrolsrv/hsccclient/hsccapiclient/group/hsccapiclient.mmp"),   # the root
    "hsccproviderclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentcontrolsrv/hsccclient/hsccproviderclient/group/hsccproviderclient.mmp"),   # the root
    "hscontentinfo": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentcontrolsrv/hscontentinfo/group/hscontentinfo.mmp"),   # the root
    "swiutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/swiutils.mmp"),   # <- hscontentcontrol.mmp
    "networkhandling": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/networkhandling/networkhandlingengine/Group/networkhandling.mmp"),   # <- ainatiui.mmp
    "cbsmcnclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/cbs/CbsServer/Group/CbsMcnClient.mmp"),   # <- networkhandling
    "phonesettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/cellular/telephonysettings/group/phonesettings.mmp"),   # <- ainatiui.mmp
    "swinstcli": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SWInstCli.mmp"),   # <- widgetmanager.mmp
    "gslauncher": dict(mmp="oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/gslauncher/group/GSLauncher.mmp"),   # <- mcsplugin.mmp
    "favouritesengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/favouritesengine/ClientServer/group/FavouritesEngine.mmp"),   # <- mcsplugin.mmp
    "favouritessrv": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/browserutilities/favouritesengine/ClientServer/group/FavouritesSrv.mmp"),   # <- favouritesengine (RFavouritesSession starts it)
    "taskswitcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/taskswitcherui/taskswitcherapp/group/tsapp.mmp"),   # the root
    "tsappecom": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/taskswitcherui/taskswitcherappecom/group/tsappecom.mmp"),   # the root
    "tsfswserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/contextengine/tsfswserver/server/group/tsfswserver.mmp"),   # the root
    "tspreviewprovidercrp": dict(rom="20016bec.dll", mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/contextengine/tsfswpreviewprovider/wsplugin/group/previewprovidercrp.mmp"),   # the root
    "tspreviewprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/contextengine/tsfswpreviewprovider/group/previewprovider.mmp"),   # the root
    "akntouchgesturefw": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/akntouchgesturefw/group/akntouchgesturefw.mmp"),   # <- tsapp.mmp
    "tscontextservicesutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/contextengine/tsctxutils/group/tscontextservicesutils.mmp"),   # <- tsapp.mmp
    "tsfswclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/contextengine/tsfswserver/client/group/tsfswclient.mmp"),   # <- tsapp.mmp
    "tsfswutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/contextengine/tsfswserver/utils/group/tsfswutils.mmp"),   # <- tsfswclient
    "tsfswengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.homescreen/taskswitcher/contextengine/tsfswserver/engine/group/tsfswengine.mmp"),   # <- tsfswserver.mmp
    # switcher registers with it at start (a null interface otherwise); the widget homescreen's data path
    "cpclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/homescreensrv_plat/sapi_contentpublishing/group/cpclient.mmp"),   # the root
    "cpserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentpublishingserver/cpserver/group/cpserver.mmp"),   # the root
    "cpstorage": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentpublishingserver/cpsqlitestorage/group/cpstorage.mmp"),   # the root
    "contentharvester": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentharvester/contentharvesterserver/group/contentharvesterserver.mmp"),   # the root
    "chfactorysettingsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentharvester/factorysettingsplugin/group/chfactorysettingsplugin.mmp"),   # the root
    "cputils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentpublishingutils/contentpublishingmap/group/cputils.mmp"),   # <- cpclient.mmp
    "pluginvalidator": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.homescreensrv/contentpublishingsrv/contentpublishingutils/pluginvalidator/group/pluginvalidator.mmp"),   # <- contentharvesterserver.mmp
    # `#ifdef linux` would take the mremap() path -- undefine it here (a global -U is its own tested step)
    "memman": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.web/webengine/osswebengine/MemoryManager/Group/MemMan.mmp",
                   x32_flags=["-Ulinux", "-U__linux", "-U__linux__"]),   # <- cpserver
    # (WebKit.rsg, a compile input of memman, comes from webcoreplatform's resource -- the browser block below)
    "psln": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/psln/Group/Psln.mmp"),   # the root
    "pslngsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/psln/PslnGSPlugin/group/PslnGSPlugin.mmp"),   # the root
    "pslnprofilesettingsloader": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/psln/PslnLibraryLoaders/group/pslnprofilesettingsloader.mmp"),   # the root
    "pslnwallpaperutilsloader": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/psln/PslnLibraryLoaders/group/pslnwallpaperutilsloader.mmp"),   # the root
    "pslnslidesetdialog": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/psln/pslnslidesetdialog/group/pslnslidesetdialog.mmp"),   # the root
    "pslnengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/psln/pslnengine/group/PslnEngine.mmp"),   # <- Psln.mmp
    "pslnframework": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/psln/PslnFramework/PslnFramework.mmp"),   # <- Psln.mmp
    "profileengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/profilesservices/ProfileEngine/group/ProfileEngine.mmp"),   # <- pslnprofilesettingsloader.mmp
    "gsapp": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSApplication/GSApplication.mmp"),   # the root
    "gslistbox": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSListBox/GSListBox.mmp"),   # the root
    "gsframework": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSFramework/GSFramework.mmp"),   # the root
    # Settings > Call: its SoftReject.rss is also the Phone app's soft-reject SMS text (phoneuiview includes SoftReject.rsg)
    "gscallplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSCallPlugin/GSCallPlugin.mmp"),
    "gsgenplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSGenPlugin/GSGenPlugin.mmp"),   # the root
    "gsdisplayplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSDisplayPlugin/GSDisplayPlugin.mmp",   # the power-save (PSM) plug-in left out: its API was never released
                            drop=["PSMDisplayPlugin.cpp", "GSDisplayPluginImplementationTable.cpp"],
                            extra=["toolchain/shims/gsdisplayplugin_gcc/GSDisplayPluginImplementationTable.cpp"]),
    "gsprslnplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSPrslnPlugin/GSPrslnPlugin.mmp"),   # the root
    "gssecurityplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSSecurityPlugin/GSSecurityPlugin.mmp"),   # the root
    "gstelplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSTelPlugin/GSTelPlugin.mmp"),   # the root
    "gsconplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSConPlugin/GSConPlugin.mmp"),   # the root
    "gsappsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSAppsPlugin/GSAppsPlugin.mmp"),   # the root
    "gsdevicemanagementplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSDeviceManagementPlugin/GSDeviceManagementPlugin.mmp"),   # the root
    "backgroundimage": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.gsprofilesrv/gssettingsuis/Gs/GSBackgroundImage/GSBackgroundImage.mmp"),   # the root
    "mediaclientimage": dict(mmp="oss.FCL.sf.os.mmimaging/imaging/imagingfws/MediaClientImage/MmpFiles/Client/MediaClientImage.mmp"),   # <- backgroundimage
    "about": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.utils/extras/about/group/about.mmp"),
    "sysversioninfo": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/PlatformEnv/sysversioninfo/group/sysversioninfo.mmp"),
    # OURS: minimal S60 Avkon application (app/document/appui/view + .rss)
    "aknapp": dict(mmp=os.path.join(OS_ROOT, "src/aknapp/aknapp.mmp"), rtti=True),
    # OURS: interim S60 launcher -- a real Avkon list of the AppList server's apps (src/aknmenu)
    "aknmenu": dict(mmp=os.path.join(OS_ROOT, "src/aknmenu/aknmenu.mmp"), rtti=True),
    # OURS: minimal CONE application (CCoeEnv/CCoeAppUi/CCoeControl)
    "coneapp": dict(mmp=os.path.join(OS_ROOT, "src/coneapp/coneapp.mmp")),
    "econs": dict(mmp="oss.FCL.sf.os.kernelhwsrv/kernel/eka/ewsrv/econs.mmp"),
    "ewsrv": dict(mmp="oss.FCL.sf.os.kernelhwsrv/kernel/eka/ewsrv/ewsrv.mmp"),
    # edisp.mmp's non-WINS arm is wd_generic.cpp alone; the x86 build of it compiled -D__X86__
    # and added epoc/vga/vgatext.cpp (CScreenDriver::New's HAL-mode-0 branch). Same here.
    "edisp": dict(mmp="oss.FCL.sf.os.kernelhwsrv/kernel/eka/drivers/edisp/edisp.mmp",
                  x32_flags=["-D__X86__"],
                  x32_extra=["baseline/symbian-2010/oss.FCL.sf.os.kernelhwsrv/kernel/eka/drivers/edisp/epoc/vga/vgatext.cpp"]),
    # the key map: Syborg's is indexed by EPOC standard key codes, which is what the PC keyboard
    # driver delivers too (it reuses the Syborg PC-set-1 table)
    "ekdata": dict(mmp="oss.FCL.sf.adapt.Qemu/baseport/syborg/keymap/keymap.mmp", rom="ekdata.dll",
                   **{"def": "oss.FCL.sf.os.kernelhwsrv/kernel/eka/eabi/ekdata.def"}),
    "eshell": dict(mmp="oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/fileserver/etshell/eshell.mmp"),
    "elocal": dict(mmp="oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/fileserver/sfat/elocal.mmp",
                   rom="elocal.fsy"),
    "efat32": dict(mmp="oss.FCL.sf.os.kernelhwsrv/userlibandfileserver/fileserver/sfat32/efat32.mmp",
                   rom="efat32.fsy"),
    # tcpip/ethernet ibys put in a ROM, server side. c32start reads the .cmi files, the root
    # server (c32exe) loads the CPMs (esocksvr, ...), ESOCK loads tcpip6.prt + the ethernet NIF.
    **{k: dict(mmp=f"{CFW}/{p}", **kw) for k, p, kw in [
        ("c32start", "commsprocess/commsrootserverconfig/configurator/group/c32start.mmp", {}),
        ("c32exe", "commsprocess/commsrootserverconfig/group/C32EXE.MMP", {}),
        ("c32root", "commsprocess/commsrootserverconfig/group/c32root.mmp", {}),
        ("procrootsrv", "commsfwsupport/commselements/rootserver/group/rootserver.mmp", {}),
        ("procroot", "commsfwsupport/commselements/rootserver/group/procroot.mmp", {}),
        ("commsfw", "commsfwsupport/commselements/commsfw/group/commsfw.mmp", {}),
        ("cflog", "commsfwsupport/commselements/commsfw/group/cflog.mmp", {}),
        ("factories", "commsfwsupport/commselements/factories/group/factories.mmp", {}),
        ("meshmachine", "commsfwsupport/commselements/meshmachine/group/meshmachine.mmp", {}),
        ("netmsg", "commsfwsupport/commselements/MsgParser/group/NETMSG.mmp", {}),
        ("netinterfaces", "commsfwsupport/commselements/NetInterfaces/group/NetInterfaces.mmp", {}),
        ("netmessages", "commsfwsupport/commselements/NetMessages/group/NetMessages.mmp", {}),
        ("netmeta", "commsfwsupport/commselements/NetMeta/group/NetMeta.mmp", {}),
        ("netsubscribe", "commsfwsupport/commselements/NetSubscribe/group/NetSubscribe.MMP", {}),
        ("nodemessages", "commsfwsupport/commselements/nodemessages/group/nodemessages.mmp", {}),
        ("responsemsg", "commsfwsupport/commselements/ResponseMsg/group/ResponseMsg.mmp", {}),
        ("serverden", "commsfwsupport/commselements/serverden/group/serverden.mmp", {}),
        ("netstsrv", "commsfwsupport/commselements/StartServer/group/NETStSrv.mmp", {}),
        ("netsm", "commsfwsupport/commselements/StateMachine/group/NETSM.mmp", {}),
        ("virtualconstructors", "commsfwsupport/commselements/virtualconstructors/group/virtualconstructors.mmp", {}),
        ("commsdat", "commsfwtools/preparedefaultcommsdatabase/group/CommsDat.MMP",   # delta: toolchain/shims/commsdat_gcc
         dict(drop=["CommsDatSessionImpl.cpp"], extra=["toolchain/shims/commsdat_gcc/CommsDatSessionImpl.cpp"],
              flags=["-I", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.commsfw/commsfwtools/preparedefaultcommsdatabase/src")])),
        ("ced", "commsfwtools/preparedefaultcommsdatabase/Tools/ced/group/ced.mmp", {}),
        ("ceddump", "commsfwtools/preparedefaultcommsdatabase/Tools/ceddump/group/ceddump.mmp", {}),
        ("commdb", "commsconfig/commsdatabaseshim/commdbshim/group/commdb.mmp", {}),
        ("commsbufs", "commsfwutils/commsbufs/group/commsbufs.mmp", {}),
        ("mbufmgr", "commsfwutils/commsbufs/group/mbufmgr.mmp", {}),
        ("mbufmgrimpl", "commsfwutils/commsbufs/group/mbufmgrimpl.mmp", {}),
        ("messageintercept", "commsfwtools/commstools/messageinterceptregister/group/messageintercept.mmp", {}),
        ("esocksvr", "datacommsserver/esockserver/group/ESockSvr.MMP", {}),
        ("esock_api_ext", "datacommsserver/esockserver/api_ext/group/api_ext.mmp", {}),
        ("commsdataobjects", "datacommsserver/esockserver/commsdataobjects/group/commsdataobjects.mmp", {}),
        ("commsdataobjectsecom", "datacommsserver/esockserver/commsdataobjects/group/commsdataobjectsecom.mmp", {}),
        ("connservparams", "datacommsserver/esockserver/connserv_params/group/connservparams.mmp", {}),
        ("subconparams", "datacommsserver/esockserver/subcon_params/group/subconparams.mmp", {}),
        ("esockmessages", "datacommsserver/esockserver/esock_messages/group/ESockMessages.mmp", {}),
        ("esockmessagesimpl", "datacommsserver/esockserver/esock_messages/group/ESockMessagesImpl.mmp", {}),
        ("esock_params", "datacommsserver/esockserver/esock_params/group/esock_params.mmp", {}),
        ("eintsock_transport", "datacommsserver/esockserver/eintsock_transport/group/eintsock_transport.mmp", {}),
        ("coreproviders", "datacommsserver/esockserver/CoreProviders/group/coreproviders.mmp", {}),
        ("coreprovidersecom", "datacommsserver/esockserver/CoreProviders/group/coreprovidersecom.mmp", {}),
        ("mobilitycoreproviders", "datacommsserver/esockserver/MobilityCoreProviders/group/mobilitycoreproviders.mmp", {}),
        ("simpleselectorbase", "datacommsserver/esockserver/simpleselectorbase/group/simpleselectorbase.mmp", {}),
        ("upscoreproviders", "datacommsserver/esockserver/UpsCoreProviders/group/upscoreproviders.mmp", {}),
        # the reference (ini-file) dialog server; an S60 ROM ships ipconnmgmt's AGENTDIALOG.DLL instead ("agentdialog")
        ("agentdialog_symbian", "datacommsserver/networkingdialogapi/default/agentdialog.mmp", {}),
    ]},
    **{k: dict(mmp=f"{NET}/{p}", **kw) for k, p, kw in [
        ("nifman", "linklayercontrol/networkinterfacemgr/group/NIFMAN.MMP", {}),
        ("agentpr", "linklayercontrol/networkinterfacemgr/group/agentpr.mmp", {}),
        ("agentprcore", "linklayercontrol/networkinterfacemgr/group/agentprcore.mmp", {}),
        ("netcfgext", "linklayercontrol/networkinterfacemgr/netcfgext/group/netcfgext.mmp", {}),
        # (NetCfgExtnBase.mmp is a dead leftover: its NetCfgExtnBase/src is gone, the code lives in netcfgext)
        ("genericscprparams", "linklayercontrol/genericscprparameters/group/genericscprparams.mmp", {}),
        ("nullagt", "linklayercontrol/nullagt/group/nullagt.mmp", dict(rom="nullagt.agt")),
        ("ethint", "linklayerprotocols/ethernetnif/group/ethproto.mmp", dict(rom="ethint.nif")),
        ("ether802", "linklayerprotocols/ethernetnif/group/ether802.mmp", {}),
        ("etherpkt", "linklayerprotocols/ethernetnif/group/EtherPkt.mmp", dict(rom="etherpkt.drv")),
        # iface.cpp: the GCCE 3.4 workaround hard-codes three class offsets under __GCCE__; GCC 14's layout differs and
        ("tcpip6", "networkprotocols/tcpipv4v6prt/group/tcpip6.mmp", dict(rom="tcpip6.prt", drop=["iface.cpp"],
                                                                        extra=["toolchain/shims/tcpip6_gcc/iface.cpp"])),
        ("inhook6", "networkprotocols/iphook/inhook6/group/inhook6.mmp", {}),
        ("ipeventtypes", "networkprotocols/ipeventnotifier/IPEventTypes/group/IPEventTypes.MMP", {}),
        # the DHCP unicast translator (ESOCK protocol "ipen", ip.ipeventnotifier.esk): its forward hook turns a DHCP
        # reply unicast to a not-yet-assigned address (dnsmasq routers) into a broadcast, else tcpip6 drops it (W4,
        ("ipeventfactory", "networkprotocols/ipeventnotifier/IPEventFactory/group/IPEventFactory.mmp", {}),
        ("ipeventnotifier", "networkprotocols/ipeventnotifier/group/ipeventnotifier.mmp", dict(rom="ipeventnotifier.prt")),
        ("ipproto", "networkcontrol/ipnetworklayer/group/ipproto.mmp", {}),
        ("addressinfo", "networkcontrol/ipnetworklayer/addressinfohook/group/addressinfohook.mmp", dict(rom="addressinfo.prt")),
        # (ipcpr/ipscpr = the legacy pre-iptransport providers: not in the real Syborg ROM,
        # stemkit/original/syborg.tmp1.oby, which is this board's ground truth for the comms file set)
        ("iptransport", "networkcontrol/iptransportlayer/group/iptransport.mmp", {}),
        ("iptransportimpl", "networkcontrol/iptransportlayer/group/iptransportimpl.mmp", dict(sf_layout=True)),
        ("iptransportmanagement", "networkcontrol/iptransportlayer/group/iptransportmanagement.mmp", {}),
        ("ipaddrinfoparams", "networkcontrol/iptransportlayer/ipaddrinfoparams/group/ipaddrinfoparams.mmp", {}),
        ("pfqoslib", "networkcontrol/pfqoslib/group/pfqoslib.mmp", {}),
        ("qoslib", "networkcontrol/qoslib/group/qoslib.mmp", {}),
        ("netups", "networkcontrol/commsuserpromptmgr/group/netups.mmp", {}),
        ("dnd", "tcpiputils/dnd/group/dndexe.mmp", {}),
        ("dhcpserv", "tcpiputils/dhcp/group/DHCPServ.mmp", {}),
        ("netcfgextndhcp", "tcpiputils/dhcp/group/NetCfgExtnDhcp.mmp", {}),
        ("dnsproxyclient", "networkprotocols/dnsproxy/dnsproxyclient/group/dnsproxyclient.mmp", {}),
        ("punycodeconverter", "tcpiputils/punycodeconv/group/punycodeconverter.mmp", {}),
    ]},
    # ... and the libraries it names from other packages
    "shimnotifiercli": dict(mmp=f"{CFW}/commsconfig/commsdatabaseshim/commdbshim/Notifier/group/ShimNotifierCli.mmp"),
    # error WITHOUT touching the status; ApEngine's CActiveApDbNotifier ignores the return, SetActive()s a status
    "shimnotifiersvr": dict(mmp=f"{CFW}/commsconfig/commsdatabaseshim/commdbshim/Notifier/group/ShimNotifierSvr.MMP"),
    "comsdbgutil": dict(mmp=f"{PDATA}/traceservices/commsdebugutility/group/commsdebugutility.mmp"),
    "logwrap": dict(mmp=f"{PDATA}/loggingservices/eventlogger/group/LOGWRAP.MMP"),
    "upsclient": dict(mmp="oss.FCL.sf.os.security/authorisation/userpromptservice/server/source/upsclient/upsclient.mmp"),
    "dial": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyutils/dial/group/DIAL.MMP"),
    "etelpckt": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserver/etelpacketdata/group/etelpckt.mmp"),
    "umtsif": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyprotocols/gprsumtsqosinterface/group/UmtsIf.mmp"),
    "qosextnapi": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyprotocols/qosextnapi/group/qosextnapi.mmp"),
    # TLS path the browser needs). SYMBIAN_CRYPTOSPI is on -> the weakcryptospi flavour, as the real
    # Syborg ROM ships it: cryptography.dll is weak_cryptography.dll (crypto.mmp, cryptography_eka2.def)
    # and it and cryptospi.dll import each other; softwarecrypto.dll is the SPI's software plug-in.
    **{k: dict(mmp=f"oss.FCL.sf.os.security/{p}", **kw) for k, p, kw in [
        ("cryptospi", "crypto/weakcryptospi/group/cryptospi.mmp", {}),
        # <= 56 bits, RSA <= 512: TCrypto::IsSymmetricWeakEnoughL leaves KErrKeyNotWeakEnough); real phones ship the
        # strong one. Weak broke WPA2 (eapol's AES key unwrap of the GTK: "Probably weak crypto library installed",
        ("cryptography", "crypto/weakcryptospi/group/crypto.mmp", dict(rom="cryptography.dll", defs=["CRYPTO_STRONG_BUILD"])),
        ("hash", "crypto/weakcryptospi/group/hash.mmp", {}),
        ("random", "crypto/weakcryptospi/group/random.mmp", {}),
        ("pbe", "crypto/weakcryptospi/group/pbe.mmp", {}),
        ("cryptospisetup", "crypto/weakcryptospi/group/cryptospisetup.mmp", {}),
        ("softwarecrypto", "cryptoplugins/cryptospiplugins/group/softwarecrypto.mmp", {}),
        ("asn1", "cryptoservices/certificateandkeymgmt/asn1/asn1.mmp", {}),
        ("crypto", "cryptoservices/certificateandkeymgmt/crypto/certman_crypto.mmp", {}),
        ("x500", "cryptoservices/certificateandkeymgmt/x500/X500.mmp", {}),
        ("x509", "cryptoservices/certificateandkeymgmt/x509/X509.mmp", {}),
        ("wtlscert", "cryptoservices/certificateandkeymgmt/wtlscert/wtlscert.mmp", {}),
        ("pkixcertbase", "cryptoservices/certificateandkeymgmt/pkixcertbase/pkixcertbase.mmp", {}),
        ("pkixcert", "cryptoservices/certificateandkeymgmt/pkixCert/PKIXCert.mmp", {}),
        ("certstore", "cryptoservices/certificateandkeymgmt/certstore/certstore.mmp", {}),
        ("asnpkcs", "cryptoservices/asnpkcs/group/asnpkcs.mmp", {}),
        ("ctframework", "cryptomgmtlibs/cryptotokenfw/group/CTFramework.mmp", {}),
        ("ctfinder", "cryptomgmtlibs/cryptotokenfw/group/CTFinder.mmp", {}),
        ("fstokencli", "cryptoservices/filebasedcertificateandkeystores/group/fstokencli.mmp", {}),
        ("fstokenshared", "cryptoservices/filebasedcertificateandkeystores/group/fstokenshared.mmp", {}),
        ("fstokenserver", "cryptoservices/filebasedcertificateandkeystores/group/fstokenserver.mmp", {}),
        ("filecertstoreplugin", "cryptoservices/filebasedcertificateandkeystores/group/filecertstoreplugin.mmp", {}),
        ("filekeystoreplugin", "cryptoservices/filebasedcertificateandkeystores/group/filekeystoreplugin.mmp", {}),
        ("sectcbutil", "cryptomgmtlibs/securityutils/group/sectcbutil.mmp", {}),
        # the file token server's resources (fstokenserver.rsg + its .rsc): data only
        ("securityconfig", "securityanddataprivacytools/securityconfig/group/securityconfig.mmp", dict(rsc_only=True)),
    ]},
    # also the browser's transport later)
    "http": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerprotocols/httptransportfw/group/http.mmp"),
    "etel": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserver/etelserverandcore/group/ETEL.MMP"),
    # bind to ESock_Main timed out (60 s) and held StartC32 -- so the UI server -- for a minute every boot
    "gsmu": dict(mmp="oss.FCL.sf.os.cellularsrv/smsprotocols/smsstack/gsmu/group/gsmu.mmp"),
    "smsu": dict(mmp="oss.FCL.sf.os.cellularsrv/smsprotocols/smsstack/smsu/group/smsu.mmp"),
    "smsprot": dict(mmp="oss.FCL.sf.os.cellularsrv/smsprotocols/smsstack/smsprot/Group/SMSPROT.mmp", rom="smsprot.prt"),   # T1 debugging: add incs_first=["toolchain/shims/ost_rdebug"] to print its OST traces to the serial log
    "wapprot": dict(mmp="oss.FCL.sf.os.cellularsrv/smsprotocols/smsstack/wapprot/Group/wapprot.mmp", rom="wapprot.prt"),
    # server, etel.cmi) + the S60 common TSY behind phonetsy.tsy (the module S60 loads by name) + the
    # licensee TSY STUB as licenseetsy.dll (a phone with no modem) + the SIM ATK stub as simatktsy.dll
    "phonetsy": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserverplugins/common_tsy/phonetsy/group/phonetsy.mmp", rom="phonetsy.tsy", flags=API_REF_LAST),
    "commontsy": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserverplugins/common_tsy/commontsy/group/commontsy.mmp", flags=API_REF_LAST),
    "licenseetsy": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserverplugins/licenseetsystub/licensee_tsy_stub/group/licenseetsy.mmp", rom="licenseetsy.dll",
                        drop=["cmmmockmesshandler.cpp"], extra=["toolchain/shims/licenseetsy_board/cmmmockmesshandler.cpp"], libs=["bafl.lib"],   # bafl: CDesC
                        # for registration/operator/signal; needs the MODEM=1 kernel; without it the fake modem is unchanged
                        defs=(["PHE_LTSY"] if os.environ.get("PHELTSY") == "1" else []),
                        flags=["-iquote", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.cellularsrv/telephonyserverplugins/licenseetsystub/licensee_tsy_stub/src"),
                               "-iquote", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.os.cellularsrv/telephonyserverplugins/licenseetsystub/licensee_tsy_stub/traces")],
                        **{"def": "oss.FCL.sf.os.cellularsrv/telephonyserverplugins/licenseetsystub/licensee_tsy_stub/eabi/licenseetsyu.def"}),
    # kernelhwsrv/kernel/eka/compsupp mmps, built with GCC. The BUILTIN entries of the same names stay the link-time
    # providers (their .def = the frozen ordinals these DLLs export)
    "scppnwdl": dict(mmp=KHS + "symcpp/scppnwdl.mmp",
                 own_newdel=True,
                 # the compilers' own pre-include (GCCE/RVCT preinclude symcpp.h: std::nothrow_t + the nothrow overloads)
                 flags=["-include", os.path.join(SYMBIAN, "symbian-2010", KHS, "symcpp/symcpp.h")], **{"def": KHS + "eabi/scppnwdlu.def"}),
    # drtaeabi: ARM's EHABI reference sources + the Symbian glue of the release, built with GCC (src/phorepoc/compsupp/
    # are libsupc++'s / libgcc's. eh_dll: the DLL itself uses C++ exceptions (try/catch in __cxa_end_catch)
    "drtaeabi": dict(mmp=os.path.join(OS_ROOT, "src/phorepoc/compsupp/drtaeabi.mmp"), eh_dll=True, rtti=True, own_newdel=True,
                     own_rtti_rt=True, own_crt=True, gcc_exports=True, allow_wsd=True,
                     supcxx_drop=["new_*.o", "del_*.o"],
                     flags=["-include", os.path.join(OS_ROOT, "src/phorepoc/compsupp/symaehabi/compsupp_pre.h")],
                     **{"def": KHS + "eabi/drtaeabiu.def"}),
    # dfpaeabi: the release's mmp is no_extras.cpp + RVCT's f_t_p.l; the soft-float helpers here are libgcc's
    "dfpaeabi": dict(mmp=KHS + "rvct/dfpaeabi.mmp", gcc_exports=True, rtti=False, **{"def": KHS + "eabi/dfpaeabiu.def"}),
    "simatktsy": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserverplugins/licenseetsystub/simatk_tsy_stub/group/simatktsy.mmp", rom="simatktsy.dll",
                      **{"def": "oss.FCL.sf.os.cellularsrv/telephonyserverplugins/licenseetsystub/simatk_tsy_stub/eabi/SIMATKTSYu.def"}),
    # sysutil's setup process: publishes the device-type attributes (+ version texts) as P&S the first
    # time a client asks (CDeviceTypeInformation launches it) -- CTSY's GetPhoneId reads them first
    "sysutilsetup": dict(mmp="oss.FCL.sf.os.devicesrv/commonservices/sysutil/group/sysutilsetup.mmp"),
    "hwrmpowerclient": dict(mmp="oss.FCL.sf.os.devicesrv/resourcemgmt/hwresourcesmgr/power/group/HWRMPowerClient.mmp"),
    # the common TSY's system-state plug-in (ECOM 0x102859CD): CTSY creates it at phone construction
    "ctsysystemstateplugin": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserverplugins/common_tsy/systemstateplugin/group/ctsysystemstateplugin.mmp", flags=API_REF_LAST),
    "etelmm": dict(mmp="oss.FCL.sf.os.cellularsrv/telephonyserver/etelmultimode/group/etelmm.mmp"),
    "logcli": dict(mmp=f"{PDATA}/loggingservices/eventlogger/group/LOGCLI.MMP"),
    # the log engine server (CLogClient connects at construction: Logs exited -1 without it) +
    # its contacts-matching plug-in
    "logserv": dict(mmp=f"{PDATA}/loggingservices/eventlogger/group/logserv.mmp"),
    "logcntmodel": dict(mmp=f"{PDATA}/loggingservices/eventlogger/group/logcntmodel.mmp"),
    "comsdbgsvr": dict(mmp=f"{PDATA}/traceservices/commsdebugutility/group/comsdbgsvrdll.mmp"),
    "ipcstream": dict(mmp="oss.FCL.sf.os.security/cryptomgmtlibs/securitycommonutils/group/ipcstream.mmp"),
    "scsclient": dict(mmp="oss.FCL.sf.os.security/cryptomgmtlibs/securitycommonutils/group/scsclient.mmp"),
    "calendar": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/application/group/Calendar.mmp"),   # the root
    "calencontroller": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/controller/group/calencontroller.mmp"),   # <- Calendar.mmp; ALSO: oss.FCL.sf.app.organizer/calendarui/controller/group/calencontroller.mmp
    "calencustomisationmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/customisationmanager/group/calencustomisationmanager.mmp"),   # <- calencontroller; ALSO: oss.FCL.sf.app.organizer/calendarui/customisationmanager/group/calencustomisationmanager.mmp
    "calenglobaldata": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/globaldata/group/CalenGlobalData.mmp"),   # <- calencustomisationmanager; ALSO: oss.FCL.sf.app.organizer/calendarui/globaldata/group/CalenGlobalData.mmp
    "caleninterimutils2": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarengines/caleninterimutils/group/CalenInterimUtils2.mmp"),   # <- calenglobaldata; ALSO: oss.FCL.sf.app.organizer/calendarengines/caleninterimutils/group/CalenInterimUtils2.mmp
    "calinterimapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/pimappservices/calendar/group/calinterimapi.mmp"),   # <- caleninterimutils2; ALSO: oss.FCL.sf.app.organizer/pimappservices/calendar/group/calinterimapi.mmp
    "agnmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/pimappservices/calendar/group/agnmodel.mmp"),   # <- calinterimapi; ALSO: oss.FCL.sf.app.organizer/pimappservices/calendar/group/agnmodel.mmp
    "libcrypto": dict(mmp="oss.FCL.sf.os.ossrv/ssl/libcrypto/group/libcrypto.mmp", cdriver=True, defs=["__SOFTFP="]),   # <- caleninterimutils2
    "libdl": dict(mmp="oss.FCL.sf.os.ossrv/genericopenlibs/openenvcore/libdl/group/libdl.mmp"),   # <- libcrypto
    "calenimp": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarengines/calenimp/group/CalenImp.mmp"),   # <- calenglobaldata; ALSO: oss.FCL.sf.app.organizer/calendarengines/calenimp/group/CalenImp.mmp
    "calencommonutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/commonutils/group/commonutils.mmp"),   # <- calenglobaldata; ALSO: oss.FCL.sf.app.organizer/calendarui/commonutils/group/commonutils.mmp
    "mnclientlib": dict(mmp="oss.FCL.sf.mw.locationsrv/locationmapnavfw/library/group/mnclientlib.mmp"),   # <- calencommonutils
    "eposlandmarks": dict(mmp="oss.FCL.sf.mw.locationsrv/landmarks/locationlandmarks/clientlib/group/EPos_LandmarksClientLibrary.mmp"),   # <- mnclientlib
    "sisregistryclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sisregistryclient.mmp"),   # <- mnclientlib
    "siscontroller": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/siscontroller.mmp"),   # <- sisregistryclient
    "securitymanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/securitymanager.mmp"),   # <- siscontroller
    "ocsp": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/ocsp/group/ocsp.mmp"),   # <- securitymanager; ALSO: oss.FCL.sf.mw.securitysrv/pkiutilities/ocsp/group/ocsp.mmp
    "ocspcommon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/ocsp/group/ocspcommon.mmp"),   # <- ocsp; ALSO: oss.FCL.sf.mw.securitysrv/pkiutilities/ocsp/group/ocspcommon.mmp
    "secsettingsclient": dict(mmp="oss.FCL.sf.os.security/cryptomgmtlibs/securityutils/group/secsettingsclient.mmp"),   # <- ocspcommon
    "ocspsupportclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/ocspsupportclient.mmp"),   # <- securitymanager
    "swicertstore": dict(mmp="oss.FCL.sf.os.security/cryptoservices/certificateandkeymgmt/swicertstore/swicertstore.mmp"),   # <- securitymanager
    "uissclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/uissclient.mmp"),   # <- securitymanager
    "devinfosupportclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/devinfosupportclient.mmp"),   # <- securitymanager
    "devinfosupportcommon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/devinfosupportcommon.mmp"),   # <- devinfosupportclient
    "plan": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/plan.mmp"),   # <- sisregistryclient
    "integrityservices": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/integrityservices.mmp"),   # <- sisregistryclient
    "swidataprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/swidataprovider.mmp"),   # <- sisregistryclient
    # Without the SIS registry server, RSisRegistrySession::Connect fails (-1) -> Calendar's Day view (map-provider
    # check in its container build, mnclientlib) left and the populator ran on a NULL container (KERN-EXEC 3).
    "sisregistryserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sisregistryserver_legacy.mmp"),   # bld.inf's non-USIF server
    "sisregistryhelperserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sisregistryhelperserver.mmp"),
    "installserver": dict(mmp=os.path.join(OS_ROOT, "src/phorepoc/swi/oss.FCL.sf.mw.appinstall-group/installserver.mmp")),
    "sislauncherserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sislauncherserver.mmp"),
    "sislauncherclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sislauncherclient.mmp"),
    "sishelper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sishelper.mmp"),
    "uiss": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/uiss.mmp"),
    "sisxfile": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sisfile.mmp"),
    "sisresult": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sisresult.mmp"),
    "cafdataprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/cafdataprovider.mmp"),
    "ocspsupport": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/ocspsupport.mmp"),
    "ocsptransport": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/ocsp/transport/ocsptransport.mmp"),
    "secutil": dict(mmp="oss.FCL.sf.os.security/cryptomgmtlibs/securityutils/group/secutil.mmp"),
    "httputils": dict(mmp="oss.FCL.sf.mw.netprotocols/applayerprotocols/httptransportfw/group/httputils.mmp"),
    "devinfosupport": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/devinfosupport.mmp"),
    "backuprestore": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/backuprestore.mmp"),
    "pkgremover": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/pkgremover.mmp"),
    "recsisx": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/sisxrecognizer.mmp"),
    "swisidchecker": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/group/swisidchecker.mmp"),
    "swiobserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/source/swiobserver/group/swiobserver.mmp"),
    "swiobserverclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/source/swiobserver/group/swiobserverclient.mmp"),
    "swiobservedinfo": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/source/swiobserver/group/swiobservedinfo.mmp"),
    "swiobserverplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/source/swiobserver/group/swiobserverplugin.mmp"),
    "upsobserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/installationservices/swi/source/upsswiobsplugin/group/upsobserver.mmp"),
    # authorise every send (service 0x2000BB6C); without it CSmsOutboxSend::StartL leaves and a message from the editor is never sent.
    "upsserver": dict(mmp="oss.FCL.sf.os.security/authorisation/userpromptservice/server/source/upsserver/upsserver.mmp",
                      # OURS: the SMS service's policy (the product's is licensee data): silent accept -- tools/phorepoc-x86/board/ups/
                      rom_data=[r"private\10283558\policies\ups_1000484b_2000bb6c.rsc=tools/phorepoc-x86/board/ups/ups_1000484b_2000bb6c.rsc"]),
    "upsdb": dict(mmp="oss.FCL.sf.os.security/authorisation/userpromptservice/database/group/upsdb.mmp"),
    "upspolicies": dict(mmp="oss.FCL.sf.os.security/authorisation/userpromptservice/policies/group/upspolicies.mmp"),
    "upsmanagement": dict(mmp="oss.FCL.sf.os.security/authorisation/userpromptservice/server/source/upsclient/upsmanagement.mmp"),
    "scsserver": dict(mmp="oss.FCL.sf.os.security/cryptomgmtlibs/securitycommonutils/group/scsserver.mmp"),
    "swidaemon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/daemon.mmp"),
    "swinstsvrui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SWInstSvrUI.mmp"),
    "swinstcommonui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SWInstCommonUI.mmp"),
    "swinsttaskmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SWInstTaskManager.mmp"),
    "swinstfiletask": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SWInstFileTask.mmp"),
    "swinstlogtask": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SWInstLogTask.mmp"),
    "swinststartuptask": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SWInstStartupTask.mmp"),
    "sisxui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppinstUi/group/SisxUI.mmp"),
    "x509certnameparser": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/x509certnameparser/group/X509CertNameParser.mmp"),
    # failure" query the TLS drop-in raises for a certificate that fails verification -- and the device token
    # server behind their trusted-sites store ("accept permanently")
    "certmanuiplugin": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/CertmanUi/GROUP/CertmanuiPlugin.mmp"),   # certmanui.rsg <- ctsecdialogimpl
    "ctsecdialogimpl": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/CTSecurityDialogs/Group/CTSecDialogImpl.mmp",
                            allow_wsd=True),   # 12 B: a block-scope _LIT GCC puts in .data (the DLL lives in eiksrv only)
    "ctsecdlgnotifier": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/CTSecurityDialogs/Group/CTSecDlgNotifier.mmp"),
    "ctsecdlgs": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/CTSecurityDialogs/Group/CTSecDlgs.mmp"),
    "devtokenshared": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/DeviceToken/Group/DevTokenShared.mmp"),
    "devtokenclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/DeviceToken/Group/DevTokenClient.mmp"),
    "devtokenserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/DeviceToken/Group/DevTokenServer.mmp"),
    "devtokenkeystoreplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/DeviceToken/Group/DevTokenKeyStorePlugin.mmp"),
    "devtokendevcertstoreplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/DeviceToken/Group/DevTokenDevCertStorePlugin.mmp"),
    "devtokentrusrvcertstoreplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.securitysrv/pkiutilities/DeviceToken/Group/DevTokenTruSrvCertStorePlugin.mmp"),
    "drmlicensemanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.drm/omadrm/drmlicensemanager/group/DRMLicenseManager.mmp"),
    # the Application manager (Settings > Installations) + its runtime plug-ins
    "appmngr2": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppMngr2/group/appmngr2.mmp"),
    "appmngr2pluginapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstall_plat/appmngr2runtimeapi/group/appmngr2pluginapi.mmp"),
    "appmngr2gsinstalledplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppMngr2/group/appmngr2gsinstalledplugin.mmp"),
    "appmngr2gsinstfilesplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppMngr2/group/appmngr2gsinstfilesplugin.mmp"),
    "appmngr2gssettingsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppMngr2/group/appmngr2gssettingsplugin.mmp"),
    "swinstadapter": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppMngr2/group/swinstadapter.mmp"),
    "appmngr2sisxplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppMngr2/Sisx/group/appmngr2sisxruntime.mmp"),
    "appmngr2widgetplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appinstall/appinstaller/AppMngr2/Widget/group/appmngr2widgetruntime.mmp"),
    "missedalarmstore": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/alarmengines/missedalarmstore/group/missedalarmstore.mmp"),   # <- calenglobaldata; ALSO: oss.FCL.sf.app.organizer/alarmengines/missedalarmstore/group/missedalarmstore.mmp
    "calensettingsui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/settings/settingsui/group/calensettingsui.mmp"),   # <- calencustomisationmanager; ALSO: oss.FCL.sf.app.organizer/calendarui/settings/settingsui/group/calensettingsui.mmp
    "calenviews": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/views/group/calenviews.mmp"),   # <- calencontroller; ALSO: oss.FCL.sf.app.organizer/calendarui/views/group/calenviews.mmp
    "multicaluidialog": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/multicaluidialog/group/multicaluidialog.mmp"),   # <- calenviews; ALSO: oss.FCL.sf.app.organizer/calendarui/multicaluidialog/group/multicaluidialog.mmp
    # GCC deltas (member-function pointers without &Class::): toolchain/shims/gesturehelper_gcc
    "gesturehelper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.mmuifw/mulwidgets/gesturehelper/group/gesturehelper.mmp",
                          flags=["-I", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.mw.mmuifw/mulwidgets/gesturehelper/src")],
                          drop=["gesturehelperimpl.cpp", "gesturerecogniser.cpp"],
                          extra=["toolchain/shims/gesturehelper_gcc/gesturehelperimpl.cpp",
                                 "toolchain/shims/gesturehelper_gcc/gesturerecogniser.cpp"]),   # <- calenviews
    "cfclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfclient.mmp"),   # <- calencontroller; ALSO: oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfclient.mmp
    "cfservices": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfservices.mmp"),
    # Event.Orientation from the orientation channel; orientationactionplugin sends akncapserver the hardware-state key
    "cfserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfserver.mmp"),
    "cfscriptengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfscriptengine.mmp"),
    "cfcontextsourcemanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfcontextsourcemanager.mmp"),
    "cfcontextsourcesettingsmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfcontextsourcesettingsmanager.mmp"),
    "cfactivatorengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfactivatorengine.mmp"),
    "cfoperationpluginservices": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfoperationpluginservices.mmp"),
    "basicoperationsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfw/group/basicoperationsplugin.mmp"),
    "appstatesourceplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfwplugins/ApplicationStateSourcePlugIn/group/ApplicationStateSourcePlugIn.mmp"),
    "psstatesourceplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfwplugins/PSStateSourcePlugIn/group/psstatesourceplugin.mmp"),
    "cenrepsourceplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfwplugins/cenrepsourceplugin/group/cenrepsourceplugin.mmp"),
    "displaysourceplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfwplugins/displaysourceplugin/group/displaysourceplugin.mmp"),
    "sensorsourceplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfwplugins/sensorsourceplugin/group/sensorsourceplugin.mmp"),
    "orientationactionplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.appsupport/contextframework/cfwplugins/orientationactionplugin/group/orientationactionplugin.mmp"),   # <- cfclient; ALSO: oss.FCL.sf.mw.appsupport/contextframework/cfw/group/cfservices.mmp
    # its SYSTEMINCLUDE ../../../inc is a package-local export dir (clientlib's bld.inf copies EPos_LandmarksErrors.h
    # there); read it from clientlib/inc instead
    "eposlmdbmanlib": dict(mmp="oss.FCL.sf.mw.locationsrv/landmarks/locationlandmarks/dbmanclientlib/group/EPos_LmDbManClientLibrary.mmp",
                           flags=["-idirafter", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.mw.locationsrv/landmarks/locationlandmarks/clientlib/inc")],
                           # GCC delta (_FOFF with a run-time array index): toolchain/shims/eposlmdbman_gcc
                           drop=["epos_hposlmdatabaseinfo.cpp"],
                           extra=["toolchain/shims/eposlmdbman_gcc/EPos_HPosLmDatabaseInfo.cpp"]),   # <- calencontroller
    # Calendar's run-time set: the agenda server (calinterimapi connects to it), the default editors (ECOM),
    # the calendar server (alarms / missed alarms); their .rsg headers are also compile inputs of the closure
    "agsvexe": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/pimappservices/calendar/group/agsvexe.mmp"),
    "calendefaulteditors": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/editors/group/CalenDefaultEditors.mmp"),
    "calensvr": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/server/CalenSvr/group/CalenSvr.mmp"),
    "calensvrclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.organizer/calendarui/server/CalenSvrClient/group/CalenSvrClient.mmp"),
    # the root. PushMtmCommands.hrh: included, unused (unreleased_inc). mceui.cpp uses KMtmUiFunctionMessageInfo
    # / ...DeliveryStatus with no released header on its include path defining them: they are the platform
    # API's MtmExtendedCapabilities.hrh (messaging_center_ui_utilities_api), force-included
    # -fno-devirtualize: `new CAknNoteDialog(..)->ExecuteLD(r)` is a VIRTUAL call (RVCT: through the vtable);
    # GCC sees the exact type and calls CEikDialog::ExecuteLD directly -- in eikdlg.dll, which mce.mmp never
    # lists, so it became a link stub
    "mce": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/mce/group/mce.mmp",
                flags=UNRELEASED_INC + ["-include", os.path.join(EPOC32_ROOT, "include/platform/app/mtmextendedcapabilities.hrh")],
                x32_flags=["-fno-devirtualize"]),
    # MTM, the e-mail MTM UI (imum.rsg), MCE's extra main-view items (a resource-only lib)
    "smss": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/smsmtm/servermtm/group/smss.mmp"),
    "imum": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/email/imum/Mtms/Group/imum.mmp"),
    "mceextraitems": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/mceextraitems/group/mceextraitems.mmp", rsc_only=True),
    # deltas in toolchain/shims/msgs_gcc: nested structs declared public, defined private (GCC rejects it;
    # force-included so its guard wins wherever the header is #included from), and an undeclared
    # `cacheEntry` in the removable-media GetEntryL (for aEntry)
    "msgs": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/msgsrvnstore/server/group/MSGS.mmp",
                 flags=["-include", os.path.join(OS_ROOT, "toolchain/shims/msgs_gcc/msvmessagedbadapter.h")],
                 drop=["msvdbadapter.cpp"], extra=["toolchain/shims/msgs_gcc/msvdbadapter.cpp"]),   # <- mce.mmp; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/msgsrvnstore/server/group/MSGS.mmp
    "mtur": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/msgsrvnstore/mtmbase/group/MTUR.mmp"),   # <- mce.mmp; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/msgsrvnstore/mtmbase/group/MTUR.mmp
    "muiu": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgavkon/muiu_domain/group/muiu_domain.mmp"),   # <- mce.mmp; ALSO: oss.FCL.sf.app.messaging/messagingappbase/msgavkon/muiu_domain/group/muiu_domain.mmp
    "muiu_internal": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgavkon/muiu_internal/group/muiu_internal.mmp"),   # <- muiu; ALSO: oss.FCL.sf.app.messaging/messagingappbase/msgavkon/muiu_internal/group/muiu_internal.mmp
    "imcm": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/email/pop3andsmtpmtm/clientmtms/group/IMCM.mmp"),   # <- muiu_internal; ALSO: oss.FCL.sf.app.messaging/email/pop3andsmtpmtm/clientmtms/group/IMCM.mmp
    "msgcommonutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/msgcommonutils/group/msgcommonutils.mmp"),   # <- muiu_internal; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/msgcommonutils/group/msgcommonutils.mmp
    "mcelogeng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/mce/group/MceLogEng.mmp"),   # <- mce.mmp; ALSO: oss.FCL.sf.app.messaging/messagingappbase/mce/group/MceLogEng.mmp
    "mmscodecclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mmsengine/mmscodecclient/bld/mmscodecclient.mmp"),   # <- mce.mmp; ALSO: oss.FCL.sf.app.messaging/mmsengine/mmscodecclient/bld/mmscodecclient.mmp
    "mmscodec": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mmsengine/mmscodec/bld/mmscodec.mmp"),   # <- mmscodecclient; ALSO: oss.FCL.sf.app.messaging/mmsengine/mmscodec/bld/mmscodec.mmp
    "mmsmessage": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mmsengine/mmsmessage/bld/mmsmessage.mmp"),   # <- mmscodec; ALSO: oss.FCL.sf.app.messaging/mmsengine/mmsmessage/bld/mmsmessage.mmp
    "mmsgenutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mmsengine/genutils/bld/mmsgenutils.mmp"),   # <- mmsmessage; ALSO: oss.FCL.sf.app.messaging/mmsengine/genutils/bld/mmsgenutils.mmp
    "mmsserversettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mmsengine/mmsserversettings/bld/mmsserversettings.mmp"),   # <- mmsmessage; ALSO: oss.FCL.sf.app.messaging/mmsengine/mmsserversettings/bld/mmsserversettings.mmp
    # sending server MTM (smss for SMS). Without it a message sent from the editor sits in the Outbox forever.
    "schsendexe": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/scheduledsendmtm/schedulesendexe/group/SchSendExe.mmp"),
    "schsend": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/scheduledsendmtm/schedulesendmtm/group/schsend.mmp"),   # <- mmsserversettings; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/scheduledsendmtm/schedulesendmtm/group/schsend.mmp
    "mmsconninit": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mmsengine/mmsconninit/bld/mmsconninit.mmp"),   # <- mmsserversettings; ALSO: oss.FCL.sf.app.messaging/mmsengine/mmsconninit/bld/mmsconninit.mmp
    "smcm": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/smsmtm/clientmtm/group/smcm.mmp",
                 # SMUTHDR pops the header fields before CommitL (ownership passed to the store)
                 drop=["SMSCLNT.CPP", "SMUTHDR.CPP"],
                 extra=["toolchain/shims/smcm_fix/SMSCLNT.CPP", "toolchain/shims/smcm_fix/SMUTHDR.CPP"]),   # <- mce.mmp; ALSO: oss.FCL.sf.app.messaging/mobilemessaging/smsmtm/clientmtm/group/smcm.mmp
    "biodb": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BDB/group/BIODB.MMP"),   # <- smcm; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BDB/group/BIODB.MMP
    "bifu": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BIFU/group/BIFU.MMP"),   # <- biodb; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BIFU/group/BIFU.MMP
    # watcher / SMS MTM look up (TSmsUtilities::ServiceIdL). NOT enough yet for the Inbox: BioWatchers/{BioWatcher,NbsWatcher}.mmp (the SMS
    # receive watcher plug-ins) build but are not registered -- see telephony-plan.md, T1 results ("Messaging")
    "mailinit": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgavkon/Initialise/group/mailinit.mmp"),
    # its base. smsgetdetdescdefault = the ECom plug-in the watcher asks for a message's details / description text.
    "biowatcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BioWatchers/BioWatcher.mmp"),
    "nbswatcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BioWatchers/NbsWatcher.mmp"),
    "wapwatcher": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BioWatchers/WapWatcher.mmp"),
    "class0sms": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/biomsgfw/BioWatchers/class0sms.mmp"),
    "smsgetdetdescdefault": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/smsmtm/smsgetdetdescdefault/group/smsgetdetdescdefault.mmp"),
    "mcesettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/mcesettings/group/MceSettings.mmp"),   # <- mce.mmp; ALSO: oss.FCL.sf.app.messaging/messagingappbase/mcesettings/group/MceSettings.mmp
    "imumutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/email/imum/Utils/Group/ImumUtils.mmp"),   # <- mcesettings; ALSO: oss.FCL.sf.app.messaging/email/imum/Utils/Group/ImumUtils.mmp
    "cmmanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/Framework/Group/cmmanager.mmp"),   # <- imumutils; ALSO: oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/cmmapi/group/cmmapi.mmp
    "cmpluginlan": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/Plugins/cmpluginlan/group/cmpluginlan.mmp"),   # LAN/Ethernet bearer: makes our IAPs visible to cmmanager + MPM
    "cmpluginpacketdata": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/Plugins/cmpluginpacketdata/group/cmpluginpacketdata.mmp"),   # packet-data bearer
    "cmpluginembdestination": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/Plugins/cmpluginembdestination/group/cmpluginembdestination.mmp"),   # embedded destinations (SNAP in SNAP)
    "cmmanagerdatabase": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/database/group/cmmanagerdatabase.mmp"),   # <- cmmanager; ALSO: oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/database/group/cmmanagerdatabase.mmp
    "connectionuiutilities": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/ConnectionUiUtilities.mmp"),   # <- cmmanager; ALSO: oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/ConnectionUiUtilities.mmp
    "apsettingshandlerui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/apengine/apsettingshandlerui/group/Apsettingshandlerui.mmp"),   # <- imumutils
    "apengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/apengine/apeng/group/APEngine.MMP"),   # <- apsettingshandlerui
    "vpnapi": dict(mmp="oss.FCL.sf.mw.vpnclient/vpnapiimpl/group/vpnapi.mmp"),   # <- apengine
    # the WLAN security settings UIs (RCL_3 mw.accesssec): no WLAN on this board, so Symbian's own stubs
    # under the real names -- the package's .iby does the same when __PROTOCOL_WLAN is off
    "wepsecuritysettingsui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.accesssec/wlansecuritysettings/wepsecuritysettingsui/group/WEPSecuritySettingsUI.mmp"),   # <- apengine
    "wpasecuritysettingsui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.accesssec/wlansecuritysettings/wpasecuritysettingsui/group/WPASecuritySettingsUI.mmp"),   # <- apengine
    "wapisecuritysettingsui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.accesssec/wlansecuritysettings/wapisecuritysettingsui/stub/group/wapisecuritysettingsstub.mmp"),
    "wifiprotplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.accesssec/wlansecuritysettings/wifiprotectedsetup/group/wifiprotpluginstub.mmp"),
    "cmpluginwlan": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/cmmgr/Plugins/cmpluginwlan/group/cmpluginwlan.mmp"),
    "wsfwlaninfo": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/wlaninfo/group/wlaninfo.mmp"),
    # the blacklist DB is SECURE[2000cf30]: without its DBMS policy RDbNamedDatabase::Create leaves KErrArgument and
    "wsfwlaninfosorting": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/wlaninfosorting/group/wlaninfosorting.mmp",
                               rom_data=["private\\100012a5\\policy\\2000cf30.spd=baseline/symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/wlaninfosorting/data/2000cf30.spd"]),
    "wsfclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/engine/client/group/client.mmp"),
    "wsfapwizard": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/apwizard/group/apwizard.mmp"),
    "wsfmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/model/group/model.mmp"),
    "wsfserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/engine/server/group/server.mmp"),
    "wsfaihelper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/aihelper/group/aihelper.mmp"),
    "wsfaiplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/aiplugin/group/aiplugin.mmp"),
    "wlansniffer": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansniffer/mainapplication/group/mainapplication.mmp"),
    "wlansettingsui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlansettingsui/group/wlansettingsui.mmp"),
    "wlanplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlanplugin/group/wlanplugin.mmp"),
    "wlanindicatorplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.wirelessacc/wlanutilities/wlanindicatorplugin/group/wlanindicatorplugin.mmp"),
    "gsconnsettingsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/cmmanager/gsconnsettingsplugin/group/gsconnsettingsplugin.mmp"),
    "senduidatautils": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/senduiservices/datautils/group/SenduiDataUtils.mmp"),   # <- sendui
    "akninputlanguage": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.inputmethods/textinput/AknInputLanguage/group/akninputlanguage.mmp"),   # <- apsettingshandlerui; ALSO: oss.FCL.sf.mw.inputmethods/textinput/AknInputLanguage/group/akninputlanguage.mmp
    "alwaysonlinemanagerclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/alwaysonline/AlwaysOnlineManager/group/AlwaysOnlineManagerClient.mmp"),   # <- imumutils; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/alwaysonline/AlwaysOnlineManager/group/AlwaysOnlineManagerClient.mmp
    "alwaysonlinemanagerserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/alwaysonline/AlwaysOnlineManager/group/AlwaysOnlineManagerServer.mmp"),   # <- alwaysonlinemanagerclient; ALSO: oss.FCL.sf.mw.messagingmw/messagingfw/alwaysonline/AlwaysOnlineManager/group/AlwaysOnlineManagerServer.mmp
    "connmonexe": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionmonitoring/connmon/connectionmonitor/group/ConnMonExe.mmp"),   # the Connection Monitor server (MPM connects to it)
    "connmonext": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionmonitoring/connmonext/group/ConnMonExt.mmp"),   # its extension API
    # the reference one) asks the notifier server; the ECom wrapper maps the notifier UIDs to the plug-in DLLs
    "agentdialog": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/CConnDlgClient.mmp"),
    "cconndlgplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/CConnDlgPlugin.mmp",   # board delta: LAN APs as on the emulator (toolchain/shims/conndlg_board)
        drop=["ActiveSelectConnectionPlugin.cpp"], extra=["toolchain/shims/conndlg_board/ActiveSelectConnectionPlugin.cpp"]),
    "connectiondialogswrapper": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/connectiondialogswrapper.mmp"),
    "disconnectdlg": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/DisconnectDlg.mmp",   # board delta: LAN APs as on the emulator (toolchain/shims/conndlg_board)
        drop=["ActiveDisconnectDlgPlugin.cpp"], extra=["toolchain/shims/conndlg_board/ActiveDisconnectDlgPlugin.cpp"]),
    "disconnectdlgclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/DisconnectDlgClient.mmp"),
    "connuiutilsnotif": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionutilities/ConnectionDialogs/group/ConnUiUtilsNotif.mmp"),
    "connmon": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.ipconnmgmt/connectionmonitoring/connmon/connectionmonitor/group/ConnMon.mmp",
                    # board delta: LAN IAPs available as on the emulator (toolchain/shims/connmon_board)
                    drop=["ConnMonBearerLAN.cpp"], extra=["toolchain/shims/connmon_board/ConnMonBearerLAN.cpp"]),   # <- alwaysonlinemanagerserver; ALSO: oss.FCL.sf.mw.ipconnmgmt/connectionmonitoring/connmon/connectionmonitor/group/ConnMon.mmp
    "policyengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/policymanagement/policyengine/group/PolicyEngineClient.mmp"),   # <- imumutils; ALSO: oss.FCL.sf.mw.remotemgmt/policymanagement/policyengine/group/PolicyEngineClient.mmp
    "dmutil": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.remotemgmt/policymanagement/dmutilserver/group/DMUtilClient.mmp"),   # <- policyengine; ALSO: oss.FCL.sf.mw.remotemgmt/policymanagement/dmutilserver/group/DMUtilClient.mmp
    "aknhlist": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.classicui/uifw/AvKon/aknhlist/group/aknhlist.mmp"),   # <- mce.mmp; ALSO: oss.FCL.sf.mw.classicui/uifw/AvKon/aknhlist/group/aknhlist.mmp
    # Messaging's run-time set: the message server EXE (msgs.dll's server side runs in it) and the SMS UI
    # MTM (smum; also carries sms.rsc, the SMS MTM registration the server reads from resource\messaging\mtm)
    "msexe": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.messagingmw/messagingfw/msgsrvnstore/serverexe/group/MSEX.MMP"),
    "smum": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/smum/group/SMUM.MMP"),
    "gmsmodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/smartmessaging/gmsmodel/group/gmsModel.mmp"),   # <- smum
    # the message-editor base (every editor/viewer). The S60 new-file service (camera/recorder capture) was
    # never released: OUR headers in toolchain/shims/newfileservice_inc, its newservice.dll imports are stubs
    "msgeditorview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgeditor/view/MsgEditorView.mmp",
                          flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),   # <- gmsmodel
    "msgeditormodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgeditor/model/MsgEditorModel.mmp",
                           flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),   # <- msgeditorview
    # .rsg inputs of msgeditorview: the editor app-UI base, and the MMS viewer (an app of its own)
    "msgeditorappui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgeditor/appui/MsgEditorAppUi.mmp",
                           flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),
    "mmsviewer": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/mmsui/viewer/MmsViewer.mmp",
                      flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),
    "xhtmlparser": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/smilengine/xhtml/group/xhtml.mmp"),   # <- msgeditorview
    "unieditor": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/application/bld/UniEditor.mmp", flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),   # the root
    "mmsui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/mmsui/mtm/MmsUi.mmp", flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),   # the MMS UI MTM (MmsUi.rsg <- unimtms)
    "unimtms": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/mtm/bld/UniMtms.mmp"),   # the root
    "unismsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/smsplugin/bld/UniSmsPlugin.mmp", flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),   # the root
    "unimmsplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/mmsplugin/bld/UniMmsPlugin.mmp"),   # the root
    "smsviewer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/smsui/viewer/SmsViewer.mmp"),   # the root
    "smilplayer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/smilui/player/SmilPlayer.mmp"),   # <- UniEditor.mmp
    "smileng": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/smilengine/engine/group/smil.mmp"),   # <- smilplayer
    "smiltran": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/smilengine/transition/group/transition.mmp"),   # <- smilplayer
    "smilmediarenderer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/smilui/media/mediarenderer.mmp", flags=UNRELEASED_INC,
                              # the SVG-T UI control (svgtuicontrol) was never released: no SVG slide renderer;
                              # the factory delta sends SVG to the default renderer (toolchain/shims/smilui_gcc)
                              drop=["SmilSVGRenderer.cpp", "SmilPlayerMediaFactory.cpp"],
                              extra=["toolchain/shims/smilui_gcc/SmilPlayerMediaFactory.cpp"]),   # <- smilplayer
    "msgmedia": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/msgmedia/group/MsgMedia.mmp", flags=["-idirafter", os.path.join(SYMBIAN, "symbian-2010/oss.FCL.sf.mw.helix/helix_ren/helix_stub/ThumbnailAPI/inc")],
                     # GCC delta: member-function pointers without &Class:: -> toolchain/shims/msgmedia_gcc
                     drop=["MmsConformance.cpp"], extra=["toolchain/shims/msgmedia_gcc/MmsConformance.cpp"]),   # <- smilmediarenderer
    "hxmetadatautil": dict(mmp="oss.FCL.sf.mw.helix/helix_ren/helix_stub/ThumbnailAPI/group/hxmetadatautil.mmp"),   # <- msgmedia
    "remconinterfacebase": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/client/intermediate/group/remconinterfacebase.mmp"),   # <- smilplayer
    "remconclient": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/client/inner/group/remconclient.mmp"),   # <- remconinterfacebase
    "remcontypes": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/types/group/remcontypes.mmp"),   # <- remconclient
    "remconcoreapi": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/remotecontrolfw/client/coreapi/group/remconcoreapi.mmp"),   # <- smilplayer
    "unipluginapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/pluginapi/bld/UniPluginApi.mmp"),   # <- UniEditor.mmp
    "unidatamodel": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/model/bld/UniDataModel.mmp", flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),   # <- UniEditor.mmp
    "uniutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/utils/bld/UniUtils.mmp", flags=["-I" + os.path.join(OS_ROOT, "toolchain/shims/newfileservice_inc")]),   # <- UniEditor.mmp
    "msgeditormediacontrol": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/msgeditor/mediacontrol/MsgEditorMediaControl.mmp"),   # <- uniutils
    "videoconversionapi": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/unieditor/videoconversionapi/bld/VideoConversionApi.mmp"),   # <- UniEditor.mmp
    "mmssettings": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mobilemessaging/mmsui/settings/MmsSettings.mmp"),   # <- UniMmsPlugin.mmp
    "msgeditorutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/smartmessaging/msgeditorutils/group/MsgEditorUtils.mmp"),   # <- SmsViewer.mmp
    "richbio": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/messagingappbase/smartmessaging/richbio/group/RichBio.mmp"),   # <- msgeditorutils
    # the Open C zlib with gzio (gzopen/gzgets: gzipped message files) over libzcore
    "libz": dict(mmp=f"{OSSRV}/compressionlibs/ziplib/group/libz/libz.mmp"),
    # P.I.P.S. E32Main -> __crt0 -> main(): the STATICLIBRARY every Open C EXE links
    "libcrt0": dict(mmp=_OENV + "backend/group/mcrt0.mmp"),
    # its `device` twin (Leave = C++ throw; the plain one calls TTrap::Trap, which the device euser does not export): device
    "libcrt0_dev": dict(mmp=_OENV + "backend/group/mcrt0.mmp", device=True),
    # publishes (bootui used to fake it). regsuggest closure of phoneui.mmp; the os.mmaudio entries are LINK
    # dependencies (effect / DevSound-extension proxies) -- audio itself stays the last rung
    "phoneui": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneui/group/phoneui.mmp",
                    # its .loc files and the bubble manager's touch-pane .rh (no bld.inf exports them)
                    rsc_incs=["baseline/symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneui/loc",
                              "baseline/symbian-rcl3/oss.FCL.sf.app.phone/phoneuis/BubbleManager/Inc",
                              "baseline/symbian-rcl3/oss.FCL.sf.app.phone/phone_plat/phone_application_commands_api/inc"],
                    # the video-sharing button text: no .loc in the release defines it (OURS English)
                    rdefs=['qtn_incal_button_share_video="Share video"']),   # the root
    "phoneuiview": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneuiview/group/phoneuiview.mmp", flags=API_REF_LAST),   # musmanager.h -> sipstrconsts.h   # <- phoneui.mmp
    # the Phone app's protocol state machine: phoneuicontrol RLibrary::Loads it by name (KGSMProtocol), so no
    # import closure reaches it -- without it CPhoneStateHandle panics "Common phone" 41 (FactoryLibraryNotFound)
    "phoneuistates": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneuistates/group/phoneuistates.mmp"),
    # loads PhoneUIVoIPExtension.dll (KVoIPExtension, UID2 KUidAppVoIPExtensionStates) instead
    "phoneuivoipextension": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneuivoipextension/group/phoneuivoipextension.mmp"),
    # the Phone app's wserv anim DLL (CPhoneClearBlacklist: RAnimDll::Load by name) -- clears the ETel autoredial blacklist
    "phoneanimdll": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/Blacklist/group/phoneanimdll.mmp"),
    "phoneserver": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/phoneclientserver/phoneserver/Group/PhoneServer.mmp"),
    "vccuipropertyhandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.iptelephony/callcontinuity/vccpropertyhandler/group/vccuipropertyhandler.mmp"),   # <- phoneuivoipextension
    "phoneengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/phonemodel/group/phonemodel.mmp",
                        drop=["cpeparsermanufacturerhandler.cpp"], extra=["toolchain/shims/phone_devmenu/cpeparsermanufacturerhandler.cpp"]),
    "audiohandling": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/audiohandling/group/audiohandling.mmp"),   # <- phoneengine; ALSO: oss.FCL.sf.app.phone/phoneengine/audiohandling/group/audiohandling.mmp
    "callhandling": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/callhandling/group/callhandling.mmp"),   # <- phoneengine; ALSO: oss.FCL.sf.app.phone/phoneengine/callhandling/group/callhandling.mmp
    "contacthandling": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/contacthandling/group/contacthandling.mmp"),   # <- phoneengine; ALSO: oss.FCL.sf.app.phone/phoneengine/contacthandling2/group/contacthandling.mmp
    "loghandling": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/loghandling/group/loghandling.mmp"),   # <- phoneengine; ALSO: oss.FCL.sf.app.phone/phoneengine/loghandling/group/loghandling.mmp
    "phoneparser": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/telutils/phoneparser/group/phoneparser.mmp",
                        drop=["PhoneGsmParser.cpp"], extra=["toolchain/shims/phone_devmenu/PhoneGsmParser.cpp"]),
    "psui": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/cellular/PsetNotesUi/Group/Psui.mmp"),   # <- phoneengine
    "servicerequest": dict(libs=["eikdlg.lib"], mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/cellular/ServiceRequest/group/ServiceRequest.mmp"),   # <- psui
    "peservicehandling": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/servicehandling/group/peservicehandling.mmp"),   # <- phoneengine; ALSO: oss.FCL.sf.app.phone/phoneengine/servicehandling/group/peservicehandling.mmp
    "phoneuiutils": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneuiutils/group/phoneuiutils.mmp",
                         # cphonerecoverysystem.cpp: AddIdToContainer throws KErrNoSpace when full and its caller's
                         # catch(TInt) only asserts in debug. We build -fno-exceptions: the throw becomes a plain
                         # return, the handler dead code -- the same release behaviour
                         defs=["try=if(1)", "catch(x)=if(0)", "throw="]),   # <- phoneuiview; ALSO: oss.FCL.sf.app.phone/phoneapp/phoneuiutils/group/phoneuiutils.mmp
    "cauiengine": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/phoneclientserver/CallUI/Group/CaUiEngine.mmp"),   # <- phoneuiutils
    "mmscli": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.messaging/mmsengine/clientmtm/bld/mmsclient.mmp"),   # <- cauiengine; ALSO: oss.FCL.sf.app.messaging/mmsengine/clientmtm/bld/mmsclient.mmp
    # the Converged Connection Handler client (iptelephony, RCL_3 only: master is empty) -- serviceselector's
    "cch": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.iptelephony/convergedconnectionhandler/cchclientapi/group/cch.mmp"),
    "cchclient": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.iptelephony/convergedconnectionhandler/cchclient/group/cchclient.mmp"),
    "serviceselector": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/convergedcallengine/serviceselector/group/serviceselector.mmp"),   # <- cauiengine
    "bubblemanager": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneuis/BubbleManager/Group/Bubblemanager.mmp"),   # <- phoneuiview
    "bmcustomization": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneuis/BubbleManager/bmcustomization/group/bmcustomization.mmp"),   # <- bubblemanager
    "audiooutputrouting": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/audiorouting/Output/AudioOutputBase/group/AudioOutput.mmp"),   # <- phoneuiview
    "custominterfaceutility": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmextfw/custominterfaceutility/CustomInterfaceUtility/Group/CustomInterfaceUtility.mmp"),   # <- audiooutputrouting
    "customcommandutility": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmextfw/customcommandutility/Group/CustomCommandUtility.mmp"),   # <- custominterfaceutility
    "midiclient": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MIDI/MMPFiles/Midi/MidiClient.mmp"),   # <- customcommandutility
    "midistandardcustomcommands": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MIDI/MMPFiles/Midi/MidiStandardCustomCommands.mmp"),   # <- midiclient
    "custominterfaceproxyfactory": dict(mmp="oss.FCL.sf.adapt.stubs/audiostubs/devsoundextensions_stubs/audioenhancementfactorystubs/CustomInterfaceProxyFactory_stub/group/CustomInterfaceProxyFactory_stub.mmp"),   # <- custominterfaceutility
    "audioequalizerproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/AudioEqualizer/AudioEqualizerProxy/Group/AudioEqualizerProxy.mmp"),   # <- custominterfaceproxyfactory
    "effectbase": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/EffectBase/Group/EffectBase.mmp"),   # <- audioequalizerproxy
    "audioequalizereffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/AudioEqualizer/AudioEqualizerEffect/Group/AudioEqualizerEffect.mmp"),   # <- audioequalizerproxy
    "mediaclientaudioinputstream": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/client/MediaClientAudioInputStream.mmp"),   # <- audioequalizereffect
    "mediaclientaudiostream": dict(mmp="oss.FCL.sf.os.mmaudio/mmlibs/mmfw/MMPFiles/client/MediaClientAudioStream.mmp"),   # <- audioequalizereffect
    "environmentalreverbproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/EnvReverb/EnvironmentalReverbProxy/Group/EnvironmentalReverbProxy.mmp"),   # <- custominterfaceproxyfactory
    "environmentalreverbeffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/EnvReverb/EnvironmentalReverbEffect/Group/EnvironmentalReverbEffect.mmp"),   # <- environmentalreverbproxy
    "stereowideningproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/StereoWidening/StereoWideningProxy/Group/StereoWideningProxy.mmp"),   # <- custominterfaceproxyfactory
    "stereowideningeffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/StereoWidening/StereoWideningEffect/Group/StereoWideningEffect.mmp"),   # <- stereowideningproxy
    "bassboostproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/BassBoost/BassBoostProxy/Group/BassBoostProxy.mmp"),   # <- custominterfaceproxyfactory
    "bassboosteffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/BassBoost/BassBoostEffect/Group/BassBoostEffect.mmp"),   # <- bassboostproxy
    "listenerdopplerproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/ListDoppler/ListenerDopplerProxy/Group/ListenerDopplerProxy.mmp"),   # <- custominterfaceproxyfactory
    "dopplerbase": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/DopplerBase/Group/DopplerBase.mmp"),   # <- listenerdopplerproxy
    "listenerdopplereffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/ListDoppler/ListenerDopplerEffect/Group/ListenerDopplerEffect.mmp"),   # <- listenerdopplerproxy
    "listenerlocationproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/ListLocation/ListenerLocationProxy/Group/ListenerLocationProxy.mmp"),   # <- custominterfaceproxyfactory
    "locationbase": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/LocationBase/Group/LocationBase.mmp"),   # <- listenerlocationproxy
    "listenerlocationeffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/ListLocation/ListenerLocationEffect/Group/ListenerLocationEffect.mmp"),   # <- listenerlocationproxy
    "listenerorientationproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/ListOrient/ListenerOrientationProxy/Group/ListenerOrientationProxy.mmp"),   # <- custominterfaceproxyfactory
    "orientationbase": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/OrientationBase/Group/OrientationBase.mmp"),   # <- listenerorientationproxy
    "listenerorientationeffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/ListOrient/ListenerOrientationEffect/Group/ListenerOrientationEffect.mmp"),   # <- listenerorientationproxy
    "sourcedopplerproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/SrcDoppler/SourceDopplerProxy/Group/SourceDopplerProxy.mmp"),   # <- custominterfaceproxyfactory
    "sourcedopplereffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/SrcDoppler/SourceDopplerEffect/Group/SourceDopplerEffect.mmp"),   # <- sourcedopplerproxy
    "sourcelocationproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/SrcLocation/SourceLocationProxy/Group/SourceLocationProxy.mmp"),   # <- custominterfaceproxyfactory
    "sourcelocationeffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/SrcLocation/SourceLocationEffect/Group/SourceLocationEffect.mmp"),   # <- sourcelocationproxy
    "sourceorientationproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/SrcOrientation/SourceOrientationProxy/Group/SourceOrientationProxy.mmp"),   # <- custominterfaceproxyfactory
    "sourceorientationeffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/SrcOrientation/SourceOrientationEffect/Group/SourceOrientationEffect.mmp"),   # <- sourceorientationproxy
    "distanceattenuationproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/DistAtten/DistanceAttenuationProxy/Group/DistanceAttenuationProxy.mmp"),   # <- custominterfaceproxyfactory
    "distanceattenuationeffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/DistAtten/DistanceAttenuationEffect/Group/DistanceAttenuationEffect.mmp"),   # <- distanceattenuationproxy
    "loudnessproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/Loudness/LoudnessProxy/Group/LoudnessProxy.mmp"),   # <- custominterfaceproxyfactory
    "loudnesseffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/Loudness/LoudnessEffect/Group/LoudnessEffect.mmp"),   # <- loudnessproxy
    "roomlevelproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/RoomLevel/RoomLevelProxy/Group/RoomLevelProxy.mmp"),   # <- custominterfaceproxyfactory
    "roomleveleffect": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/effects/RoomLevel/RoomLevelEffect/Group/RoomLevelEffect.mmp"),   # <- roomlevelproxy
    "addeddevsoundcontrolproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/addeddevsoundcontrol/AddedDevSoundControlProxy/Group/AddedDevSoundControlProxy.mmp"),   # <- custominterfaceproxyfactory
    "restrictedaudiooutputproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/restrictedaudiooutput/RestrictedAudioOutputProxy/Group/RestrictedAudioOutputProxy.mmp"),   # <- custominterfaceproxyfactory
    "restrictedaudiooutput": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/restrictedaudiooutput/RestrictedAudioOutputBase/group/RestrictedAudioOutput.mmp"),   # <- restrictedaudiooutputproxy
    "audioinputproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/audiorouting/Input/AudioInputProxy/Group/AudioInputProxy.mmp"),   # <- custominterfaceproxyfactory
    "audiooutputproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/audiorouting/Output/AudioOutputProxy/Group/AudioOutputProxy.mmp"),   # <- custominterfaceproxyfactory
    "g711decoderintfcproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/G711DecoderIntfc/G711DecoderIntfcProxy/Group/G711DecoderIntfcProxy.mmp"),   # <- custominterfaceproxyfactory
    "g729decoderintfcproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/G729DecoderIntfc/G729DecoderIntfcProxy/Group/G729DecoderIntfcProxy.mmp"),   # <- custominterfaceproxyfactory
    "ilbcdecoderintfcproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/IlbcDecoderIntfc/IlbcDecoderIntfcProxy/Group/IlbcDecoderIntfcProxy.mmp"),   # <- custominterfaceproxyfactory
    "g711encoderintfcproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/G711EncoderIntfc/G711EncoderIntfcProxy/Group/G711EncoderIntfcProxy.mmp"),   # <- custominterfaceproxyfactory
    "g729encoderintfcproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/G729EncoderIntfc/G729EncoderIntfcProxy/Group/G729EncoderIntfcProxy.mmp"),   # <- custominterfaceproxyfactory
    "ilbcencoderintfcproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/IlbcEncoderIntfc/IlbcEncoderIntfcProxy/Group/IlbcEncoderIntfcProxy.mmp"),   # <- custominterfaceproxyfactory
    "speechencoderconfigproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/SpeechEncoderConfig/SpeechEncoderConfigProxy/Group/SpeechEncoderConfigProxy.mmp"),   # <- custominterfaceproxyfactory
    "errorconcealmentintfcproxy": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/ErrConcealmentIntfc/ErrorConcealmentIntfcProxy/Group/ErrorConcealmentIntfcProxy.mmp"),   # <- custominterfaceproxyfactory
    "dialer": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneuis/dialer/group/dialer.mmp"),   # <- phoneuiview
    "phonemediatorcenter": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phonemediatorcenter/group/phonemediatorcenter.mmp"),   # <- phoneuiview; ALSO: oss.FCL.sf.app.phone/phoneapp/phonemediatorcenter/group/phonemediatorcenter.mmp
    "phoneuicontrol": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneuicontrol/group/phoneuicontrol.mmp",
                           drop=["cphonestatestartup.cpp", "cphonestate.cpp"],
                           extra=["toolchain/shims/phoneuicontrol_board/cphonestatestartup.cpp", "toolchain/shims/phone_devmenu/cphonestate.cpp"],
                           flags=["-iquote", os.path.join(SYMBIAN, "symbian-rcl3/oss.FCL.sf.app.phone/phoneapp/phoneuicontrol/src")]),   # <- phonemediatorcenter; ALSO: oss.FCL.sf.app.phone/phoneapp/phoneuicontrol/group/phoneuicontrol.mmp
    "engineinfo": dict(mmp="../symbian-rcl3/oss.FCL.sf.app.phone/phoneengine/engineinfo/group/engineinfo.mmp"),   # <- phoneuicontrol; ALSO: oss.FCL.sf.app.phone/phoneengine/engineinfo/group/engineinfo.mmp
    "cce": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/convergedcallengine/cce/group/cce.mmp"),   # <- engineinfo; ALSO: oss.FCL.sf.mw.phonesrv/convergedcallengine/cce/group/cce.mmp
    "callprovider": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/convergedcallengine/callprovider/group/callprovider.mmp"),   # <- cce; ALSO: oss.FCL.sf.mw.phonesrv/convergedcallengine/callprovider/group/callprovider.mmp
    "csplugin": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/convergedcallengine/csplugin/group/csplugin.mmp",
                     drop=["cspdevsound.cpp", "cspaudiostreams.cpp"],
                     extra=["toolchain/shims/csplugin_board/cspdevsound.cpp", "toolchain/shims/csplugin_board/cspaudiostreams.cpp"]),   # the CS call provider (CCE -> ETel): telephony T1 -- without it the Phone engine has no call provider
    "phonecmdhandler": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/phonecmdhandler/phonecmdhnlr/group/PhoneCmdHandler.mmp"),   # <- phoneuicontrol; ALSO: oss.FCL.sf.mw.phonesrv/phonecmdhandler/phonecmdhnlr/group/phonecmdhandler.mmp
    "remconextensionapi": dict(mmp="oss.FCL.sf.os.devicesrv/accessoryservices/accessoryremotecontrol/group/RemConExtensionApi.mmp"),   # <- phonecmdhandler
    "telephonyservice": dict(mmp="../symbian-rcl3/oss.FCL.sf.mw.phonesrv/telutils/telephonyservice/group/telephonyservice.mmp"),   # <- phonecmdhandler; ALSO: oss.FCL.sf.mw.phonesrv/telutils/telephonyservice/group/telephonyservice.mmp
    "musmanagerclient": dict(mmp="oss.FCL.sf.app.mmsharinguis/mmsharing/mmshmanagercli/group/musmanagerclient.mmp",
                             flags=API_REF_LAST),   # musmanager.h -> sipstrconsts.h (the SIP stack is not in the release)   # <- phoneuiview
}

if os.environ.get("PH_DBG_MSG"):
    for _k, _fs in (("mce", ["mceui.cpp"]), ("smum", ["SMSU.CPP"]), ("muiu", ["MuiuMsgEditorService.cpp"]),
                    ("msgeditormodel", ["MsgEditorDocument.cpp"]), ("smsviewer", ["MsgSmsViewerAppUi.cpp"]),
                    ("unieditor", ["UniEditorAppUi.cpp", "UniEditorOperation.cpp", "UniEditorSendOperation.cpp"]),
                    ("unismsplugin", ["UniSmsPlugin.cpp"]), ("msgs", ["CMsvCachedStore.cpp"])):
        COMPONENTS[_k].update(drop=COMPONENTS[_k].get("drop", []) + _fs,
                              extra=COMPONENTS[_k].get("extra", []) + ["toolchain/shims/dbg_phmsg/" + _f for _f in _fs])
    # smcm already swaps SMUTHDR.CPP for the OURS fix (smcm_fix): trace that copy instead
    COMPONENTS["smcm"]["extra"] = [("toolchain/shims/dbg_phmsg/SMUTHDR.CPP" if e.endswith("smcm_fix/SMUTHDR.CPP") else e) for e in COMPONENTS["smcm"]["extra"]]


def rom_name(key: str, targettype: str = "dll") -> str:
    c = COMPONENTS[key]
    if "rom" in c:
        return c["rom"]
    return key + (".exe" if targettype in ("exe", "exexp", "stdexe") else ".dll")


def _check_duplicate_keys(path: str = __file__) -> None:
    """A repeated key in the COMPONENTS literal silently replaces the earlier entry (the Clock
    app registered as "clock" displaced classicui's clock.dll, and only a ROM link noticed). Refuse it.
    (Also run over components_s4.py: its "stsserver" once hid a second one.)"""
    import ast
    tree = ast.parse(open(os.path.abspath(path), encoding="utf-8").read())
    for node in ast.walk(tree):
        if isinstance(node, ast.Dict):
            keys = [k.value for k in node.keys if isinstance(k, ast.Constant)]
            dup = sorted({k for k in keys if keys.count(k) > 1})
            if dup:
                raise SystemExit(f"{os.path.basename(path)}: duplicate registry key(s) {dup}")


_check_duplicate_keys()

# PHOREPOC_S4=1: the Symbian^4 registry (Qt 4.7 / Orbit / the Qt homescreen) on top of this one
if os.environ.get("PHOREPOC_S4") == "1":
    import components_s4 as _S4  # noqa: E402
    _check_duplicate_keys(_S4.__file__)
    COMPONENTS.update(_S4.COMPONENTS)

# (PHOREPOC_QT48=1 only: the mmps are generated by tools/phorepoc-qt48/)
if os.environ.get("PHOREPOC_QT48") == "1":
    import components_qt48 as _QT48  # noqa: E402
    _check_duplicate_keys(_QT48.__file__)
    COMPONENTS.update(_QT48.COMPONENTS)

# the optional codec pack (AAC, FFmpeg) is its own repository, built in as a layer (codecpack: PHOREPOC_ADAPTATIONS)

import components_apps as _APPS  # noqa: E402
_check_duplicate_keys(_APPS.__file__)
for _k in _APPS.COMPONENTS:
    assert _k not in COMPONENTS, "components_apps: key already in components.py: " + _k
COMPONENTS.update(_APPS.COMPONENTS)

import components_phetty as _PHETTY  # noqa: E402
for _k in _PHETTY.COMPONENTS:
    assert _k not in COMPONENTS, "components_phetty: key already in components.py: " + _k
COMPONENTS.update(_PHETTY.COMPONENTS)

import components_devmenu as _DEVMENU  # noqa: E402
for _k in _DEVMENU.COMPONENTS:
    assert _k not in COMPONENTS, "components_devmenu: key already in components.py: " + _k
COMPONENTS.update(_DEVMENU.COMPONENTS)

# The adaptations built with core (PHOREPOC_ADAPTATIONS; their registry/ dirs are on sys.path, paths.py): each one's
# registry/components_<name>.py adds the board's own components. A key may not be defined twice.
# Its optional EXTEND table adds the board's hardware to a core component without redefining it:
# {key: dict(drop=[source basenames], extra=[OS paths], ...)} -- list values are appended, others set
# (phgl: adaptation-a64 swaps the null GPU channel for the Mali one).
import importlib as _il  # noqa: E402
import paths as _P  # noqa: E402
_EXTEND = []
for _reg in _P.ADAPTATION_REGISTRIES:
    for _f in sorted(os.listdir(_reg)):
        if _f.startswith("components_") and _f.endswith(".py"):
            _m = _il.import_module(_f[:-3])
            for _k in _m.COMPONENTS:
                assert _k not in COMPONENTS, f"{_reg}/{_f}: key already registered: {_k}"
            COMPONENTS.update(_m.COMPONENTS)
            _EXTEND += [(f"{_reg}/{_f}", _k, _v) for _k, _v in getattr(_m, "EXTEND", {}).items()]
for _src, _k, _v in _EXTEND:
    assert _k in COMPONENTS, f"{_src}: EXTEND of an unknown component: {_k}"
    _c = COMPONENTS[_k] = dict(COMPONENTS[_k])
    for _field, _val in _v.items():
        _c[_field] = list(_c.get(_field) or []) + list(_val) if isinstance(_val, list) else _val

# Staged as sflibopenvgrefimpl.dll at ALF>=s2 in place of the default one.
if _ALFTRACE:
    _riw = dict(COMPONENTS["sflibopenvgrefimpl"])
    _riw["drop"] = list(_riw["drop"]) + ["riminiegl.cpp", "riapi.cpp"]
    _riw["extra"] = [e for e in _riw["extra"] if not e.endswith("sfopenvg_board/riImage.cpp")] + [
        "toolchain/shims/alf_fix/gen/riImage_ph.cpp", "toolchain/shims/alf_fix/gen/riMiniEGL_ph.cpp", "toolchain/shims/alf_fix/ri_assert.cpp",
        "toolchain/shims/alf_fix/gen/riApi_ph.cpp", "toolchain/shims/alf_fix/ri_fastblit.cpp"]
    _riw["defs"] = list(_riw["defs"]) + ["PH_ALF_FASTCLEAR"]   # riImage.cpp Surface::clear row fast path (the alf window mask clear took minutes)
    _riw["libs"] = list(_riw.get("libs", [])) + ["hal.lib"]   # ri_fastblit.cpp: HAL::Get(EFastCounterFrequency)
    _riw.update(alf=2, rom="sflibopenvgrefimpl_alf.dll", rom_as="sflibopenvgrefimpl.dll")
    COMPONENTS["sflibopenvgrefimpl_alf"] = _riw
