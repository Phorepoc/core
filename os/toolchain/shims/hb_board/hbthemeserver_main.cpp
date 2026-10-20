/****************************************************************************
**
** Copyright (C) 2008-2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (developer.feedback@nokia.com)
**
** This file is part of the HbServers module of the UI Extensions for Mobile.
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at developer.feedback@nokia.com.
**
****************************************************************************/

#include "hbthemeserverapplication_p.h"
#include <QTextStream>
#include <cstdlib>
#include <QDebug>

int main(int argc, char *argv[])
{
    // Hiding theme server from the start up in first phase
#if QT_VERSION >= 0x040601
    QApplication::setAttribute(Qt::AA_S60DontConstructApplicationPanes);
#endif // QT_VERSION

    // We need to be up and running fast
    HbThemeServerApplication::setPriority();

    HbThemeServerLocker locker;
    if(!locker.lock()) {
        return 0;
    }
    
    HbThemeServerApplication app(argc, argv);

#ifndef Q_OS_SYMBIAN
    if (app.isRunning()) {
        return EXIT_SUCCESS;
    }
#else
    // PHOREPOC board delta (2026-10-18): HbThemeServerLocker above already holds the Symbian single-instance lock (an
    // exclusive RFile). QtSingleApplication's own check (QtLockedFile, an Open C fcntl F_SETLK lock in QDir::tempPath())
    // fails on our Open C and reports "already running": the first theme server exited 0 right after its rendezvous
    // (PinePhone boots 1013-1032, x86 VirtualBox), every Orbit client then found no shared cache (HbSharedCache NULL).
    // isRunning() is still called for its side effect (the peer's local server for messages), its answer is not trusted.
    (void)app.isRunning();
#endif

    if (app.initialize()) {
        return app.exec();
    }

    return EXIT_FAILURE;
}
