#include <QFile>
#include <sodium.h>

#include "ed25519.hpp"
#include "ed25519_debug.hpp"

QEd25519PrivateKey::QEd25519PrivateKey(const QByteArray &privateKeySeed, QObject *parent)
    : QObject(parent)
    , privateKeySeed(privateKeySeed)
{
}

QEd25519PrivateKey::~QEd25519PrivateKey()
{
}

const QEd25519PrivateKey QEd25519PrivateKey::fromPEMFile(const QString &pemFilePath)
{
    const QString privateKeyContentBeginMark = QStringLiteral("-----BEGIN PRIVATE KEY-----");
    const QString privateKeyContentEndMark = QStringLiteral("-----END PRIVATE KEY-----");
    QFile pemFile(pemFilePath);
    if (!pemFile.open(QIODevice::ReadOnly))
        qFatal(WEATHER::HELPER::ED25519) << "Failed to open private key file.";
    QTextStream ts(&pemFile);
    bool inPrivateKey = false;
    QString privateKeyContent;
    while (!ts.atEnd()) {
        const QString currentLine = ts.readLine().simplified();
        if (!inPrivateKey && currentLine == privateKeyContentBeginMark)
            inPrivateKey = true;
        else if (inPrivateKey && currentLine == privateKeyContentEndMark)
            inPrivateKey = false;
        if (inPrivateKey && currentLine != privateKeyContentBeginMark)
            privateKeyContent.append(currentLine);
    }
    pemFile.close();
    const QByteArray decodedPrivateKey = QByteArray::fromBase64(privateKeyContent.toUtf8());
    // OpenSSL generated ed25519 key's seed is always located at the last 32 bytes.
    if (decodedPrivateKey.size() < 32)
        qFatal(WEATHER::HELPER::ED25519) << "Invalid private key. Needs >= 32 bytes," << decodedPrivateKey.size() << "actual.";
    const QByteArray privateKeySeed = decodedPrivateKey.right(32);
    return QEd25519PrivateKey(privateKeySeed);
}

const QByteArray QEd25519PrivateKey::signMessage(const QString &message) const
{
    if (sodium_init() < 0)
        qFatal(WEATHER::HELPER::ED25519) << "Failed to initialize libsodium.";
    unsigned char publicKey[crypto_sign_ed25519_PUBLICKEYBYTES];
    unsigned char secretKey[crypto_sign_ed25519_SECRETKEYBYTES];
    crypto_sign_ed25519_seed_keypair(publicKey, secretKey, reinterpret_cast<const unsigned char *>(privateKeySeed.constData()));
    unsigned char signature[crypto_sign_ed25519_BYTES];
    unsigned long long signatureSize;
    const QByteArray messageData = message.toUtf8();
    crypto_sign_ed25519_detached(signature, &signatureSize, reinterpret_cast<const unsigned char *>(messageData.constData()), messageData.size(), secretKey);
    return QByteArray(reinterpret_cast<const char *>(signature), signatureSize);
}

// #include "ed25519.moc"
