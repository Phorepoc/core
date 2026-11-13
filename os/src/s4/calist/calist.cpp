// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// calist.cpp -- OURS (Symbian^4 S4.5, docs/phorepoc/symbian4-build.md): the content-arsenal checkpoint. Asks the
// S^4 app library's storage (homescreensrv's CaService -> camenuserver -> castorage, filled by caappscanner from
// AppArc and the Software Component Registry) for its application entries and prints them.
// Output through qDebug (on Symbian: RDebug, the serial log).
#include <QCoreApplication>
#include <QDebug>
#include <QSharedPointer>
#include <QStringList>
#include <caservice.h>
#include <caquery.h>
#include <caentry.h>
#include <unistd.h>
#include <e32std.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QSharedPointer<CaService> service = CaService::instance();
    qDebug("[calist] CaService up");

    CaQuery query;
    query.setEntryTypeNames(QStringList() << "application");
    // the server's app scanner fills the storage from AppArc after it starts (asynchronously): the release's database
    // starts with caption-less placeholders (the "essentials" collection), so ask until the captions are there
    QList< QSharedPointer<CaEntry> > entries;
    for (int attempt = 0; attempt < 9; ++attempt) {
        if (attempt)
            sleep(10);
        entries = service->getEntries(query);
        int named = 0;
        for (int i = 0; i < entries.count(); ++i)
            named += !entries[i]->text().isEmpty();
        qDebug("[calist] %d application entries, %d with a caption (lastError %d)", entries.count(), named,
               int(service->lastError()));
        if (named)
            break;
    }
    // which of the servers behind it run
    TFindProcess find(_L("*"));
    TFullName name;
    while (find.Next(name) == KErrNone) {
        if (name.FindF(_L("camenu")) >= 0 || name.FindF(_L("scr")) >= 0 || name.FindF(_L("transaction")) >= 0) {
            TBuf8<KMaxFullName> n8;
            n8.Copy(name);
            qDebug("[calist] running: %s", (const char *)n8.PtrZ());
        }
    }
    for (int i = 0; i < entries.count(); ++i)
        qDebug("[calist]   %d: %s", entries[i]->id(), qPrintable(entries[i]->text()));
    qDebug("[calist] done");
    return 0;
}
