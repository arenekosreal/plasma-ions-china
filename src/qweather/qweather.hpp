#include <KUnitConversion/Unit>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <ion.h>

#include "ed25519.hpp"

class QWeather : public Ion
{
    Q_OBJECT

    // https://dev.qweather.com/docs/resource/indices-info/#index-type
    enum IndexType {
        ALL,
        SPT,
        CW,
        DRSG,
        FIS,
        UV,
        TRA,
        AG,
        COMF,
        FLU,
        AP,
        AC,
        GL,
        MU,
        DC,
        PTFC,
        SPI
    };

    Q_ENUM(IndexType)

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
    constexpr static KUnitConversion::UnitId temperatureUnit = KUnitConversion::Celsius;
    constexpr static KUnitConversion::UnitId windSpeedUnit = KUnitConversion::KilometerPerHour;
    constexpr static KUnitConversion::UnitId visibilityUnit = KUnitConversion::Kilometer;
    constexpr static KUnitConversion::UnitId pressureUnit = KUnitConversion::Hectopascal;
    constexpr static KUnitConversion::UnitId preciptionUnit = KUnitConversion::Millimeter;
    constexpr static KUnitConversion::UnitId humidityUnit = KUnitConversion::Percent;

    const QEd25519PrivateKey privateKey = QEd25519PrivateKey::fromPEMFile(QStringLiteral(KEY_FILE));
    const QUrl apiBase = QUrl(QStringLiteral("https://" API_HOST));

    quint8 retryTimes = 0;
    QDateTime retryAfter;
    QString currentToken;
    QNetworkAccessManager networkAccessManager;
    QMutex mutex;

    const QString getJwtToken(const qint64 iatOffset = -30, const qint64 expOffset = 3 * 60 * 60) const;
    const QNetworkRequest makeApiRequest(const QString &path);
    const QNetworkRequest makeApiRequest(const QString &path, const QUrlQuery &query);
    bool isCurrentJwtTokenNeedsRefresh(const qint64 expireOffset = -15 * 60) const;
    quint64 getRequestBackoffSeconds() const;
    void onNetworkError();
    const LastObservation getLastObservation(const QJsonObject &response, const int uvIndex) const;
    ConditionIcons getWeatherIcon(const QString &icon, const bool windy) const;
    Ion::WindDirections getWindDirectionIcon(const double degree) const;
    const QString getWindDirectionIcon(const Ion::WindDirections windDirection) const;
    void updateFutureDays(std::shared_ptr<FutureDays> futureDays, const QJsonObject &futureDaysResponse);

    static const MetaData getMetaData(const QString &credit, const QString &creditUrl);
    static int getIndexValue(const QJsonObject &indexResponse, const IndexType indexType, int defaultValue = -1);
    static const QJsonObject extractResponse(QNetworkReply *reply);
    static void updateWarnings(std::shared_ptr<Warnings> warnings, const QJsonObject &warningsResponse);
    static qreal getWindChill(const qreal temperature, const qreal windSpeed);
    static qreal getHeatIndexFromHumidity(const qreal temperature, const qreal humidity);
    static qreal getHeatIndexFromDewpoint(const qreal temperature, const qreal dewpoint);
    static const QString getHumidex(const qreal humidexValue);
    static qreal getHumidex(const qreal temperature, const qreal dewpoint);
    static qreal getDewpoint(const qreal temperature, const qreal humidity);
    static Warnings::PriorityClass getPriority(const QString &severity);
    static const QString getVisibility(const qreal visibilityValue);
};

const QString serialize(const Station &station);

const Station deserialize(const QString &serialized);

const Station stripNewPlaceInfo(const Station &station);
