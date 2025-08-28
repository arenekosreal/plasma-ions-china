#pragma once

#include <QCache>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#ifdef ION_LEGACY
#include <ion.h>
#else
#include <weatherion_export.h>
typedef Ion IonInterface
#error "Not implemented yet."
#endif

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
// Referer: FORECAST_CITY_PAGE
// URL Params: stationid; _;
// Hourly weather data is hardcoded in FORECAST_CITY_PAGE.
#define WEATHER_API API_BASE "/rest/weather"

struct HourlyInfo
{
    QDateTime time;
    QString img;
    double rain;
    double temperature;
    double windSpeed;
    QString windDirect;
    double atmosphericPressure;
    double humidity;
    double possibility;
};

struct WarnInfo
{
    QDateTime startTime;
    QJsonObject warnObject;
};

class Q_DECL_EXPORT WwwNmcCnIon : public IonInterface
{
    Q_OBJECT
public:
    WwwNmcCnIon(QObject *parent);
    ~WwwNmcCnIon() override;

private:
    const char placeInfoSep = '|';
    const char extraDataSep = ';';
    QCache<QString, QList<WarnInfo>> warnInfoCache;

    QNetworkReply *requestSearchingPlacesApi(QNetworkAccessManager &networkAccessManager, const QString &searchString, const int searchLimit = 10);
    QNetworkReply *requestWeatherApi(QNetworkAccessManager &networkAccessManager, const QString &stationId, const QString &referer);
    QNetworkReply *requestWebPage(QNetworkAccessManager &networkAccessManager, const QUrl &webPage);
    ConditionIcons getWeatherConditionIcon(const QString &img, bool windy, bool night) const;
    bool updateWarnInfoCache(const QJsonObject &warnObject, const QString &stationId);
    static QJsonArray extractSearchApiResponse(QNetworkReply *reply);
    static QJsonObject extractWeatherApiResponse(QNetworkReply *reply);
    static QList<HourlyInfo> extractWebPage(QNetworkReply *reply);
    template<typename T>
    static T handleNetworkReply(const QNetworkReply *reply, std::function<T(QNetworkReply*)> callable);

#ifdef ION_LEGACY
private:
    const char sourceSep = '|';
    // Slots
    void onSearchApiRequestFinished(QNetworkReply *reply, const QString &source);
    void onWeatherApiRequestFinished(QNetworkReply *reply, const QString &source, const QString &creditUrl, Plasma5Support::DataEngine::Data &data, const bool callSetData);
    void onWebPageRequestFinished(QNetworkReply *reply, const QString &source, Plasma5Support::DataEngine::Data &data, const bool callSetData);

// IonInterface API
protected:
    bool updateIonSource(const QString &source) override;

public Q_SLOTS:
    void reset() override;
#else // ION_LEGACY
// IonInterface API
public Q_SLOTS:
    void findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &serchString) override;
    void fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo) override;
#endif // ION_LEGACY
};
