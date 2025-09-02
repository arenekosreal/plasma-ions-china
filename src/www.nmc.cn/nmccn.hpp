#pragma once

#include <QCache>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "nmccn_debug.hpp"

#ifdef ION_LEGACY
#include <ion.h>
#else // ION_LEGACY
#include <weatherion_export.h>
typedef Ion IonInterface
#error "Not implemented yet."
#endif // ION_LEGACY

#define USER_AGENT "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36"
// String 9999 means an invalid value from api, maybe record is outdated.
#define INVALID_VALUE_STR "9999"

#define API_BASE "https://www.nmc.cn"
#define FORECAST_PAGE API_BASE "/publish/forecast.html"
// FORECAST_CITY_PAGE + "/xxx/xxxxxxxxx.html"
#define FORECAST_CITY_PAGE API_BASE "/publish/forecast"
// Referer: FORECAST_PAGE
// URL Params: q; limit; timestamp; _;
#define SEARCH_API API_BASE "/essearch/api/autocomplete"
// Referer: FORECAST_CITY_PAGE + "/xxx/xxxxxxxxx.html"
// URL Params: stationid; _;
// Hourly weather data is hardcoded in FORECAST_CITY_PAGE.
#define WEATHER_API API_BASE "/rest/weather"

struct WarnInfo
{
    QDateTime startTime;
    QJsonObject warnObject;
};

class Q_DECL_EXPORT NmcCnIon : public IonInterface
{
    Q_OBJECT
public:
    NmcCnIon(QObject *parent);
    ~NmcCnIon() override;

private:
    const char placeInfoSep = '|';
    const char extraDataSep = ';';
    const QString realMonthDayFormat = "MM-dd";
    const QString realDateFormat = "yyyy-" + realMonthDayFormat;
    const QString realTimeFormat = "HH:mm";
    const QString realDateTimeFormat = realDateFormat + " " + realTimeFormat;
    QCache<QString, QList<WarnInfo>> warnInfoCache;
    QCache<QString, QJsonObject> lastValidDayCache;
    QNetworkAccessManager networkAccessManager;

    QNetworkReply *requestSearchingPlacesApi(const QString &searchString, const int searchLimit = 10);
    QNetworkReply *requestWeatherApi(const QString &stationId, const QString &referer);
    ConditionIcons getWeatherConditionIcon(const QString &img, const bool windy, const bool night) const;
    QString getWindDirectionString(const float degree) const;
    bool updateWarnInfoCache(const QJsonObject &warnObject, const QString &stationId);
    bool updateLastValidDayCache(const QJsonObject &day, const QString &stationId);
    QJsonArray extractSearchApiResponse(QNetworkReply *reply);
    QJsonObject extractWeatherApiResponse(QNetworkReply *reply);
    template<typename T>
    T handleNetworkReply(QNetworkReply *reply, std::function<T(QNetworkReply*)> callable)
    {
        if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
            qDebug(IONENGINE_NMCCN) << "Request successfully.";
            return callable(reply);
        }
        else {
            qFatal(IONENGINE_NMCCN) << "Request failed with error: " << reply->error();
        }
        T ret;
        return ret;
    };

#ifdef ION_LEGACY
private:
    const char sourceSep = '|';
    QCache<QString, Plasma5Support::DataEngine::Data> dataCache;

private slots:
    void onSearchApiRequestFinished(QNetworkReply *reply, const QString &source);
    void onWeatherApiRequestFinished(QNetworkReply *reply, const QString &source, const QString &creditUrl, const bool callSetData);

// IonInterface API
protected:
    bool updateIonSource(const QString &source) override;

public slots:
    void reset() override;
#else // ION_LEGACY
// IonInterface API
public slots:
    void findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &serchString) override;
    void fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo) override;
#endif // ION_LEGACY
};
