# The homescreen configurations (HSPS install packages)

At its first start HSPS, the homescreen's configuration server, installs the packages under
`Z:\private\200159c0\install\<configuration>\`: `hsps\00\` (the manifest and the widget/view configuration XML) and
`xuikon\00\<name>.o0000`, the configuration's Xuikon XML + CSS compiled into the binary ODT form. `manifest.txt` maps
each file to its ROM path; the ROM recipe stages them when this folder is present.

These files are **generated** from the Symbian^3 sources (`oss.FCL.sf.app.homescreen/idlehomescreen/data/qhd_tch/`,
the 360x640 touch set, language 00) with Nokia's ThemeInstaller (`oss.FCL.sf.app.homescreentools/themeinstaller`,
the XML2ODT compiler). Both are EPL-1.0, Copyright (c) Nokia Corporation and/or its subsidiary(-ies); so are these
outputs. They are kept here so a build needs no Java; the pdk gets its own compiler (a Go port of ThemeInstaller's
XML2ODT) to regenerate them when a configuration changes.
