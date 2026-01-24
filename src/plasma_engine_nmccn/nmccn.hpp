#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <ion.h>

class Q_DECL_EXPORT NmcCn : public IonInterface
{
    Q_OBJECT
public:
    NmcCn(QObject *parent);
    ~NmcCn();

protected:
    bool updateIonSource(const QString &source) override; // Ion apis
public Q_SLOTS:
    void reset() override; // Ion apis

private:
    const QString userAgent = QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36"),
                  apiBase = QStringLiteral("https://www.nmc.cn");
    const QString dateFormat = QStringLiteral("MM-dd"), fullDateFormat = QStringLiteral("yyyy-") + dateFormat, timeFormat = QStringLiteral("HH:mm"),
                  fullTimeFormat = QStringLiteral("%1 %2").arg(fullDateFormat).arg(timeFormat);
    const QString invalidValue = QStringLiteral("9999");
    const QChar sourcePartsSep = QLatin1Char('|'),
                placeInfoSep = QLatin1Char('|'),
                extraDataPartsSep = QLatin1Char(';');

    QNetworkAccessManager networkAccessManager;

    QJsonValue extractApiResponse(QNetworkReply *reply) const;
    ConditionIcons getWeatherConditionIcon(const QString &img, const bool windy, const bool night) const;
    WindDirections getWindDirection(const float degree) const;
    QString getWindDirection(const WindDirections windDirection) const;
    void findPlaces(const QString &searchString, const int searchLimit = 10);
    void fetchForecast(const QString &stationId, const QString &referer);
Q_SIGNALS:
    void foundPlaces(const QJsonArray &responseData);
    void fetchedForecast(const QJsonObject &responseData);
};
