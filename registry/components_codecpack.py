# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-1.0

"""components_codecpack -- the OPTIONAL codec pack (docs/phorepoc/audio.md "Codec policy"), merged into
components.COMPONENTS only when PHOREPOC_CODECPACK=1: AAC stays out of the default build, the regression and any
prebuilt image. Build it yourself: bash tools/codecpack/aac_codecpack.sh, then
    PHOREPOC_CODECPACK=1 ./ph rsc ariheaacdecmmfcodec aacaudioplaycontrollerplugin aacaudioplaycontrollerrecognizer
    PHOREPOC_CODECPACK=1 ./ph build ariheaacdecwrapper ariheaacdecmmfcodec aacdecoderconfig eaacplusutil \\
        eaacplusaudiocontrollerutility aacaudioplaycontrollerplugin aacaudioplaycontrollerrecognizer
    PHOREPOC_CODECPACK=1 ./ph rom
Everything but OUR decoder library and OUR header utility (the release has only a stub) is the 2010 release's own
code (the Aricent HE-AAC plug-in, the S60 AAC controller).
"""
import os

from paths import OS_ROOT, SYMBIAN, BUILD, EPOC32_ROOT  # noqa: E402
_AAC = "oss.FCL.sf.mw.mmmw/mmfenh/advancedaudiocontroller/audiocontrollerpluginsvariant"

COMPONENTS = {
    # OURS: the decoder library behind the Aricent HE-AAC plug-in = FFmpeg's AAC decoder (LGPL 2.1+, fetched and
    # configured by tools/codecpack/aac_codecpack.sh into .codecpack/, which also writes this mmp)
    "ariheaacdecwrapper": dict(mmp=os.path.join(OS_ROOT, ".codecpack", "ariheaacdecwrapper.mmp"), cdriver=True,
                               allow_wsd=True,   # FFmpeg's static tables (~300 KB: sine/cbrt/FFT twiddles, VLCs)
                               c_std="gnu11",
                               # FFmpeg's refstruct needs C11 <stdatomic.h>: GCC's own, after everything of ours
                               flags=["-idirafter", "/usr/lib/gcc/x86_64-linux-gnu/13/include",
                                      "-include", os.path.join(OS_ROOT, "src", "codecs", "ariheaacdecwrapper", "src",
                                                               "ffmpeg_board.h")],
                               defs=["HAVE_AV_CONFIG_H", "_ISOC99_SOURCE", "_FILE_OFFSET_BITS=64",
                                     "_LARGEFILE_SOURCE"]),
    # the release: the Aricent HE-AAC MMF plug-in (" EAC" -> " P16"), the S60 AAC controller + its utilities
    "ariheaacdecmmfcodec": dict(mmp="oss.FCL.sf.adapt.codecs.aricent/heaac_dec/ariheaacdecmmfcodec/group/ariheaacdecmmfcodec.mmp"),
    "aacdecoderconfig": dict(mmp="oss.FCL.sf.os.mmaudio/devsoundextensions/mmfcustominterfaces/AacDecoderConfig/AacDecoderConfig/group/AacDecoderConfig.mmp"),
    # OURS: the AAC header utility (the release has only a do-nothing stub) behind that stub's frozen exports
    "eaacplusutil": dict(mmp=os.path.join(OS_ROOT, "src", "codecs", "eaacplusutil", "group", "eaacplusutil.mmp"),
                         rtti=True, **{"def": "oss.FCL.sf.os.mmaudio/eaacplusutil_stub/eabi/eaacplusutil_stubu.def"}),
    "eaacplusaudiocontrollerutility": dict(mmp=_AAC + "/AdvancedAudioControllerUtility/Group/EAACPlusAudioControllerUtility.mmp"),
    "aacaudioplaycontrollerplugin": dict(mmp=_AAC + "/aacaudioplaycontroller/Group/AACAudioPlayControllerPlugin.mmp"),
    "aacaudioplaycontrollerrecognizer": dict(mmp=_AAC + "/aacaudioplaycontroller/Group/AACAudioPlayControllerRecognizer.mmp"),
}
