#pragma once

#include <QObject>

class QEd25519PrivateKey : public QObject
{
    Q_OBJECT

public:
    QEd25519PrivateKey(const QByteArray &privateKeySeed, QObject *parent = nullptr);
    ~QEd25519PrivateKey();
    static const QEd25519PrivateKey fromPEMFile(const QString &pemFilePath);
    const QByteArray signMessage(const QString &message) const;

private:
    const QByteArray privateKeySeed;
};