// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// qttest.cpp -- OURS: log every Qt start-up step (qDebug -> RDebug), to see where a Qt app stops.
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QWidget>
#include <QtGui/QStyle>
#include <QtGui/QLabel>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <e32debug.h>

struct TEarly { TEarly() { RDebug::Printf("[qttest] static init"); } } gEarly;

int main(int argc, char* argv[])
	{
	RDebug::Printf("[qttest] main, argc %d", argc);
	RDebug::Printf("[qttest] before QApplication");
	QApplication app(argc, argv);
	RDebug::Printf("[qttest] QApplication up");
	QDesktopWidget* d = QApplication::desktop();
	QRect g = d->screenGeometry();
	qDebug("[qttest] desktop %d x %d, screens %d", g.width(), g.height(), d->screenCount());
	qDebug("[qttest] style %s", qPrintable(app.style()->objectName()));
	qDebug("[qttest] applicationDirPath %s", qPrintable(QCoreApplication::applicationDirPath()));
	qDebug("[qttest] C:/private exists %d", (int)QDir(QLatin1String("C:/private")).exists());
	QLabel w(QLatin1String("Qt start-up test"));
	w.showMaximized();
	qDebug("[qttest] window shown");
	QTimer::singleShot(8000, &app, SLOT(quit()));
	const int r = app.exec();
	qDebug("[qttest] DONE %d", r);
	return r;
	}
