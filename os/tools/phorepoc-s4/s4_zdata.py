# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""s4_zdata.py -- Symbian^4 (docs/phorepoc/symbian4-build.md): the Z: data of the S^4 platform packages besides Orbit
(hb_zdata.py) -- the files their bld.infs export to /epoc32/data/z, which the S^4 ROM build adds
(tools/phorepoc-x86/x32_rom_args.py reads every .phorepoc-s4-build/*-zdata.txt).

  py tools/phorepoc-s4/s4_zdata.py      -> .phorepoc-s4-build/s4-zdata.txt  (lines `<rom path>=<repo-relative source>`)
"""
import os
import sys

BUILD = os.environ["PHOREPOC_BUILD"]
S10 = os.path.join(os.environ["PHOREPOC_SYMBIAN"], "symbian-2010")
OS_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
REPO = ""   # (absolute paths below; the private tree used repo-relative ones)
S4R = os.path.join(BUILD, "qt48")
B = S10
USIF = f"{B}/oss.FCL.sf.mw.appinstall/installationservices/swiconfig/usif/scr"
HSRV = S4R + "/hsrv-src/contentstorage"

# (rom path, source): from the packages' bld.inf PRJ_EXPORTS (the /epoc32/data/z lines)
DATA = [
    # USIF's Software Component Registry: the provisioned (prebuilt) database the server copies to C: at first use,
    # and its settings (swiconfig/group/bld.inf)
    (r"sys\install\scr\provisioned\scr.db", f"{USIF}/scr.db"),
    (r"private\10202be9\10285bc0.txt", f"{USIF}/10285bc0.txt"),
    # the content arsenal's database (contentstorage/group/group.pro: the storage copies it to C: at first start)
    (r"private\20022f90\castorage.db", f"{HSRV}/castorage/data/castorage.db"),
    (r"private\20022f90\backup_registration.xml", f"{HSRV}/data/backup_registration.xml"),
]

# S4.6: the homescreen (oss.FCL.sf.app.homescreen: homescreenapp/rom/homescreenapp_core.iby + _resources.iby)
HSA = S4R + "/hs-src/homescreenapp"
WP = f"{HSA}/hsapplication/resource/wallpapers"
HS_DATA = [
    # the release's device database (installs_symbian.pri: resource_emulator/homescreen.db -> /private/20022f35):
    # the scene, 3 pages, the clock + shortcut widgets and their preferences
    (r"private\20022f35\homescreen.db", f"{HSA}/hsapplication/resource/resource_emulator/homescreen.db"),
    (r"private\20022f35\backup_registration.xml", f"{HSA}/hsapplication/data/backup_registration.xml"),
] + [(rf"private\20022f35\wallpapers\{k}\{o}_{p}.png", f"{WP}/{k}/{o}_{p}.png")
     for k, os_ in (("scene", ("default",)), ("page", ("default", "1", "2", "3"))) for o in os_
     for p in ("portrait", "landscape")] + [
    (rf"private\20022f35\wallpapers\originals\{n}.png", f"{WP}/page/originals/{n}.png") for n in ("car", "shapes", "shoes")
] + [
    # the widgets' registry entries (the content arsenal's widget scanner reads them)
    (r"private\20022f35\import\widgetregistry\20022F46\hsshortcutwidgetplugin.manifest",
     f"{HSA}/widgetplugins/hsshortcutwidgetplugin/resource/hsshortcutwidgetplugin.manifest"),
    (r"private\20022f35\import\widgetregistry\20022F6C\hsclockwidgetplugin.manifest",
     f"{HSA}/widgetplugins/hsclockwidgetplugin/resource/hsclockwidgetplugin.manifest"),
    (r"private\20022f35\import\widgetregistry\20022F6C\qtg_graf_hswidget_preview_clock.svg",
     f"{HSA}/widgetplugins/hsclockwidgetplugin/resource/qtg_graf_hswidget_preview_clock.svg"),
    (r"resource\qt\crml\hsapplibrary.qcrml", f"{HSA}/stateplugins/hsapplibrarystateplugin/conf/hsapplibrary.qcrml"),
    # (no Qt service framework database: Orbit's descriptors go to the database manager's import folder, service_descriptors.py)
]
# Qt plug-in stubs (qmake's qmakepluginstubs/<target>.qtplugin): QPluginLoader finds a Symbian plug-in DLL through
# the stub in \resource\qt\plugins\<subdir>
QTPLUGINS = [
    ("", f"{HSA}/hshomescreenclientplugin", "hshomescreenclientplugin"),
    ("", f"{HSA}/hsmenuclientplugin", "hsmenuclientplugin"),
    ("homescreen", f"{HSA}/stateplugins/hsapplibrarystateplugin", "hsapplibrarystateplugin"),
    ("homescreen", f"{HSA}/stateplugins/hshomescreenstateplugin", "hshomescreenstateplugin"),
    ("homescreen", f"{HSA}/stateplugins/hsmenuworkerstateplugin", "hsmenuworkerstateplugin"),
    ("homescreen", f"{HSA}/widgetplugins/hsclockwidgetplugin", "hsclockwidgetplugin"),
    ("homescreen", f"{HSA}/widgetplugins/hsshortcutwidgetplugin", "hsshortcutwidgetplugin"),
    ("commandhandler", f"{HSRV}/cahandler/app", "caapphandlerplugin"),
    ("commandhandler", f"{HSRV}/cahandler/tapp", "catapphandlerplugin"),
    ("commandhandler", f"{HSRV}/cahandler/url", "caurlhandlerplugin"),
    ("iconengines", f"{HSRV}/camificonengine", "camificon"),   # (board delta: an FBS session per thread, boot 1059)
]
HS_DATA += [("resource\\qt\\plugins\\" + (sub + "\\" if sub else "") + f"{n}.qtplugin", f"{d}/qmakepluginstubs/{n}.qtplugin")
            for sub, d, n in QTPLUGINS]

# S4.7: the task switcher + the activity framework (homescreensrv taskswitcher/rom/tstaskmonitor_core.iby,
# tsdevicedialog_core.iby, activityfw/rom/activitymanager_core.iby)
HSRVR = S4R + "/hsrv-src"
HS_DATA += [
    (r"resource\qt\plugins\activitytsplugin.qtplugin",
     f"{HSRVR}/taskswitcher/activitytsplugin/qmakepluginstubs/activitytsplugin.qtplugin"),
    (r"resource\taskswitcher\activitytsplugin.xml", f"{HSRVR}/taskswitcher/activitytsplugin/data/activitytsplugin.xml"),
    # (no stub for tstaskmonitorclient.dll: a linked DLL, not a Qt plug-in. qmake writes one anyway, but taskswitcher/rom/tstaskmonitor_core.iby
    # and sis/taskmonitor.pkg ship only the DLL and nothing calls QPluginLoader on it)
    # Orbit's device dialog server finds its plug-ins in \resource\plugins\devicedialogs
    (r"resource\plugins\devicedialogs\tsdevicedialogplugin.qtplugin",
     f"{HSRVR}/taskswitcher/tsdevicedialogplugin/qmakepluginstubs/tsdevicedialogplugin.qtplugin"),
    # ... and Hb's stock ones (registry/components_s4.py: message box, notification, progress, indicator menu)
] + [
    (rf"resource\plugins\devicedialogs\{n}.qtplugin",
     f"{S4R}/hb-symbian/src/hbplugins/devicedialogs/{d}/qmakepluginstubs/{n}.qtplugin")
    for d, n in (("devicemessageboxplugin", "HbDeviceMessageBoxPlugin"),
                 ("devicenotificationdialogplugin", "HbDeviceNotificationDialogPlugin"),
                 ("deviceprogressdialogplugin", "HbDeviceProgressDialogPlugin"),
                 ("indicatormenuplugin", "HbIndicatorMenuPlugin"))
] + [
    (r"resource\qt\crml\tsdevicedialog.qcrml", f"{HSRVR}/homescreensrv_plat/taskswitcher_api/tsdevicedialog.qcrml"),
    (r"resource\qt\plugins\afservice.qtplugin", f"{HSRVR}/activityfw/activityserviceplugin/qmakepluginstubs/afservice.qtplugin"),
    (r"resource\qt\plugins\hbactivityplugin.qtplugin",
     f"{HSRVR}/activityfw/orbitintegration/hbactivityplugin/qmakepluginstubs/hbactivityplugin.qtplugin"),
    (r"resource\activity\afservice.xml", f"{HSRVR}/activityfw/activityserviceplugin/data/afservice.xml"),
    # the repository hsapplicationlauncher needs (no definition in the release: a board file, key 0x7 = language set)
    (r"private\10202be9\2002b3aa.txt", os.path.join(OS_ROOT, "toolchain", "shims", "hs_board", "2002b3aa.txt")),
]
# the stand-alone widgets (hsw_configure_symbian.sh): what each package's rom/*.iby puts in the homescreen's widget registry
# folder -- the plug-in stub, the manifest, the icon and the service descriptor. The descriptor is the package's
# xml_for_sisx, the one Nokia made for an installed widget (its <filepath> names the stub on c:)
HSWR = S4R + "/hsw/app"
for _uid, _n in (("2002EAD2", "profileswidgetplugin"),):
    _dst = rf"private\20022f35\import\widgetregistry\{_uid}" + "\\"
    HS_DATA += [
        (_dst + f"{_n}.qtplugin", f"{HSWR}/{_n}/qmakepluginstubs/{_n}.qtplugin"),
        (_dst + f"{_n}.manifest", f"{HSWR}/{_n}/resource/{_n}.manifest"),
        (_dst + f"{_n}.png", f"{HSWR}/{_n}/resource/{_n}.png"),
        (_dst + f"{_n}.xml", f"{HSWR}/{_n}/resource/{_n}.xml_for_sisx"),
    ]


def main() -> int:
    out = os.path.join(REPO, S4R, "s4-zdata.txt")
    lines = []
    data = DATA + (HS_DATA if os.path.isdir(os.path.join(REPO, HSA)) else [])
    # the S^4 UI's English texts (tools/phorepoc-s4/build_translations.sh: the release's .ts through lrelease);
    # a QTranslator asked for <name>_en_GB falls back to <name>_en
    # (Qt 4.8 tree: the .qm files are Qt-version independent -> the 4.7 tree's when this one has none)
    qmr = S4R if os.path.isdir(os.path.join(REPO, S4R, "qm")) else ".phorepoc-s4-build"
    qm = os.path.join(REPO, qmr, "qm")
    if os.path.isdir(qm):
        data += [(rf"resource\qt\translations\{f}", f"{qmr}/qm/{f}")
                 for f in sorted(os.listdir(qm)) if f.endswith(".qm")]
    for i, (rom, src) in enumerate(data):
        # Qt 4.8's qmake does not write some generated data (qmakepluginstubs/*.qtplugin): the 4.7 tree's (same text)
        if S4R != ".phorepoc-s4-build" and src.startswith(S4R) and not os.path.isfile(os.path.join(REPO, src)):
            old = ".phorepoc-s4-build" + src[len(S4R):]
            if os.path.isfile(os.path.join(REPO, old)):
                src = old
                data[i] = (rom, src)
        if not os.path.isfile(os.path.join(REPO, src)):
            print(f"s4_zdata: missing {src}")
            return 1
        lines.append(f"{rom}={src}")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"s4_zdata: {len(lines)} ROM data files -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
