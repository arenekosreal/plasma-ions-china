#include <KUnitConversion/Unit>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <ion.h>

class QWeather : public Ion
{
    Q_OBJECT

public:
    QWeather(QObject *parent);
    ~QWeather();

public Q_SLOTS:
    // Ion apis
    void findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &searchString) override;
    void fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo) override;

private:
    constexpr static QJsonDocument::JsonFormat jsonFormat = QJsonDocument::Compact;
    constexpr static QByteArray::Base64Options base64Options = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;
    constexpr static Qt::ConnectionType signalConnectionType = (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection);
    constexpr static Qt::DateFormat dateFormat = Qt::ISODate;
    constexpr static QLocale::FormatType warningTimestampFormat = QLocale::ShortFormat;
    constexpr static KUnitConversion::UnitId temperatureUnit = KUnitConversion::Celsius;
    constexpr static KUnitConversion::UnitId windSpeedUnit = KUnitConversion::MeterPerSecond;
    constexpr static KUnitConversion::UnitId visibilityUnit = KUnitConversion::Meter;
    constexpr static KUnitConversion::UnitId pressureUnit = KUnitConversion::Hectopascal;
    constexpr static KUnitConversion::UnitId preciptionUnit = KUnitConversion::Millimeter;
    constexpr static KUnitConversion::UnitId humidityUnit = KUnitConversion::Percent;

    const QUrl apiBase;

    quint8 retryTimes = 0;
    QDateTime retryAfter;
    QString currentToken;
    QNetworkAccessManager networkAccessManager;

    const QString getJwtToken(const qint64 iatOffset = -30, const qint64 expOffset = 3 * 60 * 60) const;
    const QNetworkRequest makeApiRequest(const QString &path);
    const QNetworkRequest makeApiRequest(const QString &path, const QUrlQuery &query);
    bool isCurrentJwtTokenNeedsRefresh(const qint64 expireOffset = -15 * 60) const;
    quint64 getRequestBackoffSeconds() const;
    void onNetworkError();
    ConditionIcons getWeatherIcon(const QString &icon, const bool windy) const;
    const QJsonObject extractResponse(QNetworkReply *reply);
    Warnings::PriorityClass getPriority(const QString &severity) const;
    bool fillCurrentWeather(std::shared_ptr<Forecast> forecast, const QJsonObject &currentResponse);
    bool fillDailyWeather(std::shared_ptr<Forecast> forecast, const QJsonObject &dailyResponse);
    bool fillWarnings(std::shared_ptr<Forecast> forecast, const QJsonObject &warningsResponse, const QString &warningUrl);
    const QByteArray signMessage(const QString &message) const;
};
