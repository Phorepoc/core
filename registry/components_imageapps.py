# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

"""components_imageapps -- the 2010 image applications: Photo Browser (OpenGL ES, over our libGLES_CM) with its engine,
the imaging framework's test viewer, and the image editor. Merged into components.COMPONENTS (components.py); a key
ships once an image has been built for it. Paths are relative to symbian-2010 like components.py."""
import os

from paths import OS_ROOT, SYMBIAN  # noqa: E402

_IE = "oss.FCL.sf.app.imgeditor/"
_PB = "oss.FCL.sf.incubator.photobrowser/"
_PBD = os.path.join(SYMBIAN, "symbian-2010", _PB)
# the mmps spell their include folders in another letter case than the checkout (Common/Inc, ..\Common\inc): every
# folder both ways, and group/ for UidList.txt
_PB_INC = []
for _d in ("group", "Common/Inc", "Common/inc", "EngInc", "IEBgps/inc", "IEBgpClient/inc", "AppInc", "data"):
    _PB_INC += ["-I", _PBD + _d]


# Photo Browser's own registration lists no data types, so it is no candidate of Default applications > View images and File Manager
# cannot hand it a picture. Its mmp is re-written next to the original (same folder, so every relative path stays right), with the
# registration resource swapped for our patched copy (os/src/imageapps/PhotoBrowser_reg.rss: the JPEG types at High priority), which
# is copied into the original's data/ folder too: the build runs on the host and in containers, where our os/ tree has other paths.
_PB_MMP_SRC = os.path.join(_PBD, "group", "Imagic.mmp")
_PB_MMP = _PB + "group/Imagic_phe.mmp"
_PB_REG_SRC = os.path.join(OS_ROOT, "src/imageapps/PhotoBrowser_reg.rss")
if os.path.isfile(_PB_MMP_SRC) and os.path.isfile(_PB_REG_SRC):
    import re as _re
    _t = open(_PB_MMP_SRC, encoding="latin-1", newline="").read()
    _t2 = _re.sub(r"(?im)^(START RESOURCE\s+)PhotoBrowser_reg\.rss", lambda m: m.group(1) + "PhotoBrowser_phe_reg.rss", _t)
    assert _t2 != _t, "Imagic.mmp: no PhotoBrowser_reg.rss resource to swap"
    for _dst, _txt in ((os.path.join(_PBD, "group", "Imagic_phe.mmp"), _t2),
                       (os.path.join(_PBD, "data", "PhotoBrowser_phe_reg.rss"), open(_PB_REG_SRC, encoding="latin-1", newline="").read())):
        try:
            if not os.path.isfile(_dst) or open(_dst, encoding="latin-1", newline="").read() != _txt:
                open(_dst, "w", encoding="latin-1", newline="").write(_txt)
        except OSError:
            pass

COMPONENTS = {
    # --- Photo Browser ("Imagic"): Avkon + GLES 1.x; engine = ieutils (common) < iebgps (thumbnail server) < iebgpclient < ieengine
    "ieutils": dict(mmp=_PB + "group/IEUtils.mmp", flags=_PB_INC, drop=["IEEngineUtils.cpp"], extra=["src/imageapps/IEEngineUtils.cpp"], **{"def": os.path.join(OS_ROOT, "src/imageapps/ieutils.def")}),
    "iebgps": dict(mmp=_PB + "IEBgps/group/IEBgps.mmp", flags=_PB_INC, drop=["IETNGenerator.cpp"], extra=["src/imageapps/IETNGenerator.cpp"], **{"def": os.path.join(OS_ROOT, "src/imageapps/iebgps.def")}),
    "iebgpclient": dict(mmp=_PB + "IEBgpClient/group/IEBgpClient.mmp", flags=_PB_INC, drop=["IEImagicGBPSAO.cpp"], extra=["src/imageapps/IEImagicGBPSAO.cpp"], **{"def": os.path.join(OS_ROOT, "src/imageapps/iebgpclient.def")}),
    "ieengine": dict(mmp=_PB + "group/IEEngine.mmp", flags=_PB_INC, drop=["IEBgpsController.cpp"], extra=["src/imageapps/IEBgpsController.cpp"], **{"def": os.path.join(OS_ROOT, "src/imageapps/ieengine.def")}),
    "photobrowser": dict(mmp=_PB_MMP, flags=_PB_INC + ["-fpermissive", "-include", os.path.join(OS_ROOT, "src/imageapps/ImagicAppUi.h")],
                     drop=["DrawUtility.cpp", "ImagicContainerBrowser.cpp", "ImagicAppUi.cpp"],   # patched copies: implicit int, a case label crossing an initialization; ImagicAppUi(.h/.cpp): opens the one picture File Manager passes (PHOREPOC PATCH; the header is force-included so it wins over AppInc/)
                     extra=["src/imageapps/DrawUtility.cpp", "src/imageapps/ImagicContainerBrowser.cpp", "src/imageapps/ImagicAppUi.cpp"]),
    # --- the imaging framework's test viewer, re-made in Avkon (its Techview UI libraries are not in the OS)
    "timageviewer": dict(mmp=os.path.join(OS_ROOT, "src/timageviewer/timageviewer.mmp")),
    # --- Image Editor (app.imgeditor): the app, its manager/UI/engine libraries, the filters (loaded by name) and the editing plug-ins (.pgn, entry = ordinal 1)
    "imageeditorprovider": dict(mmp=_IE + "imageeditor/EditorProvider/group/ImageEditorProvider.mmp"),
    "iepb": dict(mmp=_IE + "imageeditor/IEPB/group/imageeditorpluginbase.mmp"),
    "imageeditormanager": dict(mmp=_IE + "imageeditor/ImageEditorManager/group/ImageEditorManager.mmp"),
    "imageeditorui": dict(mmp=_IE + "imageeditor/ImageEditorUI/group/ImageEditorUI.mmp"),
    "pb": dict(mmp=_IE + "imageeditor/PB/group/pluginbase.mmp"),
    "imageeditor": dict(mmp=_IE + "imageeditor/imageeditorApp/group/ImageEditor.mmp"),
    "ie_blackwhite": dict(mmp=_IE + "imageeditor/plugins/BlackWhitePlugin/group/BlackWhitePlugin.mmp", rom="BlackWhite.pgn"),
    "ie_brightness": dict(mmp=_IE + "imageeditor/plugins/BrightnessPlugin/group/BrightnessPlugin.mmp", rom="Brightness.pgn"),
    "ie_bubble": dict(mmp=_IE + "imageeditor/plugins/BubblePlugin/group/BubblePlugin.mmp", rom="Bubble.pgn"),
    "ie_cartoonize": dict(mmp=_IE + "imageeditor/plugins/CartoonizePlugin/group/CartoonizePlugin.mmp", rom="Cartoonize.pgn"),
    "ie_clipart": dict(mmp=_IE + "imageeditor/plugins/ClipartPlugin/group/ClipartPlugin.mmp", rom="Clipart.pgn"),
    "ie_contrast": dict(mmp=_IE + "imageeditor/plugins/ContrastPlugin/group/ContrastPlugin.mmp", rom="Contrast.pgn"),
    "ie_crop": dict(mmp=_IE + "imageeditor/plugins/CropPlugin/group/CropPlugin.mmp", rom="Crop.pgn"),
    "ie_draw": dict(mmp=_IE + "imageeditor/plugins/DrawPlugin/group/drawplugin.mmp", rom="draw.pgn"),
    "ie_frame": dict(mmp=_IE + "imageeditor/plugins/FramePlugin/group/FramePlugin.mmp", rom="Frame.pgn"),
    "ie_negative": dict(mmp=_IE + "imageeditor/plugins/NegativePlugin/group/NegativePlugin.mmp", rom="Negative.pgn"),
    "ie_redeyereduction": dict(mmp=_IE + "imageeditor/plugins/RedEyePlugin/group/RedEyeReductionPlugin.mmp", rom="RedEyeReduction.pgn"),
    "ie_resize": dict(mmp=_IE + "imageeditor/plugins/ResizePlugin/group/ResizePlugin.mmp", rom="Resize.pgn"),
    "ie_rotateleft": dict(mmp=_IE + "imageeditor/plugins/RotateLPlugin/group/RotateLPlugin.mmp", rom="RotateLeft.pgn"),
    "ie_rotateright": dict(mmp=_IE + "imageeditor/plugins/RotateRPlugin/group/RotateRPlugin.mmp", rom="RotateRight.pgn"),
    "ie_sepia": dict(mmp=_IE + "imageeditor/plugins/SepiaPlugin/group/SepiaPlugin.mmp", rom="Sepia.pgn"),
    "ie_sharpness": dict(mmp=_IE + "imageeditor/plugins/SharpnessPlugin/group/SharpnessPlugin.mmp", rom="sharpness.pgn"),
    "ie_text": dict(mmp=_IE + "imageeditor/plugins/TextPlugin/group/TextPlugin.mmp", rom="Text.pgn"),
    "enginewrapper": dict(mmp=_IE + "imageeditorengine/EngineWrapper/group/EngineWrapper.mmp"),
    "imageeditorutils": dict(mmp=_IE + "imageeditorengine/ImageEditorUtils/group/ImageEditorUtils.mmp"),
    "jpegrotator": dict(mmp=_IE + "imageeditorengine/JpegRotator/group/JpegRotator.mmp"),
    "jpegscaler": dict(mmp=_IE + "imageeditorengine/JpegScaler/group/JpegScaler.mmp"),
    "systemparameters": dict(mmp=_IE + "imageeditorengine/SystemParameters/group/SystemParameters.mmp"),
    "filterbrightness": dict(mmp=_IE + "imageeditorengine/filters/FilterBrightness/group/FilterBrightness.mmp"),
    "filterbubble": dict(mmp=_IE + "imageeditorengine/filters/FilterBubble/group/FilterBubble.mmp"),
    "filterbuffer": dict(mmp=_IE + "imageeditorengine/filters/FilterBuffer/group/FilterBuffer.mmp"),
    "filtercartoon": dict(mmp=_IE + "imageeditorengine/filters/FilterCartoon/group/FilterCartoon.mmp"),
    "filterclipart": dict(mmp=_IE + "imageeditorengine/filters/FilterClipart/group/FilterClipart.mmp"),
    "filtercontrast": dict(mmp=_IE + "imageeditorengine/filters/FilterContrast/group/FilterContrast.mmp"),
    "filtercrop": dict(mmp=_IE + "imageeditorengine/filters/FilterCrop/group/FilterCrop.mmp"),
    "filterdraw": dict(mmp=_IE + "imageeditorengine/filters/FilterDraw/group/filterdraw.mmp"),
    "filterframe": dict(mmp=_IE + "imageeditorengine/filters/FilterFrame/group/FilterFrame.mmp"),
    "filtergrayscale": dict(mmp=_IE + "imageeditorengine/filters/FilterGrayScale/group/FilterGrayScale.mmp"),
    "filtericlsource": dict(mmp=_IE + "imageeditorengine/filters/FilterIclSource/group/FilterIclSource.mmp"),
    "filterjpegsource": dict(mmp=_IE + "imageeditorengine/filters/FilterJpegSource/group/FilterJpegSource.mmp"),
    "filterjpegtarget": dict(mmp=_IE + "imageeditorengine/filters/FilterJpegTarget/group/FilterJpegTarget.mmp"),
    "filternegate": dict(mmp=_IE + "imageeditorengine/filters/FilterNegate/group/FilterNegate.mmp"),
    "filterredeye": dict(mmp=_IE + "imageeditorengine/filters/FilterRedEye/group/FilterRedEye.mmp"),
    "filterrotate": dict(mmp=_IE + "imageeditorengine/filters/FilterRotate/group/FilterRotate.mmp"),
    "filterscale": dict(mmp=_IE + "imageeditorengine/filters/FilterScale/group/FilterScale.mmp"),
    "filtersepia": dict(mmp=_IE + "imageeditorengine/filters/FilterSepia/group/FilterSepia.mmp"),
    "filtersharpness": dict(mmp=_IE + "imageeditorengine/filters/FilterSharpness/group/FilterSharpness.mmp"),
    "filtertext": dict(mmp=_IE + "imageeditorengine/filters/FilterText/group/FilterText.mmp"),
}
