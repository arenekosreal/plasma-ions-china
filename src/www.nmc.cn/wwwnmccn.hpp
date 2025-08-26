#pragma once

#include <QCache>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QReadWriteLock>

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

struct StationInfo
{
    QString code;
    QString province;
    QString city;
    QString url;
};

struct BasicWeatherInfo
{
    QString info;
    QString img;
    double temperature;
};

struct WeatherInfo : public BasicWeatherInfo
{
    double temperatureDiff;
    int airpressure;
    int humidity;
    double rain;
    int rcomfort;
    int icomfort;
    double feelst;
};

struct BasicWindInfo
{
    QString direct;
    QString power;
};

struct WindInfo : public BasicWindInfo
{
    double degree;
    double speed;
};

struct WarnInfo
{
    QString alert;
    QString pic;
    QString province;
    QString city;
    QString url;
    QString issuecontent;
    QString fmeans;
    QString signaltype;
    QString signallevel;
    QString pic2;
    // Not in JSON
    QDateTime expireTime;
};

typedef QList<WarnInfo> WarnInfoList;

struct SunriseSunsetInfo
{
    QDateTime sunrise;
    QDateTime sunset;
};

struct BasicInfo
{
    StationInfo station;
    QDateTime publish_time;
};

struct RealInfo : public BasicInfo
{
    WeatherInfo weather;
    WindInfo wind;
    WarnInfo warn;
    SunriseSunsetInfo sunriseSunset;
};

struct BasicWeatherWithWindInfo
{
    BasicWeatherInfo weather;
    BasicWindInfo wind;
};

struct DetailInfo
{
    QDate date;
    QDateTime pt;
    BasicWeatherWithWindInfo day;
    BasicWeatherWithWindInfo night;
    double precipitation;
};

struct PredictInfo : public BasicInfo
{
    QList<DetailInfo> detail;
};

struct AirInfo
{
    QDateTime forecasttime;
    int aqi;
    int aq;
    QString text;
    QString aqiCode;
};

struct TempChartInfo
{
    QDate time;
    double max_temp;
    double min_temp;
    QString day_img;
    QString day_text;
    QString night_img;
    QString night_text;
};

struct WeatherInfoData
{
    RealInfo real;
    PredictInfo predict;
    AirInfo air;
    QList<TempChartInfo> tempchart;
};

template<typename T>
struct ApiResponse
{
    QString msg;
    int code;
    T data;
};
typedef ApiResponse<WeatherInfoData> WeatherApiResponse;
typedef ApiResponse<QStringList> StationSearchApiResponse;

struct HourlyInfo : public BasicInfo
{
    QDateTime time;
    QString img;
    double rain;
    double temperature;
    double windSpeed;
    QString windDirect;
    int pressure;
    int humidity;
    double possibility;
};

typedef QList<HourlyInfo> HourlyInfoList;

class Q_DECL_EXPORT WwwNmcCnIon : public IonInterface
{
    Q_OBJECT
public:
    WwwNmcCnIon(QObject *parent);
    ~WwwNmcCnIon() override;

private:
    const char placeInfoSep = '|';
    QCache<QString, WarnInfoList> activeWarnings;
    QCache<QString, WeatherApiResponse> weathers;
    QCache<QString, HourlyInfoList> hourlyWeathers;
    QCache<QString, StationSearchApiResponse> searchedStations;
    QMap<QUrl, QString> stationIdMap;
    QReadWriteLock readWriteLock;

    StationSearchApiResponse searchPlacesApi(QNetworkAccessManager &networkAccessManager, const QString &searchString, const int searchLimit = 10);
    WeatherApiResponse searchWeatherApi(QNetworkAccessManager &networkAccessManager, const QString &stationId, const QUrl &referer);
    HourlyInfoList extractWebPage(QNetworkAccessManager &networkAccessManager, const QUrl &webPage);
    ConditionIcons getWeatherConditionIcon(const QString &img, bool windy, bool night) const;
    // Slots
    void onNetworkRequestFinished(QNetworkReply *reply);

#ifdef ION_LEGACY
private:
    const char sourceSep = '|';

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
