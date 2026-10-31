// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0

// iapclient.cpp -- configurable In-App Purchase stand-in. Answers are read from c:\data\phorepoc\iap\iap.ini (QSettings, IniFormat)
// at call time, so any app (or the user) can change what each function returns without restarting anything:
//
//   [purchaseProduct]               status / ticket for every product (default status: ConnectionError)
//   [purchaseProduct/<productId>]   the same, for one product
//   [getRestorableProducts]         status (default ok), products=id1,id2
//   [getProductData]                status (default ConnectionError), plus any extra keys returned in the hash
//   [getProductData/<productId>]    the same, for one product (price=0 ...)
//   [getUserAndDeviceData]          status (default ConnectionError)
//   [owned]                         ids=...   written here when a purchase completes with status ok
//
// Without a file this is what a phone sees with the store gone. Every call logs one "PH: iap" line.
#include "iapclient.h"

#include <QSettings>
#include <QDir>
#include <QStringList>
#include <QMetaType>
#include <QMetaObject>
#include <e32debug.h>

static const char KConfig[] = "c:/data/phorepoc/iap/iap.ini";
static int gNextRequest = 0;

class IAPClientPrivate
{
public:
    IAPClientPrivate() : settings(QString::fromLatin1(KConfig), QSettings::IniFormat) {}
    QSettings settings;
};

static void logCall(const char *fn, const QString &pid, const QString &status, int id)
{
    RDebug::Printf("PH: iap %s product=%s -> %s (request %d)", fn, pid.toLatin1().constData(),
                   status.toLatin1().constData(), id);
}

// <func>/<product>/<key> wins over <func>/<key>
static QVariant cfg(QSettings &s, const QString &fn, const QString &pid, const QString &key, const QVariant &def)
{
    if (!pid.isEmpty() && s.contains(fn + QLatin1Char('/') + pid + QLatin1Char('/') + key))
        return s.value(fn + QLatin1Char('/') + pid + QLatin1Char('/') + key);
    return s.value(fn + QLatin1Char('/') + key, def);
}

IAPClient::IAPClient(QObject *parent) : QObject(parent), d(new IAPClientPrivate)
{
    qRegisterMetaType<IAPClient::ProductDataHash>("IAPClient::ProductDataHash");
    qRegisterMetaType<IAPClient::ProductDataList>("IAPClient::ProductDataList");
}

IAPClient::~IAPClient()
{
    delete d;
}

int IAPClient::purchaseProduct(const QString &productId, IAPClient::ForceRestorationFlag)
{
    d->settings.sync();
    const int id = ++gNextRequest;
    const QString status = cfg(d->settings, QLatin1String("purchaseProduct"), productId, QLatin1String("status"),
                               QLatin1String("ConnectionError")).toString();
    const QString ticket = cfg(d->settings, QLatin1String("purchaseProduct"), productId, QLatin1String("ticket"), QString()).toString();
    if (status == QLatin1String("ok")) {
        QStringList owned = d->settings.value(QLatin1String("owned/ids")).toStringList();
        if (!owned.contains(productId)) {
            owned << productId;
            QDir().mkpath(QLatin1String("c:/data/phorepoc/iap"));
            d->settings.setValue(QLatin1String("owned/ids"), owned);
            d->settings.sync();
        }
    }
    logCall("purchaseProduct", productId, status, id);
    QMetaObject::invokeMethod(this, "deliverPurchase", Qt::QueuedConnection,
                              Q_ARG(int, id), Q_ARG(QString, status), Q_ARG(QString, ticket));
    return id;
}

int IAPClient::getProductData(const QString &productId)
{
    d->settings.sync();
    const int id = ++gNextRequest;
    const QString status = cfg(d->settings, QLatin1String("getProductData"), productId, QLatin1String("status"),
                               QLatin1String("ConnectionError")).toString();
    ProductDataHash h;
    if (status == QLatin1String("ok")) {
        // product-wide keys first, then this product's own (they win)
        const QStringList groups = QStringList() << QLatin1String("getProductData")
                                                 << QLatin1String("getProductData/") + productId;
        foreach (const QString &g, groups) {
            d->settings.beginGroup(g);
            foreach (const QString &k, d->settings.childKeys())
                if (k != QLatin1String("status"))
                    h.insert(k, d->settings.value(k));
            d->settings.endGroup();
        }
        h.insert(QLatin1String("id"), productId);
    }
    logCall("getProductData", productId, status, id);
    QMetaObject::invokeMethod(this, "deliverProductData", Qt::QueuedConnection,
                              Q_ARG(int, id), Q_ARG(QString, status), Q_ARG(IAPClient::ProductDataHash, h));
    return id;
}

int IAPClient::getRestorableProducts()
{
    d->settings.sync();
    const int id = ++gNextRequest;
    const QString status = d->settings.value(QLatin1String("getRestorableProducts/status"), QLatin1String("ok")).toString();
    ProductDataList list;
    if (status == QLatin1String("ok")) {
        QStringList ids = d->settings.value(QLatin1String("getRestorableProducts/products")).toStringList();
        ids << d->settings.value(QLatin1String("owned/ids")).toStringList();
        QStringList seen;
        foreach (const QString &p, ids) {
            const QString t = p.trimmed();
            if (t.isEmpty() || seen.contains(t))
                continue;
            seen << t;
            ProductDataHash h;
            h.insert(QLatin1String("id"), t);
            list.append(h);
        }
    }
    logCall("getRestorableProducts", QString::number(list.count()) + QLatin1String(" products"), status, id);
    QMetaObject::invokeMethod(this, "deliverRestorable", Qt::QueuedConnection,
                              Q_ARG(int, id), Q_ARG(QString, status), Q_ARG(IAPClient::ProductDataList, list));
    return id;
}

int IAPClient::getUserAndDeviceData()
{
    d->settings.sync();
    const int id = ++gNextRequest;
    const QString status = d->settings.value(QLatin1String("getUserAndDeviceData/status"), QLatin1String("ConnectionError")).toString();
    logCall("getUserAndDeviceData", QString(), status, id);
    QMetaObject::invokeMethod(this, "deliverUserAndDevice", Qt::QueuedConnection, Q_ARG(int, id), Q_ARG(QString, status));
    return id;
}

void IAPClient::deliverPurchase(int requestId, QString status, QString ticket)
{
    emit purchaseCompleted(requestId, status, ticket);
    emit purchaseFlowFinished(requestId);
}

void IAPClient::deliverProductData(int requestId, QString status, IAPClient::ProductDataHash data)
{
    emit productDataReceived(requestId, status, data);
}

void IAPClient::deliverRestorable(int requestId, QString status, IAPClient::ProductDataList list)
{
    emit restorableProductsReceived(requestId, status, list);
}

void IAPClient::deliverUserAndDevice(int requestId, QString status)
{
    emit userAndDeviceDataReceived(requestId, status);
}
