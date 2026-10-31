// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0

// iapclient.h -- Phorepoc's own clean-room stand-in for the Nokia In-App Purchase client API (iapclientapi.dll). Written from
// the binary interface a consumer imports (class layout, three proven ordinals, the signal signatures it connects by string),
// not from any vendor header. The class is a QObject plus ONE private d-pointer (sizeof == 12): callers allocate it themselves.
#ifndef PHOREPOC_IAPCLIENT_H
#define PHOREPOC_IAPCLIENT_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QHash>
#include <QList>

class IAPClientPrivate;

class Q_DECL_EXPORT IAPClient : public QObject
{
    Q_OBJECT
public:
    typedef QHash<QString, QVariant> ProductDataHash;
    typedef QList<ProductDataHash> ProductDataList;

    enum ForceRestorationFlag { NoForcedRestoration = 0, ForcedRestoration = 1 };

    explicit IAPClient(QObject *parent = 0);
    virtual ~IAPClient();

    // every call returns a new positive request id at once and answers through its signal(s) later, from the event loop
    int purchaseProduct(const QString &productId, IAPClient::ForceRestorationFlag flag = NoForcedRestoration);
    int getProductData(const QString &productId);
    int getRestorableProducts();
    int getUserAndDeviceData();

Q_SIGNALS:
    void purchaseCompleted(int requestId, QString status, QString purchaseTicket);
    void purchaseFlowFinished(int requestId);
    void productDataReceived(int requestId, QString status, IAPClient::ProductDataHash productData);
    void restorableProductsReceived(int requestId, QString status, IAPClient::ProductDataList products);
    void userAndDeviceDataReceived(int requestId, QString status);

private Q_SLOTS:
    void deliverPurchase(int requestId, QString status, QString ticket);
    void deliverProductData(int requestId, QString status, IAPClient::ProductDataHash data);
    void deliverRestorable(int requestId, QString status, IAPClient::ProductDataList list);
    void deliverUserAndDevice(int requestId, QString status);

private:
    IAPClientPrivate *d;
};

// the ABI contract: a QObject (vptr + d_ptr) and one pointer
typedef char iapclient_size_is_12[(sizeof(IAPClient) == 12) ? 1 : -1];

#endif
