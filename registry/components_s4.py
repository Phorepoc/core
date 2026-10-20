# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-1.0

"""components_s4 -- the Symbian^4 registry, merged into components.COMPONENTS only
when PHOREPOC_S4=1, so the S^3 build, closure and regression never see it.

The Qt 4.7 mmps are the ones qmake GENERATES (tools/phorepoc-s4/qt_configure_symbian.sh + qt_mocables.sh), copied
into the source module directory they were written for (their SOURCEPATHs are relative to it) by
tools/phorepoc-s4/qt_stage_mmps.sh. Builds: `PHOREPOC_S4=1 build_mmp_x32.py <key>` (./ph s4 build <key>) -> images
in .phorepoc-s4-build/x32img; every S^3 image is a provider as is.
"""
import os

from paths import OS_ROOT, SYMBIAN, BUILD, EPOC32_ROOT  # noqa: E402
QT48 = os.environ.get("PHOREPOC_QT48") == "1"
QTB = os.path.join(BUILD, "qt48") if QT48 else os.path.join(BUILD, "s4")
QT = os.path.join(QTB, "qt-src")


# the generated mmps name /epoc32/mkspecs/... (where a Qt INSTALL put the mkspecs): the source tree's are the same
# files -- qplatformdefs.h, and stl-off/new (the <new> of a build without STL)
_MKSPECS = ["-I" + os.path.join(QT, "mkspecs", "common", "symbian", "stl-off"),
            "-I" + os.path.join(QT, "mkspecs", "common", "symbian"),
            # (4.8: the mkspec's header-wrappers, as components_qt48)
            "-I" + os.path.join(QT, "mkspecs", "common", "symbian", "header-wrappers"),
            # configure's output (qconfig.h/.cpp) sits in the shadow tree
            "-I" + os.path.join(QTB, "qt-symbian", "src", "corelib", "global"),
            # gcc's freestanding C++ headers (<exception>, <typeinfo>, <new>), AFTER everything of ours (container paths)
            "-idirafter", "/usr/include/c++/13", "-idirafter", "/usr/include/x86_64-linux-gnu/c++/13/x32",
            "-idirafter", os.path.join(OS_ROOT, "toolchain", "shims", "qt_cxxstd")]


def _qt(module: str, mmp: str, flags=(), **kw) -> dict:
    """A Qt module: its generated mmp (staged into the source dir) and its frozen EABI .def."""
    return dict(mmp=os.path.join(QT, "src", module, mmp), flags=_MKSPECS + list(flags), **kw)


# Qt on this board: no ARMv6 atomics (qatomic_symbian.cpp -> __e32_atomic_*, SYMBIAN_E32_ATOMIC_API), no C++
# exceptions (QT_NO_EXCEPTIONS; the Symbian leave/throw bridges are the open question, symbian4-build.md s.6)
_QTDEFS = ["SYMBIAN_E32_ATOMIC_API", "QT_NO_EXCEPTIONS", "__declspec(x)="]
# force-included after the variant .hrh: the S^4-only WSERV features our S^3 WSERV lacks (s4_undefs.h)
_S4UNDEFS = ["-include", os.path.join(OS_ROOT, "toolchain", "shims", "qt_cxxstd", "s4_undefs.h")]

HB = os.path.join(QTB, "hb-src")
_QTINC = os.path.join(QTB, "qt-symbian", "include")
# Orbit's mmps name Qt's INSTALLED headers (/epoc32/include/QtCore...): ours are the Qt build tree's forwarding headers
_HBFLAGS = _MKSPECS + ["-I" + _QTINC] + ["-I" + os.path.join(_QTINC, m) for m in
                                        ("QtCore", "QtGui", "QtNetwork", "QtSvg", "QtXml", "QtSql")]


def _hb(sub: str, mmp: str, deffile=None, **kw) -> dict:
    """An Orbit (hb) component: its qmake-generated mmp, staged into hb-src/src/<sub> (hb_configure_symbian.sh)."""
    d = dict(mmp=os.path.join(HB, "src", sub, mmp), flags=_HBFLAGS, defs=_QTDEFS, allow_wsd=True, cdriver=True,
             rtti=True, x32_flags=_S4UNDEFS, vfp=True)	# vfp: FPU code on ARM_VFP=1 (softfp = the same call ABI as Qt 4.8's)
    if deffile:
        d["def"] = os.path.join(HB, "src", deffile)
    d.update(kw)
    return d


# Orbit's public headers (the wrapper tree hb_configure_symbian.sh writes), for Orbit's clients
_HBINC = ["-I" + os.path.join(QTB, "hb-symbian", "include", d)
          for d in ("hbcore", "hbwidgets", "hbutils", "hbinput", "hbfeedback")]

# apart from the S^3 .epoc32), after the component's own include paths
_S4INC = os.path.join(QTB, "epoc32", "include")
_S4EXPORTS = ["-idirafter", _S4INC, "-idirafter", os.path.join(_S4INC, "mw"),
              "-idirafter", os.path.join(_S4INC, "platform", "mw"), "-idirafter", os.path.join(_S4INC, "platform")]

QTM = os.path.join(QTB, "qtm-src")
HSRV = os.path.join(QTB, "hsrv-src")
# Qt Mobility's module headers for its clients (CONFIG += mobility: <QServiceManager>, <qservicemanager.h>)
_QTMINC = ["-I" + os.path.join(QTM, "include", "Qt" + m)
           for m in ("ServiceFramework", "PublishSubscribe", "SystemInfo", "Mobility")]
QTE = os.path.join(QTB, "qte-src")
HS = os.path.join(QTB, "hs-src")
# the homescreen: Orbit + Qt Mobility clients, over the S^4 exports
_HSFLAGS = _HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS


_STS = "oss.FCL.sf.mw.mmmw/mmserv/sts/"
# imported on its own by the ROM tools)
_STLPORT_X32 = ["toolchain/shims/stlport_board"]
_STLPORT_X32_FLAGS = ["-include", "symcpp.h"]


def _qx(root: str, mmp: str, deffile=None, **kw) -> dict:
    """A Qt Mobility / Qt extensions component: its in-source qmake-generated mmp
    (tools/phorepoc-s4/qtm_configure_symbian.sh, qte_configure_symbian.sh) and its frozen EABI .def."""
    d = dict(mmp=os.path.join(root, mmp), flags=_HBFLAGS + _S4EXPORTS, defs=_QTDEFS, allow_wsd=True, cdriver=True,
             rtti=True, x32_flags=_S4UNDEFS)
    if deffile:
        d["def"] = os.path.join(root, deffile)
    d.update(kw)
    return d


COMPONENTS = {
    "qtserviceframework": _qx(QTM, "src/serviceframework/QtServiceFramework_dll.mmp",
                              "src/s60installs/eabi/QtServiceFrameworku.def", rom="QtServiceFramework.dll"),
    # board delta: refresh the C: database when the ROM one is newer (toolchain/shims/qtm_board)
    "qsfwdatabasemanagerserver": _qx(QTM, "src/serviceframework/databasemanagerserver_symbian/qsfwdatabasemanagerserver_exe.mmp",
                                     drop=["databasemanagersession.cpp"], extra=["toolchain/shims/qtm_board/databasemanagersession.cpp"]),
    "qtpublishsubscribe": _qx(QTM, "src/publishsubscribe/QtPublishSubscribe_dll.mmp",
                              "src/s60installs/eabi/QtPublishSubscribeu.def", rom="QtPublishSubscribe.dll"),
    "qpspathmapperserver": _qx(QTM, "src/publishsubscribe/psmapperserver_symbian/qpspathmapperserver_exe.mmp"),
    # (s60_HbKeymap: Orbit's input keymaps, HbCore)
    # board delta: no WLAN engine on this board -> CWlanInfo without a management client does not poll through it
    "qtsysteminfo": _qx(QTM, "src/systeminfo/QtSystemInfo_dll.mmp", "src/s60installs/eabi/QtSystemInfou.def",
                        rom="QtSystemInfo.dll", flags=_HBFLAGS + _HBINC + _S4EXPORTS,
                        drop=["wlaninfo_s60.cpp"], extra=["toolchain/shims/qtsysteminfo_board/wlaninfo_s60.cpp"],
                        defs=_QTDEFS + ["PHOREPOC_NO_WLAN"]),
    # the native ones are the release's own mmps under the same export
    "cautils": _qx(HSRV, "contentstorage/cautils/cautils_dll.mmp", "contentstorage/eabi/cautilsu.def", rom="cautils.dll",
                   flags=_HBFLAGS + _HBINC + _S4EXPORTS),
    "castorage": _qx(HSRV, "contentstorage/castorage/group/castorage.mmp", "contentstorage/castorage/eabi/castorageu.def",
                     rom="castorage.dll"),
    "casrvmanager": _qx(HSRV, "contentstorage/casrv/casrvmgr/group/casrvmanager.mmp",
                        "contentstorage/casrv/casrvmgr/eabi/casrvmanageru.def", rom="casrvmanager.dll"),
    "camenu": _qx(HSRV, "contentstorage/group/camenu.mmp", "contentstorage/eabi/camenuu.def", rom="camenu.dll",
                  flags=_HBFLAGS + _HBINC + _S4EXPORTS),
    "camenuserver": _qx(HSRV, "contentstorage/group/camenusrv.mmp"),
    # board delta: S^3's AppArc has no per-app update list -> the package's own earlier full rescan on each notification
    "caappscanner": _qx(HSRV, "contentstorage/casrv/caappscanner/group/caappscanner.mmp", rom="caappscanner.dll",
                        drop=["casrvappscanner.cpp"], extra=["toolchain/shims/caappscanner_board/casrvappscanner.cpp"],
                        defs=_QTDEFS + ["PHOREPOC_S3_APPARC"]),
    "caclient": _qx(HSRV, "contentstorage/caclient/caclient_dll.mmp", "contentstorage/caclient/eabi/caclientu.def",
                    rom="caclient.dll", flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS),
    # (launch an app / a URL / a Qt Highway app: Qt service-framework plug-ins) and the widget scanner (the HS widgets)
    "casoftwareregistry": _qx(HSRV, "contentstorage/casoftwareregistry/casoftwareregistry_dll.mmp",
                              "contentstorage/casoftwareregistry/eabi/casoftwareregistryu.def",
                              rom="casoftwareregistry.dll", flags=_HBFLAGS + _HBINC + _S4EXPORTS),
    "caapphandlerplugin": _qx(HSRV, "contentstorage/cahandler/app/caapphandlerplugin_dll.mmp",
                              "contentstorage/cahandler/app/plugin_commonu.def", rom="caapphandlerplugin.dll",
                              flags=_HBFLAGS + _QTMINC + _S4EXPORTS),
    # the Qt icon engine for .mif / .mbm application icons (homescreensrv_core.iby) -- without it every S^3 app had no
    "camificon": _qx(HSRV, "contentstorage/camificonengine/camificon_dll.mmp",
                     "contentstorage/camificonengine/plugin_commonu.def", rom="camificon.dll",
                     drop=["camificonengine.cpp"], extra=["toolchain/shims/hsrv_board/camificonengine.cpp"],   # per-thread FBS session
                     libs=["fbscli.lib"],
                     flags=_HBFLAGS + _QTMINC + _S4EXPORTS),
    "caurlhandlerplugin": _qx(HSRV, "contentstorage/cahandler/url/caurlhandlerplugin_dll.mmp",
                              "contentstorage/cahandler/url/plugin_commonu.def", rom="caurlhandlerplugin.dll",
                              flags=_HBFLAGS + _QTMINC + _S4EXPORTS),
    "catapphandlerplugin": _qx(HSRV, "contentstorage/cahandler/tapp/catapphandlerplugin_dll.mmp",
                               "contentstorage/cahandler/tapp/plugin_commonu.def", rom="catapphandlerplugin.dll",
                               flags=_HBFLAGS + _QTMINC + _S4EXPORTS),
    "cawidgetscanner": _qx(HSRV, "contentstorage/casrv/cawidgetscanner/group/cawidgetscanner.mmp",
                           rom="cawidgetscanner.dll", flags=_HBFLAGS + _HBINC + _S4EXPORTS),
    # monitor server + its client, the screenshot plug-in, the Orbit device dialog), the activity framework (the
    # storage server + client, the Qt service plug-in, the launcher, Orbit's activity plug-in), the app-key handler
    # (TsTaskMonitor::changeList returns QList<QPair<TsTaskChangeInfo, ..>> with TsTaskChangeInfo only declared: RVCT
    # instantiated late, GCC needs the complete type -> its header force-included)
    "tstaskmonitorclient": _qx(HSRV, "taskswitcher/client/tstaskmonitorclient_dll.mmp",
                               "taskswitcher/client/eabi/tstaskmonitorclientu.def", rom="tstaskmonitorclient.dll",
                               flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS,
                               x32_flags=_S4UNDEFS + ["-include", os.path.join(
                                   HSRV, "taskswitcher", "client", "inc", "tstaskchangeinfo.h")]),
    "hsrunningappmonitor": _qx(HSRV, "taskswitcher/server/hsrunningappmonitor_exe.mmp",
                               flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS),
    # (an unused #include <stdexcept>: the host libstdc++ header needs a hosted C++ library -> its guard predefined)
    "activitytsplugin": _qx(HSRV, "taskswitcher/activitytsplugin/activitytsplugin_dll.mmp",
                            "taskswitcher/activitytsplugin/plugin_commonu.def", rom="activitytsplugin.dll",
                            flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS, x32_flags=_S4UNDEFS + ["-D_GLIBCXX_STDEXCEPT"]),
    "tsscreenshotplugin": _qx(HSRV, "taskswitcher/screenshotplugin/group/tsscreenshotplugin.mmp",
                              rom="tsscreenshotplugin.dll"),
    "tsdevicedialogplugin": _qx(HSRV, "taskswitcher/tsdevicedialogplugin/tsdevicedialogplugin_dll.mmp",
                                "taskswitcher/tsdevicedialogplugin/plugin_commonu.def", rom="tsdevicedialogplugin.dll",
                                flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS),
    "afstoragecommon": _qx(HSRV, "activityfw/storage/common/group/common.mmp"),
    "afstorageclient": _qx(HSRV, "activityfw/storage/client/group/client.mmp",
                           "activityfw/storage/client/eabi/afstorageclientu.def", rom="afstorageclient.dll"),
    # board delta: S^3's AppArc has no per-app update list -> the removed apps from two full scans (as caappscanner)
    "hsactivitydbserver": _qx(HSRV, "activityfw/storage/server/group/server.mmp",
                              drop=["afapplicationscollection.cpp"],
                              extra=["toolchain/shims/afstorage_board/afapplicationscollection.cpp"],
                              defs=_QTDEFS + ["PHOREPOC_S3_APPARC"]),
    "afactivitylauncher": _qx(HSRV, "activityfw/afactivitylauncher/group/afactivitylauncher.mmp",
                              "activityfw/afactivitylauncher/eabi/afactivitylauncheru.def", rom="afactivitylauncher.dll"),
    "afservice": _qx(HSRV, "activityfw/activityserviceplugin/afservice_dll.mmp",
                     "activityfw/activityserviceplugin/eabi/afserviceu.def", rom="afservice.dll",
                     flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS),
    "hbactivityplugin": _qx(HSRV, "activityfw/orbitintegration/hbactivityplugin/hbactivityplugin_dll.mmp",
                            "activityfw/orbitintegration/hbactivityplugin/plugin_commonu.def", rom="hbactivityplugin.dll",
                            flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS),
    "hsappkeyplugin": _qx(HSRV, "hsappkeyhandler/group/hsappkeyplugin.mmp", rom="hsappkeyplugin.dll",
                          drop=["hsappkeyplugin.cpp"], extra=["toolchain/shims/hsrv_board/hsappkeyplugin.cpp"],
                          libs=["centralrepository.lib", "efsrv.lib"]),
    # (hsimagehandlingclient.cpp uses the photo editor's crop mode outside its FF_IMAGE_EDITOR guard; the release has
    # no PhotoEditor service, so the request is never created and the value never sent: a placeholder)
    "hsutils": _qx(HS, "homescreenapp/hsutils/hsutils_dll.mmp", "homescreenapp/hsutils/eabi/hsutilsu.def",
                   rom="hsutils.dll", flags=_HSFLAGS, x32_flags=_S4UNDEFS + ["-DEEditorHighwayWallpaperCrop=1"]),
    "hsdomainmodel": _qx(HS, "homescreenapp/hsdomainmodel/hsdomainmodel_dll.mmp",
                         "homescreenapp/hsdomainmodel/eabi/hsdomainmodelu.def", rom="hsdomainmodel.dll", flags=_HSFLAGS),
    "hsmenuserviceprovider": _qx(HS, "homescreenapp/serviceproviders/hsmenuserviceprovider/hsmenuserviceprovider_dll.mmp",
                                 "homescreenapp/serviceproviders/hsmenuserviceprovider/eabi/hsmenuserviceprovideru.def",
                                 rom="hsmenuserviceprovider.dll", flags=_HSFLAGS),
    "hsmenuclientplugin": _qx(HS, "homescreenapp/hsmenuclientplugin/hsmenuclientplugin_dll.mmp",
                              "homescreenapp/hsmenuclientplugin/plugin_commonu.def", rom="hsmenuclientplugin.dll",
                              flags=_HSFLAGS),
    "hshomescreenclientplugin": _qx(HS, "homescreenapp/hshomescreenclientplugin/hshomescreenclientplugin_dll.mmp",
                                    "homescreenapp/hshomescreenclientplugin/plugin_commonu.def",
                                    rom="hshomescreenclientplugin.dll", flags=_HSFLAGS),
    "hshomescreenstateplugin": _qx(HS, "homescreenapp/stateplugins/hshomescreenstateplugin/hshomescreenstateplugin_dll.mmp",
                                   "homescreenapp/stateplugins/hshomescreenstateplugin/plugin_commonu.def",
                                   rom="hshomescreenstateplugin.dll", flags=_HSFLAGS),
    "hsapplibrarystateplugin": _qx(HS, "homescreenapp/stateplugins/hsapplibrarystateplugin/hsapplibrarystateplugin_dll.mmp",
                                   "homescreenapp/stateplugins/hsapplibrarystateplugin/plugin_commonu.def",
                                   rom="hsapplibrarystateplugin.dll", flags=_HSFLAGS),
    "hsmenuworkerstateplugin": _qx(HS, "homescreenapp/stateplugins/hsmenuworkerstateplugin/hsmenuworkerstateplugin_dll.mmp",
                                   "homescreenapp/stateplugins/hsmenuworkerstateplugin/plugin_commonu.def",
                                   rom="hsmenuworkerstateplugin.dll", flags=_HSFLAGS),
    # the clock type key: the organizer revision the package was written against (201037's clock_settings_keys_api;
    # the release's later organizer dropped it, S^3's header has TUid keys) -> toolchain/shims/hs_board, force-included (same guard)
    "hsclockwidgetplugin": _qx(HS, "homescreenapp/widgetplugins/hsclockwidgetplugin/hsclockwidgetplugin_dll.mmp",
                               "homescreenapp/widgetplugins/hsclockwidgetplugin/plugin_commonu.def",
                               rom="hsclockwidgetplugin.dll",
                               flags=_HSFLAGS, x32_flags=_S4UNDEFS + ["-include", os.path.join(
                                   OS_ROOT, "toolchain", "shims", "hs_board", "clockdomaincrkeys.h")]),
    "hsshortcutwidgetplugin": _qx(HS, "homescreenapp/widgetplugins/hsshortcutwidgetplugin/hsshortcutwidgetplugin_dll.mmp",
                                  "homescreenapp/widgetplugins/hsshortcutwidgetplugin/plugin_commonu.def",
                                  rom="hsshortcutwidgetplugin.dll", flags=_HSFLAGS),
    "hswidgetuninstaller": _qx(HS, "homescreenapp/hswidgetuninstaller/hswidgetuninstaller_exe.mmp", flags=_HSFLAGS),
    # (its registration declares Qt Highway services: xqserviceipcconst.h, an S^4 export)
    "hsapplication": _qx(HS, "homescreenapp/hsapplication/hsapplication_exe.mmp", flags=_HSFLAGS,
                         rsc_incs=[os.path.relpath(os.path.join(QTB, "epoc32", "include", "mw"), OS_ROOT).replace("\\", "/")]),
    # the UI-services list starts Orbit's theme + device dialog servers, the security-check list hsapplicationlauncher
    # (the idle app), the SWP lists the running-app monitor. Built into emitter/rsc-s4 (same names as the S^3 lists)
    # board deltas: the security-check list starts clockserver the S^3 way (fire-and-forget: it never rendezvous)
    "ssmcmdlists_s4": dict(mmp="oss.FCL.sf.os.devicesrv/sysstatemgmt/ssmcmdlists/group/ssmcmdlists.mmp", rsc_only=True,
                           ssm=True, rsc_out="rsc-s4",
                           rss_delta={"securitycheckcmdlist.rss": "tools/phorepoc-x86/board/ssm/s4/securitycheckcmdlist.rss",
                                      "noncriticalcmdlist_hw.rss": "tools/phorepoc-x86/board/ssm/s4/noncriticalcmdlist_hw.rss"}),
    # what the S^4 boot starts as its idle app (master's security-check list): keeps the homescreen running
    "hsapplicationlauncher": _qx(HS, "homescreenapp/hsapplicationlauncher/group/hsapplicationlauncher.mmp"),
    # Orbit's device dialog server (notes, the indicator menu, the homescreen's dialogs)
    "hbdevicedialogappserver": _hb("hbservers/hbdevicedialogappserver", "hbdevicedialogappserver_exe.mmp"),
    # the Symbian^4 system tone service (mw.mmmw mmserv/sts; xqsystemtoneservice's CSystemToneService -- a link stub
    # until now): the client DLL (+ its proxy lib) and the server, whose player map is std::map -> Open C++
    "stsproxy": dict(mmp=_STS + "stsproxy/group/stsproxy.mmp", cdriver=True, incs_first=_STLPORT_X32,
                     x32_flags=_STLPORT_X32_FLAGS),
    "systemtoneservice": dict(mmp=_STS + "group/systemtoneservice.mmp",
                              **{"def": os.path.join(SYMBIAN, "symbian-2010", _STS, "eabi", "systemtoneservice.def")}),
    "sts": dict(mmp=_STS + "sts/group/sts.mmp", cdriver=True, incs_first=_STLPORT_X32, x32_flags=_STLPORT_X32_FLAGS),
    # (key: "stsserver" is USIF's software transaction service -- same short name, another server)
    "systemtoneserver": dict(mmp=_STS + "stsserver/group/stsserver.mmp", rom="stsserver.exe", cdriver=True,
                             incs_first=_STLPORT_X32, x32_flags=_STLPORT_X32_FLAGS),
    "calist": dict(mmp=os.path.join(OS_ROOT, "src", "s4", "calist", "calist.mmp"), defs=_QTDEFS, rtti=True, cdriver=True,
                   flags=_HBFLAGS + _HBINC + _QTMINC + _S4EXPORTS),
    # whether it is removable) + its transaction service and the SIF notifier. The release's 2010 tree (symbian-rcl3's
    "sqlite3_secure": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swcomponentregistry/sqlite_secure/group/sqlite3_secure.mmp",
                           cdriver=True, allow_wsd=True, defs=["__declspec(x)="]),
    "securityutils": dict(mmp="oss.FCL.sf.os.security/cryptomgmtlibs/securitycommonutils/group/securityutils.mmp"),
    "stsclient": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swtransactionservices/group/stsclient.mmp"),
    "stsserver": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swtransactionservices/group/stsserver.mmp",
                      rom="softwaretransactionservices.exe"),
    "scrdatabase": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swcomponentregistry/group/scrdatabase.mmp",
                        cdriver=True),
    "scrclient": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swcomponentregistry/group/scrclient.mmp"),
    "scrhelperclient": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swcomponentregistry/scrhelper/group/scrhelperclient.mmp"),
    "scrhelperserver": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swcomponentregistry/scrhelper/group/scrhelperserver.mmp"),
    "scrserver": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swcomponentregistry/group/scrserver.mmp",
                      cdriver=True),
    "siftransport": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swinstallationfw/group/siftransport.mmp"),
    "sif": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swinstallationfw/group/sif.mmp"),
    "sifnotification": dict(mmp="oss.FCL.sf.mw.appinstall/installationservices/swinstallationfw/group/sifnotification.mmp"),
    "xqserviceutil": _qx(QTE, "qthighway/xqserviceutil/xqserviceutil_dll.mmp", "qthighway/eabi/xqserviceutilu.def",
                         rom="xqserviceutil.dll"),
    "xqserviceipcclient": _qx(QTE, "qthighway/xqserviceipc/xqserviceipc/xqserviceipcclient_dll.mmp",
                              "qthighway/xqserviceipc/eabi/xqserviceipcclientu.def", rom="xqserviceipcclient.dll"),
    "xqserviceipcserver": _qx(QTE, "qthighway/xqserviceipc/xqserviceipcserver/xqserviceipcserver_dll.mmp",
                              "qthighway/xqserviceipc/eabi/xqserviceipcserveru.def", rom="xqserviceipcserver.dll"),
    "xqservice": _qx(QTE, "qthighway/xqservice/xqservice_dll.mmp", "qthighway/eabi/xqserviceu.def", rom="xqservice.dll"),
    "xqsettingsmanager": _qx(QTE, "qtmobileextensions/src/settingsmanager/xqsettingsmanager_dll.mmp",
                             "qtmobileextensions/src/eabi/xqsettingsmanageru.def", rom="xqsettingsmanager.dll"),
    "xqsysinfo": _qx(QTE, "qtmobileextensions/src/sysinfo/xqsysinfo_dll.mmp", "qtmobileextensions/src/eabi/xqsysinfou.def",
                     rom="xqsysinfo.dll"),
    "xqutils": _qx(QTE, "qtmobileextensions/src/utils/xqutils_dll.mmp", "qtmobileextensions/src/eabi/xqutilsu.def",
                   rom="xqutils.dll"),
    "xqkeycapture": _qx(QTE, "qtmobileextensions/src/keycapture/xqkeycapture_dll.mmp",
                        "qtmobileextensions/src/eabi/xqkeycaptureu.def", rom="xqkeycapture.dll"),
    "xqsystemtoneservice": _qx(QTE, "qtmobileextensions/src/systemtoneservice/xqsystemtoneservice_dll.mmp",
                               "qtmobileextensions/src/eabi/xqsystemtoneserviceu.def", rom="xqsystemtoneservice.dll"),
    "hbcore": _hb("hbcore", "HbCore_dll.mmp", "hbcore/defs/eabi/HbCoreu.def", rom="HbCore.dll"),
    "hbwidgets": _hb("hbwidgets", "HbWidgets_dll.mmp", "hbwidgets/defs/eabi/HbWidgetsu.def", rom="HbWidgets.dll"),
    "hbutils": _hb("hbutils", "HbUtils_dll.mmp", "hbutils/defs/eabi/HbUtilsu.def", rom="HbUtils.dll"),
    "hbinput": _hb("hbinput", "HbInput_dll.mmp", "hbinput/defs/eabi/HbInputu.def", rom="HbInput.dll"),
    # the S^4 touch-feedback types our S^3 touchlogicalfeedback.h lacks: the nearest S^3 ones (hb_s3feedback.h)
    "hbfeedback": _hb("hbfeedback", "HbFeedback_dll.mmp", "hbfeedback/defs/eabi/HbFeedbacku.def", rom="HbFeedback.dll",
                      x32_flags=_S4UNDEFS + ["-include", os.path.join(OS_ROOT, "toolchain", "shims", "qt_cxxstd",
                                                                      "hb_s3feedback.h")]),
    # hbthemeserverutils.cpp calls ceil() with no <math.h> (RVCT's builtin)
    "hbthemeserver": _hb("hbservers/hbthemeserver", "hbthemeserver_exe.mmp",
                         # board delta: no QtSingleApplication "already running" exit (toolchain/shims/hb_board)
                         drop=["main.cpp"], extra=["toolchain/shims/hb_board/hbthemeserver_main.cpp"],
                         x32_flags=_S4UNDEFS + ["-Dceil(x)=__builtin_ceil(x)"]),
    "hbhello": dict(mmp=os.path.join(OS_ROOT, "src", "s4", "hbhello", "hbhello.mmp"), defs=_QTDEFS, rtti=True,
                    cdriver=True, flags=_HBFLAGS + _HBINC),
    "hbbincssmaker": dict(mmp=os.path.join(OS_ROOT, "src", "s4", "hbbincssmaker", "hbbincssmaker_symbian.mmp"),
                          defs=_QTDEFS, rtti=True, cdriver=True, allow_wsd=True, flags=_HBFLAGS,
                          x32_flags=_S4UNDEFS),
    "qtcore": _qt("corelib", "QtCore_dll.mmp", rom="QtCore.dll", **{"def": os.path.join(QT, "src/s60installs/eabi/QtCoreu.def")},
                  drop=["qatomic_generic_armv6.cpp"], defs=_QTDEFS, allow_wsd=True, cdriver=True, rtti=True),
    "qtgui": _qt("gui", "QtGui_dll.mmp", rom="QtGui.dll", **{"def": os.path.join(QT, "src/s60installs/eabi/QtGuiu.def")},
                 defs=_QTDEFS, allow_wsd=True, cdriver=True, rtti=True,
                 # (QFileDialog was left out while the host uic misnamed widgets: the Q_FOREACH bug, qt-patches/0002)
                 x32_flags=_S4UNDEFS),
    "qtsvg": _qt("svg", "QtSvg_dll.mmp", rom="QtSvg.dll", **{"def": os.path.join(QT, "src/s60installs/eabi/QtSvgu.def")},
                 defs=_QTDEFS, allow_wsd=True, cdriver=True, rtti=True, x32_flags=_S4UNDEFS),
    "qtnetwork": _qt("network", "QtNetwork_dll.mmp", rom="QtNetwork.dll",
                     **{"def": os.path.join(QT, "src/s60installs/eabi/QtNetworku.def")},
                     defs=_QTDEFS, allow_wsd=True, cdriver=True, rtti=True, x32_flags=_S4UNDEFS),
    "qtsql": _qt("sql", "QtSql_dll.mmp", rom="QtSql.dll", **{"def": os.path.join(QT, "src/s60installs/eabi/QtSqlu.def")},
                 defs=_QTDEFS, allow_wsd=True, cdriver=True, rtti=True, x32_flags=_S4UNDEFS),
    "qtxml": _qt("xml", "QtXml_dll.mmp", rom="QtXml.dll", **{"def": os.path.join(QT, "src/s60installs/eabi/QtXmlu.def")},
                 defs=_QTDEFS, allow_wsd=True, cdriver=True, rtti=True, x32_flags=_S4UNDEFS),
    # the Open C SQLite library QtSql's built-in driver links (the release's own; S^3 did not need it)
    "sqlite3": dict(mmp="oss.FCL.sf.os.persistentdata/persistentstorage/sqlite3api/GROUP/sqlite3.mmp",
                    cdriver=True, allow_wsd=True, defs=["__declspec(x)="]),
    # the per-platform-version helpers QtCore loads by name (qt_resolveS60PluginFunc): the 5.0 one
    "qts60plugin_5_0": _qt("plugins/s60/5_0", "qts60plugin_5_0_dll.mmp", rom="qts60plugin_5_0.dll",
                           **{"def": os.path.join(QT, "src/plugins/s60/eabi/qts60pluginu.def")},
                           defs=_QTDEFS, allow_wsd=True, cdriver=True, rtti=True),
    # Qt's E32Main -> __crt0 -> main() glue (a static library every Qt program links)
    "qtmain": _qt("s60main", "qtmain_lib.mmp", defs=_QTDEFS),
    "qtguihello": dict(mmp=os.path.join(OS_ROOT, "src", "s4", "qtguihello", "qtguihello.mmp"), defs=_QTDEFS, rtti=True,
                       cdriver=True, flags=_MKSPECS + [
                           "-I" + os.path.join(QTB, "qt-symbian", "include"),
                           "-I" + os.path.join(QTB, "qt-symbian", "include", "QtCore"),
                           "-I" + os.path.join(QTB, "qt-symbian", "include", "QtGui")]),
    "qthello": dict(mmp=os.path.join(OS_ROOT, "src", "s4", "qthello", "qthello.mmp"), defs=_QTDEFS, rtti=True, cdriver=True,
                    flags=_MKSPECS + ["-I" + os.path.join(QTB, "qt-symbian", "include"),
                                      "-I" + os.path.join(QTB, "qt-symbian", "include", "QtCore")]),
}

# PHOREPOC_QT48=1 = the S^4 set for BOTH arches: most x32_flags are S^3-target shims, not x32 ones (s4_undefs.h, hb_s3feedback.h,
# ceil, force-includes) -> the ARM builder (which reads only flags) gets them too; container-only paths and x86 code-model
# switches stay x32-only. (On x32 a flag in both lists is harmless: same -D, guarded -include.)
if QT48:
    def _portable(fl):
        out, i = [], 0
        while i < len(fl):
            f = fl[i]
            if f in ("-idirafter", "-isystem") and i + 1 < len(fl) and fl[i + 1].startswith("/usr/"):
                i += 2
                continue
            if f in ("-fpie", "-fPIE") or f.startswith("/usr/"):
                i += 1
                continue
            out.append(f)
            i += 1
        return out
    for _c in COMPONENTS.values():
        # <cstdlib> -> Open C + std:: names (toolchain/shims/hb_cstd), FIRST: libstdc++'s wrapper reaches newlib on ARM
        _c["flags"] = ["-I" + os.path.join(OS_ROOT, "toolchain", "shims", "hb_cstd")] + list(_c.get("flags", []))
        _c["arm_incs_first"] = True
        _x = _portable(list(_c.get("x32_flags", [])))
        # drop a -include already in flags (the x32 builder adds x32_flags again: symcpp.h twice redefined std::nothrow_t)
        _have = {_c["flags"][j + 1] for j in range(len(_c["flags"]) - 1) if _c["flags"][j] == "-include"}
        _y, j = [], 0
        while j < len(_x):
            if _x[j] == "-include" and j + 1 < len(_x) and _x[j + 1] in _have:
                j += 2
                continue
            _y.append(_x[j]); j += 1
        _x = _y
        if _x:
            _c["flags"] = list(_c.get("flags", [])) + _x
    # Qt itself comes from components_qt48 (qt48_* keys, one Qt in the ROM): drop this registry's Qt 4.7 module entries
    # (their mmps resolve into the 4.8 tree, where the 4.7-only ones -- qts60plugin_5_0 ... -- do not exist)
    for _k in [k for k, c in COMPONENTS.items() if os.path.normpath(c.get("mmp", "")).startswith(os.path.normpath(QT) + os.sep)]:
        del COMPONENTS[_k]
