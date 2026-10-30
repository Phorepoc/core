# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

"""components_qt48 -- Qt 4.8.7 for the S^3 firmware, merged into components.COMPONENTS.

Keys are qt48_*; ROM names are the real Qt names (QtCore.dll ...). The mmps are the ones qmake GENERATES
(tools/phorepoc-qt48/qt_configure_symbian.sh + qt_mocables.sh), copied into the source module directory they were
written for by tools/phorepoc-qt48/qt_stage_mmps.sh. Export ordinals follow 4.8.7's frozen src/s60installs/eabi/*.def.
ARM: `ARM_VFP=1 python3 tools/emitter/build_mmp.py qt48_core` ... No C++ exceptions (QT_NO_EXCEPTIONS,
qt-patches/0001): the ROM's leave model is setjmp TRAP.
"""
import os

from paths import OS_ROOT, SYMBIAN, BUILD, EPOC32_ROOT  # noqa: E402
QB = os.path.join(BUILD, "qt48")
QT = os.path.join(QB, "qt-src")
QTINC = os.path.join(QB, "qt-symbian", "include")

# the generated mmps name /epoc32/mkspecs/... (where a Qt INSTALL put the mkspecs): the source tree's are the same
_MKSPECS = ["-I" + os.path.join(QT, "mkspecs", "common", "symbian", "stl-off"),
            "-I" + os.path.join(QT, "mkspecs", "common", "symbian", "header-wrappers"),
            "-I" + os.path.join(QT, "mkspecs", "common", "symbian"),
            "-I" + os.path.join(QB, "qt-symbian", "src", "corelib", "global")]    # configure's qconfig.h/.cpp

# x32 only (container paths): gcc's freestanding C++ headers (<exception>, <typeinfo>, <new>) AFTER everything of ours -- the 4.7 set's recipe
_X32CXX = ["-idirafter", "/usr/include/c++/13", "-idirafter", "/usr/include/x86_64-linux-gnu/c++/13/x32",
           "-idirafter", os.path.join(OS_ROOT, "toolchain", "shims", "qt_cxxstd")]
_QTDEFS = ["SYMBIAN_E32_ATOMIC_API", "QT_NO_EXCEPTIONS", "__declspec(x)=", "__SOFTFP="]


def _qt(module: str, mmp: str, flags=(), **kw) -> dict:
    specs = [f for f in _MKSPECS if "stl-off" not in f] if "stl-off" in kw.get("noinc", []) else _MKSPECS
    d = dict(mmp=os.path.join(QT, "src", module, mmp), flags=specs + list(flags), defs=_QTDEFS, vfp=True)
    d["x32_flags"] = _X32CXX
    d.update(kw)
    return d


def _dll(module, mmp, rom, deffile, **kw):
    return _qt(module, mmp, rom=rom, allow_wsd=True, cdriver=True, rtti=True, sis=True,
               **{"def": os.path.join(QT, "src/s60installs/eabi", deffile)}, **kw)


COMPONENTS = {
    "qt48_core": _dll("corelib", "QtCore_dll.mmp", "QtCore.dll", "QtCoreu.def", drop=["qatomic_generic_armv6.cpp"]),
    # import_typeinfo: QS60MainAppUi derives CAknAppUi -- its real typeinfo from avkon, so an app's
    # dynamic_cast<CAknAppUi*>(CEikonEnv::Static()->AppUi()) finds it (else NULL: King Oddball never asked for landscape)
    "qt48_gui": _dll("gui", "QtGui_dll.mmp", "QtGui.dll", "QtGuiu.def", import_typeinfo=True),
    "qt48_network": _dll("network", "QtNetwork_dll.mmp", "QtNetwork.dll", "QtNetworku.def"),
    "qt48_svg": _dll("svg", "QtSvg_dll.mmp", "QtSvg.dll", "QtSvgu.def"),
    # -fpie: qsqlrecord.cpp takes &QVectorData::shared_null - 8 as an absolute 32-bit immediate (R_X86_64_32, out of range at link); PIE code uses the rip-relative form the prelinker aims
    "qt48_sql": _dll("sql", "QtSql_dll.mmp", "QtSql.dll", "QtSqlu.def", x32_flags=_X32CXX + ["-fpie"]),
    "qt48_xml": _dll("xml", "QtXml_dll.mmp", "QtXml.dll", "QtXmlu.def"),
    # (4.8 folded the per-platform qts60plugin_* helpers into QtCore/QtGui: qt_resolveS60PluginFunc is an ABSENT slot)
    # Qt's E32Main -> main() glue (a static library every Qt program links)
    "qt48_main": _qt("s60main", "qtmain_lib.mmp"),
    # OURS: logs each Qt start-up step (find where a Qt app stops)
    "qt48_test": dict(mmp=os.path.join(OS_ROOT, "src", "qt48", "qttest", "qttest.mmp"), rom="qttest.exe", defs=_QTDEFS, rtti=True,
                      cdriver=True, vfp=True,
                      flags=_MKSPECS + ["-I" + QTINC] + ["-I" + os.path.join(QTINC, m) for m in ("QtCore", "QtGui")]),
    # OURS: a minimal Qt hello (QApplication + QLabel + QPushButton)
    "qt48_hello": dict(mmp=os.path.join(OS_ROOT, "src", "qt48", "qthello", "qthello.mmp"), rom="qthello.exe", defs=_QTDEFS, rtti=True,
                       cdriver=True, vfp=True,
                       flags=_MKSPECS + ["-I" + QTINC] + ["-I" + os.path.join(QTINC, m) for m in ("QtCore", "QtGui")]),
}

# Q2: QtScript (JavaScriptCore) + QtDeclarative (Qt Quick 1) -- the mmps qmake generated (qconfig.h edited to drop
# QT_NO_SCRIPT/QT_NO_DECLARATIVE/QT_NO_XMLPATTERNS, see qt48-build.md Q2)
COMPONENTS.update({
    "qt48_script": _dll("script", "QtScript_dll.mmp", "QtScript.dll", "QtScriptu.def", noinc=["stl-off"],
                        flags=["-I" + os.path.join(OS_ROOT, "tools", "phorepoc-qt48", "shims"),
                               "-I" + os.path.join(SYMBIAN, "kernelhwsrv", "kernel", "eka", "compsupp", "symcpp"),
                               "-DENABLE_JIT=0", "-DENABLE_YARR_JIT=0", "-DENABLE_ASSEMBLER=0"]),
    "qt48_declarative": _dll("declarative", "QtDeclarative_dll.mmp", "QtDeclarative.dll", "QtDeclarativeu.def"),
})

_MOB = os.path.join(SYMBIAN, "symbian-2010", "oss.FCL.sf.mw.qtmobility", "src")
COMPONENTS["qt48_multimediakit"] = dict(
    mmp=os.path.join(OS_ROOT, "src", "qt48", "qtmultimediakit", "QtMultimediaKit.mmp"), rom="QtMultimediaKit.dll",
    flags=_MKSPECS + ["-I" + os.path.join(QT, "include")] + ["-I" + os.path.join(QT, "include", m) for m in ("QtCore", "QtGui", "QtNetwork")]
          + ["-I" + os.path.join(_MOB, "global"), "-I" + os.path.join(QB, "mobility", "multimedia")] + ["-I" + os.path.join(_MOB, "multimedia", s) for s in ("", "audio", "video", "effects")]
          + ["-I" + os.path.join(EPOC32_ROOT, "", "include", "mmf", "common"), "-I" + os.path.join(EPOC32_ROOT, "", "include", "mmf", "server")],
    defs=_QTDEFS + ["QT_BUILD_MULTIMEDIA_LIB", "QT_MAKEDLL"], vfp=True, allow_wsd=True, cdriver=True, rtti=True,
    **{"def": os.path.join(_MOB, "s60installs", "eabi", "QtMultimediaKitu.def")})


# OURS: a minimal Qt Quick 1 test (QDeclarativeView + an inline QML string)
COMPONENTS["qt48_qmlhello"] = dict(mmp=os.path.join(OS_ROOT, "src", "qt48", "qmlhello", "qmlhello.mmp"), rom="qmlhello.exe", defs=_QTDEFS, rtti=True,
                                   cdriver=True, vfp=True,
                                   flags=_MKSPECS + ["-I" + QTINC] + ["-I" + os.path.join(QTINC, m) for m in ("QtCore", "QtGui", "QtDeclarative", "QtScript", "QtNetwork")])


# ---- Qt Mobility 1.2.0 (upstream tag v1.2.0, LGPL; tools/phorepoc-qt48/build_qtmobility_sis.sh) --------------------------------
# The mmps are the ones qmake generated from features/config.pri.symbian3 (one per DLL the real firmware carries: the module DLLs
# and the plug-ins). Keys are the lower-cased lib stems so an mmp's `LIBRARY QtSensors.lib` resolves to the component of that name;
# the four modules components_s4.py already registers (1.0.3 sources, S4 line) keep their S4 keys when PHOREPOC_S4=1.
QTM = os.path.join(QB, "qtm12-src")
_QTMINC = ["-I" + os.path.join(QTM, "src", "global"), "-I" + os.path.join(QTM, "include")] + \
          ["-I" + os.path.join(QTM, "include", "Qt" + m) for m in (
              "Bearer", "Contacts", "Feedback", "Gallery", "Location", "Messaging", "Organizer", "PublishSubscribe", "Sensors",
              "ServiceFramework", "SystemInfo", "Versit", "VersitOrganizer", "Mobility")]
_QTQINC = ["-I" + os.path.join(QT, "include")] + ["-I" + os.path.join(QT, "include", m) for m in (
    "QtCore", "QtGui", "QtNetwork", "QtXml", "QtSql", "QtSvg", "QtDeclarative", "QtScript")]


def _qtm(rel, deffile=None, **kw):
    d = dict(mmp=os.path.join(QTM, rel), flags=_MKSPECS + _QTQINC + _QTMINC, defs=_QTDEFS, vfp=True, allow_wsd=True,
             cdriver=True, rtti=True, x32_flags=_X32CXX)
    if deffile:
        d["def"] = os.path.join(QTM, "src", "s60installs", "eabi", deffile)
    d.update(kw)
    return d


_QTM12 = {   # key: (mmp, .def or None for a plug-in, ROM/installed file name)
    "qtbearer": ("src/bearer/QtBearer_dll.mmp", "QtBeareru.def", "QtBearer.dll"),
    "qtsensors": ("src/sensors/QtSensors_dll.mmp", "QtSensorsu.def", "QtSensors.dll"),
    "qtlocation": ("src/location/QtLocation_dll.mmp", "QtLocationu.def", "QtLocation.dll"),
    "qtversit": ("src/versit/QtVersit_dll.mmp", "QtVersitu.def", "QtVersit.dll"),
    "qtcontacts": ("src/contacts/QtContacts_dll.mmp", "QtContactsu.def", "QtContacts.dll"),
    "qtorganizer": ("src/organizer/QtOrganizer_dll.mmp", "QtOrganizeru.def", "QtOrganizer.dll"),
    "qtversitorganizer": ("src/versitorganizer/QtVersitOrganizer_dll.mmp", "QtVersitOrganizeru.def", "QtVersitOrganizer.dll"),
    "qtfeedback": ("src/feedback/QtFeedback_dll.mmp", "QtFeedbacku.def", "QtFeedback.dll"),
    "qtgallery": ("src/gallery/QtGallery_dll.mmp", "QtGalleryu.def", "QtGallery.dll"),
    "qtmessaging": ("src/messaging/QtMessaging_dll.mmp", "QtMessagingu.def", "QtMessaging.dll"),
    # plug-ins (no .def: nothing links against them)
    "qtsensors_generic": ("plugins/sensors/generic/qtsensors_generic_dll.mmp", None, "qtsensors_generic.dll"),
    "qtsensors_sym": ("plugins/sensors/symbian/qtsensors_sym_dll.mmp", None, "qtsensors_sym.dll"),
    "qtfeedback_symbian": ("plugins/feedback/symbian/qtfeedback_symbian_dll.mmp", None, "qtfeedback_symbian.dll"),
    "qtfeedback_mmk": ("plugins/feedback/mmk/qtfeedback_mmk_dll.mmp", None, "qtfeedback_mmk.dll"),
    "qtcontacts_symbian": ("plugins/contacts/symbian/plugin/qtcontacts_symbian_dll.mmp", None, "qtcontacts_symbian.dll"),
    "qtcontacts_serviceactionmanager": ("plugins/contacts/serviceactionmanager/qtcontacts_serviceactionmanager_dll.mmp", None,
                                        "qtcontacts_serviceactionmanager.dll"),
    "qtorganizer_symbian": ("plugins/organizer/symbian/qtorganizer_symbian_dll.mmp", None, "qtorganizer_symbian.dll"),
    "qtlandmarks_symbian": ("plugins/landmarks/symbian_landmarks/qtlandmarks_symbian_dll.mmp", None, "qtlandmarks_symbian.dll"),
    "qtgeoservices_nokia": ("plugins/geoservices/nokia/qtgeoservices_nokia_dll.mmp", None, "qtgeoservices_nokia.dll"),
    "qtversit_backuphandler": ("plugins/versit/backuphandler/qtversit_backuphandler_dll.mmp", None, "qtversit_backuphandler.dll"),
    "qtversit_vcardpreserver": ("plugins/versit/vcardpreserver/qtversit_vcardpreserver_dll.mmp", None, "qtversit_vcardpreserver.dll"),
}
_QTM12_S4 = {   # the S4 line's 1.0.3 entries have these keys; the 1.2 ones are only added without it
    "qtserviceframework": ("src/serviceframework/QtServiceFramework_dll.mmp", "QtServiceFrameworku.def", "QtServiceFramework.dll"),
    "qtpublishsubscribe": ("src/publishsubscribe/QtPublishSubscribe_dll.mmp", "QtPublishSubscribeu.def", "QtPublishSubscribe.dll"),
    "qtsysteminfo": ("src/systeminfo/QtSystemInfo_dll.mmp", "QtSystemInfou.def", "QtSystemInfo.dll"),
}
if os.environ.get("PHOREPOC_S4") != "1":
    _QTM12.update(_QTM12_S4)
for _k, (_m, _d, _r) in _QTM12.items():
    COMPONENTS[_k] = _qtm(_m, _d, rom=_r, sis=True)

# the QML import plug-ins (plugins/declarative; the firmware's declarative_*.dll). messaging/multimedia ride on qtmessaging / the 1.2 QtMultimediaKit.
for _n in ("contacts", "feedback", "gallery", "location", "organizer", "publishsubscribe", "sensors", "serviceframework", "systeminfo"):
    COMPONENTS["declarative_" + _n] = _qtm("plugins/declarative/%s/declarative_%s_dll.mmp" % (_n, _n), None,
                                           rom="declarative_%s.dll" % _n, sis=True,
                                           flags=_MKSPECS + _QTQINC + _QTMINC + ["-I" + os.path.join(QT, "include", "QtDeclarative")])
