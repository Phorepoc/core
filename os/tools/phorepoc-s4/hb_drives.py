# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""hb_drives.py -- Orbit's typeface metrics files are found on any drive, not only on z:.

Orbit (hbcore/utils/hbtypefacexmlparser_p.h) hard-codes TYPEFACE_RESOURCE_FOLDER "z:\\resource\\fonts": in S^4 they were
ROM files. A Catalogue package cannot write to z:, and an image carries no Orbit data, so the files sit in \\resource\\fonts of
the drive Orbit was installed on. This marked change makes the readers look for the file on every drive, z: last, the same
all-drives lookup Qt 4.8 for Symbian uses for its plug-in stubs (QPluginLoader::setFileName: QDir::drives()).
It rewrites the EXPORTED copy (<hb-src>/src; the baseline tree is never written), idempotently:

    python3 hb_drives.py <hb-src>/src
"""
import os
import sys

MARK = "PHOREPOC (drives)"

HELPER = """
#ifdef Q_OS_SYMBIAN
// PHOREPOC (drives): the file on whichever drive carries it (z: last): Orbit installs from a package, not from the ROM
inline QString hbTypefaceResourceFile(const QString &aName)
{
    QFileInfoList drives = QDir::drives();
    for (int i = 0; i < drives.count(); ++i) {
        QString path = drives.at(i).absolutePath() + QLatin1String("resource/fonts/") + aName;
        if (QFile::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QString(TYPEFACE_RESOURCE_FOLDER) + QDir::separator() + aName;
}
#endif
"""


def load(path):
    s = open(path, newline="", encoding="utf-8", errors="surrogateescape").read()
    return s.replace("\r\n", "\n"), ("\r\n" if "\r\n" in s else "\n")


def save(path, s, nl):
    open(path, "w", newline="", encoding="utf-8", errors="surrogateescape").write(s.replace("\n", nl))


def main(src):
    u = os.path.join(src, "hbcore", "utils")

    path = os.path.join(u, "hbtypefacexmlparser_p.h")
    s, nl = load(path)
    if MARK not in s:
        anchor = "    #define TYPEFACE_RESOURCE_FOLDER HB_RESOURCES_DIR\n#endif\n"
        if anchor not in s:
            sys.exit("hb_drives: anchor not found in " + path)
        save(path, s.replace(anchor, anchor + HELPER, 1), nl)

    path = os.path.join(u, "hbtypefacexmlparser.cpp")
    s, nl = load(path)
    if MARK not in s:
        a = s.index("        QString resourceFilePath(TYPEFACE_RESOURCE_FOLDER);\n")
        key = 'resourceFilePath.append("typefaces.xml");'
        b = s.index("\n", s.index(key, a)) + 1
        old = s[a:b]
        new = ("#ifdef Q_OS_SYMBIAN\n"
               '        QString resourceFilePath(hbTypefaceResourceFile("typefaces.xml")); // ' + MARK + "\n"
               "#else\n" + old + "#endif\n")
        save(path, s[:a] + new + s[b:], nl)

    path = os.path.join(u, "hbtypefaceinfodatabase_p.cpp")
    s, nl = load(path)
    if MARK not in s:
        a = s.index("    typefaceMetricsFileName.append(TYPEFACE_RESOURCE_FOLDER);\n")
        end = '    typefaceMetricsFileName.append(".xml");\n'
        b = s.index(end, a) + len(end)
        old = s[a:b]
        cut = old.index("    // replace whitespace")
        head, tail = old[:cut], old[cut:]
        cut2 = tail.index("    typefaceMetricsFileName.append( temp );")
        keep, rest = tail[:cut2], tail[cut2:]
        new = (keep + "#ifdef Q_OS_SYMBIAN\n"
               "    typefaceMetricsFileName = hbTypefaceResourceFile(QString(TYPEFACE_METRICS_FILE_STEM) + temp + \".xml\"); // " + MARK + "\n"
               "#else\n" + head + rest + "#endif\n")
        save(path, s[:a] + new + s[b:], nl)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
