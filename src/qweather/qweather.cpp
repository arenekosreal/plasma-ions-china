#include <KLocalizedString>
#include <KPluginFactory>
#include <KUnitConversion/Unit>
#include <KUnitConversion/Value>
#include <QDateTime>
#include <QFile>
#include <QFuture>
#include <QHash>
#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QUrlQuery>
#include <QtMath>

#include <sodium.h>

#include "qweather.hpp"
#include "qweather_credential.hpp"
#include "qweather_debug.hpp"

K_PLUGIN_CLASS_WITH_JSON(QWeather, "metadata.json");

QWeather::QWeather(QObject *parent)
    : Ion(parent)
    , apiBase(QStringLiteral("https://" API_HOST))
    , networkAccessManager(this)
{
    if (sodium_init() < 0) {
        qFatal(WEATHER::ION::QWEATHER) << "Failed to initialize libsodium.";
    }
}

QWeather::~QWeather()
{
    networkAccessManager.clearConnectionCache();
}

void QWeather::findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &searchString)
{
    if (retryAfter.isValid() && QDateTime::currentDateTime() <= retryAfter) {
        qWarning(WEATHER::ION::QWEATHER) << "Request has been stopped due to errors.";
        qWarning(WEATHER::ION::QWEATHER) << "Check other logs to find out reason.";
        return;
    }
    promise->start();
    if (promise->isCanceled()) {
        qDebug(WEATHER::ION::QWEATHER) << "Search request has been cancelled.";
        promise->finish();
        return;
    }
    qDebug(WEATHER::ION::QWEATHER) << "Finding place:" << searchString;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("location"), QString::fromUtf8(QUrl::toPercentEncoding(searchString)));
    const QNetworkRequest req = makeApiRequest(QStringLiteral("/geo/v2/city/lookup"), query);
    QNetworkReply *reply = networkAccessManager.get(req);
    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [=]() {
            const QJsonObject response = extractResponse(reply);
            qDebug(WEATHER::ION::QWEATHER) << "Got response" << response;
            if (response.isEmpty()) {
                qWarning(WEATHER::ION::QWEATHER) << "Got empty response object.";
                promise->finish();
                onNetworkError();
                return;
            }
            std::shared_ptr<Locations> locations = std::make_shared<Locations>();
            for (const QJsonValue &location : response[QStringLiteral("location")].toArray()) {
                const QString locationId = location[QStringLiteral("id")].toString();
                const double lat = location[QStringLiteral("lat")].toString().toDouble();
                const double lon = location[QStringLiteral("lon")].toString().toDouble();
                const QString region = location[QStringLiteral("adm1")].toString();
                const QString place = location[QStringLiteral("adm2")].toString();
                const QString station = location[QStringLiteral("name")].toString();
                const QString country = location[QStringLiteral("country")].toString();
                const QString displayName =
                    region.startsWith(place) ? QStringLiteral("%1-%2").arg(region, station) : QStringLiteral("%1-%2-%3").arg(region, place, station);
                Location toBeAdded;
                toBeAdded.setDisplayName(displayName);
                toBeAdded.setPlaceInfo(locationId);
                toBeAdded.setStation(station);
                toBeAdded.setCoordinates(QPointF(lat, lon));
                locations->addLocation(toBeAdded);
            }
            promise->addResult(locations);
            promise->finish();
            retryTimes = 0;
            retryAfter = QDateTime();
        },
        signalConnectionType);
}

void QWeather::fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo)
{
    if (retryAfter.isValid() && QDateTime::currentDateTime() <= retryAfter) {
        qWarning(WEATHER::ION::QWEATHER) << "Request has been stopped due to errors.";
        qWarning(WEATHER::ION::QWEATHER) << "Check other logs to find out reason.";
        return;
    }
    promise->start();
    if (promise->isCanceled()) {
        qDebug(WEATHER::ION::QWEATHER) << "Fetch request has been cancelled.";
        promise->finish();
        return;
    }
    qDebug(WEATHER::ION::QWEATHER) << "Fetching forecast for place info" << placeInfo;
    std::shared_ptr<Forecast> forecast = std::make_shared<Forecast>();
    QUrlQuery locationQuery;
    locationQuery.addQueryItem(QStringLiteral("location"), placeInfo);
    const QNetworkRequest locationRequest = makeApiRequest(QStringLiteral("/geo/v2/city/lookup"), locationQuery);
    QNetworkReply *locationReply = networkAccessManager.get(locationRequest);
    connect(
        locationReply,
        &QNetworkReply::finished,
        this,
        [=]() {
            const QJsonObject response = extractResponse(locationReply);
            qDebug(WEATHER::ION::QWEATHER) << "Got response" << response;
            if (response.isEmpty()) {
                qWarning(WEATHER::ION::QWEATHER) << "Unable to get valid response object.";
                promise->finish();
                onNetworkError();
                return;
            }
            const QJsonValue matched = response[QStringLiteral("location")].toArray().first();
            qDebug(WEATHER::ION::QWEATHER) << "Got location" << matched;
            MetaData metaData;
            metaData.setCredit(i18n("Source: %1", i18n("QWeather")));
            metaData.setCreditURL(matched[QStringLiteral("fxLink")].toString());
            metaData.setTemperatureUnit(temperatureUnit);
            metaData.setWindSpeedUnit(windSpeedUnit);
            metaData.setVisibilityUnit(visibilityUnit);
            metaData.setPressureUnit(pressureUnit);
            metaData.setHumidityUnit(humidityUnit);
            metaData.setRainfallUnit(preciptionUnit);
            metaData.setSnowfallUnit(preciptionUnit);
            metaData.setPrecipUnit(preciptionUnit);
            forecast->setMetadata(metaData);

            Station station;
            station.setStation(matched[QStringLiteral("name")].toString());
            station.setPlace(matched[QStringLiteral("name")].toString());
            station.setRegion(matched[QStringLiteral("adm1")].toString());
            station.setCountry(matched[QStringLiteral("country")].toString());
            station.setCoordinates(matched[QStringLiteral("lat")].toString().toDouble(), matched[QStringLiteral("lon")].toString().toDouble());
            forecast->setStation(station);
            const QNetworkRequest currentRequest = makeApiRequest(
                QStringLiteral("/weather/v1/current/%1/%2").arg(station.latitude().toDouble(), 0, 'f', 2).arg(station.longitude().toDouble(), 0, 'f', 2));
            QNetworkReply *currentReply = networkAccessManager.get(currentRequest);
            const QNetworkRequest dailyRequest = makeApiRequest(
                QStringLiteral("/weather/v1/daily/%1/%2").arg(station.latitude().toDouble(), 0, 'f', 2).arg(station.longitude().toDouble(), 0, 'f', 2));
            QNetworkReply *dailyReply = networkAccessManager.get(dailyRequest);
            const QNetworkRequest warningRequest = makeApiRequest(
                QStringLiteral("/weatheralert/v1/current/%1/%2").arg(station.latitude().toDouble(), 0, 'f', 2).arg(station.longitude().toDouble(), 0, 'f', 2));
            QNetworkReply *warningReply = networkAccessManager.get(warningRequest);
            QFuture<void> everythingFinished = QtFuture::whenAll(QtFuture::connect(currentReply, &QNetworkReply::finished),
                                                                 QtFuture::connect(dailyReply, &QNetworkReply::finished),
                                                                 QtFuture::connect(warningReply, &QNetworkReply::finished));
            everythingFinished.then(this, [=] {
                const QJsonObject currentResponse = extractResponse(currentReply);
                qDebug(WEATHER::ION::QWEATHER) << "Got response for current weather" << currentResponse;
                if (currentResponse.isEmpty()) {
                    qWarning(WEATHER::ION::QWEATHER) << "Got empty response for current weather";
                    promise->finish();
                    onNetworkError();
                    return;
                }
                const QJsonObject dailyResponse = extractResponse(dailyReply);
                qDebug(WEATHER::ION::QWEATHER) << "Got response for daily weather" << dailyResponse;
                if (dailyResponse.isEmpty()) {
                    qWarning(WEATHER::ION::QWEATHER) << "Got empty response for daily weather";
                    promise->finish();
                    onNetworkError();
                    return;
                }
                const QJsonObject warningResponse = extractResponse(warningReply);
                qDebug(WEATHER::ION::QWEATHER) << "Got response for current warnings" << warningResponse;
                if (warningResponse.isEmpty()) {
                    qWarning(WEATHER::ION::QWEATHER) << "Got empty response for current warnings";
                    promise->finish();
                    onNetworkError();
                    return;
                }
                const QString warningUrl = QString(forecast->metaData().value<MetaData>().creditURL().toString())
                                               .replace(QStringLiteral("/weather/"), QStringLiteral("/severe-weather/"));
                if (fillCurrentWeather(forecast, currentResponse) && fillDailyWeather(forecast, dailyResponse)
                    && fillWarnings(forecast, warningResponse, warningUrl)) {
                    promise->addResult(forecast);
                }
                promise->finish();
            });
        },
        signalConnectionType);
}

const QString QWeather::getJwtToken(const qint64 iatOffset, const qint64 expOffset) const
{
    QJsonObject header;
    header[QStringLiteral("alg")] = QStringLiteral("EdDSA");
    header[QStringLiteral("kid")] = QStringLiteral(KID);
    const QString headerBase64 = QString::fromUtf8(QJsonDocument(header).toJson(jsonFormat).toBase64(base64Options));
    QJsonObject payload;
    payload[QStringLiteral("sub")] = QStringLiteral(SUB);
    const quint64 currentTime = QDateTime::currentSecsSinceEpoch();
    payload[QStringLiteral("iat")] = QJsonValue::fromVariant(currentTime + iatOffset);
    payload[QStringLiteral("exp")] = QJsonValue::fromVariant(currentTime + expOffset);
    const QString payloadBase64 = QString::fromUtf8(QJsonDocument(payload).toJson(jsonFormat).toBase64(base64Options));
    const QString toBeSigned = headerBase64 + QStringLiteral(".") + payloadBase64;
    const QString signature = QString::fromUtf8(signMessage(toBeSigned).toBase64(base64Options));
    const QString token = toBeSigned + QStringLiteral(".") + signature;
    qDebug(WEATHER::ION::QWEATHER) << "Generated token:" << token;
    return token;
}

const QNetworkRequest QWeather::makeApiRequest(const QString &path)
{
    return makeApiRequest(path, QUrlQuery());
}

const QNetworkRequest QWeather::makeApiRequest(const QString &path, const QUrlQuery &query)
{
    if (isCurrentJwtTokenNeedsRefresh())
        currentToken = getJwtToken();
    QUrl url(apiBase);
    url.setPath(path);
    if (!query.isEmpty())
        url.setQuery(query);
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Authorization, QStringLiteral("Bearer %1").arg(currentToken));
    QNetworkRequest req(url);
    req.setHeaders(headers);
    qDebug(WEATHER::ION::QWEATHER) << "Generated request url" << req.url();
    qDebug(WEATHER::ION::QWEATHER) << "Generated request header" << req.headers();
    return req;
}

quint64 QWeather::getRequestBackoffSeconds() const
{
    if (retryTimes > 0) {
        qint64 retryAfter = qPow(2, retryTimes);
        QRandomGenerator rng;
        qint64 delay = -1;
        while (delay < 0 || delay > retryAfter - 1) {
            delay = rng.generate();
        }
        return retryAfter + delay;
    }
    return 0;
}

bool QWeather::isCurrentJwtTokenNeedsRefresh(const qint64 expireOffset) const
{
    if (!currentToken.isEmpty()) {
        const QRegularExpression tokenRegex(QStringLiteral("(.+)\\.(.+)\\.(.+)"));
        const QRegularExpressionMatch tokenMatch = tokenRegex.match(currentToken);
        qint64 expireTimestamp = 0;
        if (tokenMatch.hasCaptured(2)) {
            const QJsonDocument payload = QJsonDocument::fromJson(QByteArray::fromBase64(tokenMatch.captured(2).toUtf8(), base64Options));
            bool ok;
            const qint64 expireTimestampValue = payload[QStringLiteral("exp")].toString().toLong(&ok);
            expireTimestamp = ok ? expireTimestampValue : expireTimestamp;
        }
        return expireTimestamp + expireOffset - QDateTime::currentSecsSinceEpoch() <= 0;
    }
    return true;
}

void QWeather::onNetworkError()
{
    if (retryTimes < (quint8)0xff)
        retryTimes++;
    else
        qWarning(WEATHER::ION::QWEATHER) << "Too many errors";
}

const QJsonObject QWeather::extractResponse(QNetworkReply *reply)
{
    // Check https://dev.qweather.com/docs/resource/error-code/ for more info.
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        const int responseCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        if (responseCode != 200 && contentType == QStringLiteral("application/problem+json")) {
            qDebug(WEATHER::ION::QWEATHER) << "Found v2 error response.";
            switch (responseCode) {
            case 404:
                qWarning(WEATHER::ION::QWEATHER) << "Unable to find resource" << reply->url();
                break;
            case 405:
                qWarning(WEATHER::ION::QWEATHER) << "Invalid operation" << reply->operation();
                break;
            default: {
                const QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
                const QUrl type(response[QStringLiteral("error")][QStringLiteral("type")].toString());
                qWarning(WEATHER::ION::QWEATHER) << "Got an error from api, check" << type << "for more info.";
                const QJsonValue invalidParams = response[QStringLiteral("invalidParams")];
                if (invalidParams.isArray())
                    qWarning(WEATHER::ION::QWEATHER) << "Invalid params:" << invalidParams.toArray();
                break;
            }
            }
        } else if (responseCode == 200 && contentType == QStringLiteral("application/json")) {
            const QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
            const QString codeKeyName = QStringLiteral("code");
            if (response.contains(codeKeyName)) {
                QJsonValue errorCodeValue = response[codeKeyName];
                qDebug(WEATHER::ION::QWEATHER) << "Found v1 error code" << errorCodeValue;
                const int errorCode = errorCodeValue.toString().toInt();
                switch (errorCode) {
                case 200: {
                    QJsonObject ret(response);
                    ret.remove(codeKeyName);
                    return ret;
                }
                default:
                    qWarning(WEATHER::ION::QWEATHER) << "Found response code" << errorCode;
                    qWarning(WEATHER::ION::QWEATHER) << "Check https://dev.qweather.com/docs/resource/error-code/#error-code-v1 for more info.";
                    break;
                }
            } else if (!response.isEmpty()) {
                return response;
            } else {
                qWarning(WEATHER::ION::QWEATHER) << "Got empty response.";
            }
        } else {
            qWarning(WEATHER::ION::QWEATHER) << "Found unknown response code" << responseCode << "with content-type" << contentType;
        }
        reply->deleteLater();
    }
    return QJsonObject();
}

Ion::ConditionIcons QWeather::getWeatherIcon(const QString &icon, const bool windy) const
{
    // https://dev.qweather.com/docs/resource/icons/#weather-icons
    const QHash<QString, Ion::ConditionIcons> conditionsMap({
        {QStringLiteral("100"), windy ? Ion::ClearWindyDay : Ion::ClearDay},
        {QStringLiteral("101"), windy ? Ion::PartlyCloudyWindyDay : Ion::PartlyCloudyDay},
        {QStringLiteral("102"), windy ? Ion::FewCloudsWindyDay : Ion::FewCloudsDay},
        {QStringLiteral("103"), windy ? Ion::ClearWindyDay : Ion::ClearDay},
        {QStringLiteral("104"), windy ? Ion::OvercastWindy : Ion::Overcast},

        {QStringLiteral("150"), windy ? Ion::ClearWindyNight : Ion::ClearNight},
        {QStringLiteral("151"), windy ? Ion::PartlyCloudyWindyNight : Ion::PartlyCloudyNight},
        {QStringLiteral("152"), windy ? Ion::FewCloudsWindyNight : Ion::FewCloudsNight},
        {QStringLiteral("153"), windy ? Ion::ClearWindyNight : Ion::ClearNight},

        {QStringLiteral("300"), Ion::ChanceShowersDay},
        {QStringLiteral("301"), Ion::ChanceShowersDay},
        {QStringLiteral("302"), Ion::Thunderstorm},
        {QStringLiteral("303"), Ion::Thunderstorm},
        {QStringLiteral("304"), Ion::Thunderstorm},
        {QStringLiteral("305"), Ion::LightRain},
        {QStringLiteral("306"), Ion::Rain},
        {QStringLiteral("307"), Ion::Rain},
        {QStringLiteral("308"), Ion::Rain},
        {QStringLiteral("309"), Ion::LightRain},
        {QStringLiteral("310"), Ion::Rain},
        {QStringLiteral("311"), Ion::Rain},
        {QStringLiteral("312"), Ion::Rain},
        {QStringLiteral("313"), Ion::FreezingRain},
        {QStringLiteral("314"), Ion::LightRain},
        {QStringLiteral("315"), Ion::Rain},
        {QStringLiteral("316"), Ion::Rain},
        {QStringLiteral("317"), Ion::Rain},
        {QStringLiteral("318"), Ion::Rain},

        {QStringLiteral("350"), Ion::ChanceShowersNight},
        {QStringLiteral("351"), Ion::ChanceShowersNight},
        {QStringLiteral("399"), Ion::Rain},

        {QStringLiteral("400"), Ion::LightSnow},
        {QStringLiteral("401"), Ion::Snow},
        {QStringLiteral("402"), Ion::Snow},
        {QStringLiteral("403"), Ion::Snow},
        {QStringLiteral("404"), Ion::RainSnow},
        {QStringLiteral("405"), Ion::RainSnow},
        {QStringLiteral("406"), Ion::RainSnow},
        {QStringLiteral("407"), Ion::ChanceSnowDay},
        {QStringLiteral("408"), Ion::LightSnow},
        {QStringLiteral("409"), Ion::Snow},
        {QStringLiteral("410"), Ion::Snow},

        {QStringLiteral("456"), Ion::RainSnow},
        {QStringLiteral("457"), Ion::ChanceSnowNight},
        {QStringLiteral("499"), Ion::Snow},

        {QStringLiteral("500"), Ion::Mist},
        {QStringLiteral("501"), Ion::Mist},
        {QStringLiteral("502"), Ion::Haze},
        {QStringLiteral("503"), Ion::Haze},
        {QStringLiteral("504"), Ion::Haze},
        {QStringLiteral("507"), Ion::Haze},
        {QStringLiteral("508"), Ion::Haze},
        {QStringLiteral("509"), Ion::Mist},
        {QStringLiteral("510"), Ion::Mist},
        {QStringLiteral("511"), Ion::Haze},
        {QStringLiteral("512"), Ion::Haze},
        {QStringLiteral("513"), Ion::Haze},
        {QStringLiteral("514"), Ion::Mist},
        {QStringLiteral("515"), Ion::Mist},
        {QStringLiteral("999"), Ion::NotAvailable},
    });
    return conditionsMap.value(icon, conditionsMap.values().last());
}

Warnings::PriorityClass QWeather::getPriority(const QString &severity) const
{
    // https://dev.qweather.com/docs/resource/warning-info/#severity
    if (severity == QStringLiteral("extreme"))
        return Warnings::Extreme;
    else if (severity == QStringLiteral("severe"))
        return Warnings::High;
    else if (severity == QStringLiteral("moderate"))
        return Warnings::Medium;
    else // "minor" / "unknown"
        return Warnings::Low;
}

bool QWeather::fillCurrentWeather(std::shared_ptr<Forecast> forecast, const QJsonObject &currentResponse)
{
    const bool windy = currentResponse[QStringLiteral("wind")][QStringLiteral("scale")].toInt() > 2;
    LastObservation lo;
    lo.setCurrentConditions(currentResponse[QStringLiteral("condition")][QStringLiteral("text")].toString());
    lo.setConditionIcon(Ion::getWeatherIcon(getWeatherIcon(currentResponse[QStringLiteral("condition")][QStringLiteral("code")].toString(), windy)));
    lo.setTemperature(currentResponse[QStringLiteral("temperature")][QStringLiteral("value")].toDouble());
    lo.setWindSpeed(currentResponse[QStringLiteral("wind")][QStringLiteral("speed")][QStringLiteral("value")].toDouble());
    lo.setWindGust(currentResponse[QStringLiteral("windGust")][QStringLiteral("value")].toDouble());
    lo.setWindDirection(currentResponse[QStringLiteral("wind")][QStringLiteral("direction")][QStringLiteral("compass")].toString().toUpper());
    lo.setVisibility(currentResponse[QStringLiteral("visibility")][QStringLiteral("value")].toDouble());
    lo.setPressure(currentResponse[QStringLiteral("pressure")][QStringLiteral("value")].toDouble());
    lo.setUVIndex(currentResponse[QStringLiteral("uvIndex")].toInt());
    lo.setHumidity(currentResponse[QStringLiteral("humidity")].toDouble() * 100);
    lo.setDewpoint(currentResponse[QStringLiteral("dewPoint")][QStringLiteral("value")].toDouble());
    forecast->setLastObservation(lo);
    return lo.currentConditions().isValid() && lo.conditionIcon().isValid() && lo.temperature().isValid();
}

bool QWeather::fillDailyWeather(std::shared_ptr<Forecast> forecast, const QJsonObject &dailyResponse)
{
    std::shared_ptr<FutureDays> fd = std::make_shared<FutureDays>();
    bool success = true;
    for (const QJsonValue day : dailyResponse[QStringLiteral("days")].toArray()) {
        FutureDayForecast fdf;
        FutureForecast daytime, nighttime;
        const bool daytimeWindy = day[QStringLiteral("daytime")][QStringLiteral("wind")][QStringLiteral("scale")].toInt() > 2,
                   nighttimeWindy = day[QStringLiteral("nighttime")][QStringLiteral("wind")][QStringLiteral("scale")].toInt() > 2;
        daytime.setConditionIcon(
            Ion::getWeatherIcon(getWeatherIcon(day[QStringLiteral("daytime")][QStringLiteral("condition")][QStringLiteral("code")].toString(), daytimeWindy)));
        daytime.setCondition(day[QStringLiteral("daytime")][QStringLiteral("condition")][QStringLiteral("text")].toString());
        daytime.setHighTemp(day[QStringLiteral("daytime")][QStringLiteral("temperatureMax")][QStringLiteral("value")].toDouble());
        daytime.setLowTemp(day[QStringLiteral("daytime")][QStringLiteral("temperatureMin")][QStringLiteral("value")].toDouble());
        if (day[QStringLiteral("daytime")][QStringLiteral("precipitation")][QStringLiteral("amount")][QStringLiteral("value")].toDouble() > 0) {
            const double precipitationProbability =
                day[QStringLiteral("daytime")][QStringLiteral("precipitation")][QStringLiteral("probability")].toDouble() * 100;
            daytime.setConditionProbability(precipitationProbability);
        }
        success = success && !daytime.condition().value().isEmpty();
        if (!success)
            break;
        fdf.setDaytime(daytime);
        nighttime.setConditionIcon(Ion::getWeatherIcon(
            getWeatherIcon(day[QStringLiteral("nighttime")][QStringLiteral("condition")][QStringLiteral("code")].toString(), nighttimeWindy)));
        nighttime.setCondition(day[QStringLiteral("nighttime")][QStringLiteral("condition")][QStringLiteral("text")].toString());
        nighttime.setHighTemp(day[QStringLiteral("nighttime")][QStringLiteral("temperatureMax")][QStringLiteral("value")].toDouble());
        nighttime.setLowTemp(day[QStringLiteral("nighttime")][QStringLiteral("temperatureMin")][QStringLiteral("value")].toDouble());
        if (day[QStringLiteral("nighttime")][QStringLiteral("precipitation")][QStringLiteral("amount")][QStringLiteral("value")].toDouble() > 0) {
            const double precipitationProbability =
                day[QStringLiteral("nighttime")][QStringLiteral("precipitation")][QStringLiteral("probability")].toDouble() * 100;
            nighttime.setConditionProbability(precipitationProbability);
        }
        success = success && !nighttime.condition().value().isEmpty();
        if (!success)
            break;
        fdf.setNight(nighttime);
        const QDateTime forecastTime = QDateTime::fromString(day[QStringLiteral("forecastStartTime")].toString(), dateFormat).toLocalTime();
        qDebug(WEATHER::ION::QWEATHER) << "Got future forecast for" << forecastTime;
        fdf.setMonthDay(forecastTime.date().day());
        fdf.setWeekDay(QLocale().dayName(forecastTime.date().dayOfWeek()));
        success = success && fdf.monthDay().value() > 0;
        if (!success)
            break;
        fd->addDay(fdf);
    }
    if (success) {
        forecast->setFutureDays(fd);
    }
    return success;
}

bool QWeather::fillWarnings(std::shared_ptr<Forecast> forecast, const QJsonObject &warningsResponse, const QString &warningUrl)
{
    std::shared_ptr<Warnings> warnings = std::make_shared<Warnings>();
    bool success = true;
    QMap<const QString, Warning> merged;
    for (const QJsonValue warning : warningsResponse[QStringLiteral("alerts")].toArray()) {
        const QString id = warning[QStringLiteral("id")].toString();
        if (!merged.contains(id)) {
            const QString timestamp =
                QLocale().toString(QDateTime::fromString(warning[QStringLiteral("issuedTime")].toString(), dateFormat).toLocalTime(), warningTimestampFormat);
            Warning w(getPriority(warning[QStringLiteral("severity")].toString()), warning[QStringLiteral("headline")].toString());
            w.setInfo(warningUrl);
            w.setTimestamp(timestamp);
            merged.insert(id, w);
            const QJsonArray supersedes = warning[QStringLiteral("messageType")][QStringLiteral("supersedes")].toArray();
            if (!supersedes.isEmpty()) {
                for (const QJsonValue supersede : supersedes) {
                    merged.remove(supersede.toString());
                }
            }
        } else {
            qWarning(WEATHER::ION::QWEATHER) << "Id" << id << "is found more than once.";
        }
    }
    for (const Warning &w : merged.values()) {
        success = success && !w.timestamp().value().isEmpty() && !w.description().isEmpty() && !w.info().value().isEmpty();
        if (!success)
            break;
        warnings->addWarning(w);
    }
    if (success)
        forecast->setWarnings(warnings);
    return success;
}

const QByteArray QWeather::signMessage(const QString &message) const
{
    unsigned char publicKey[crypto_sign_ed25519_PUBLICKEYBYTES];
    unsigned char secretKey[crypto_sign_ed25519_SECRETKEYBYTES];
    crypto_sign_ed25519_seed_keypair(publicKey, secretKey, PRIVATE_KEY_SEED);
    unsigned char signature[crypto_sign_ed25519_BYTES];
    unsigned long long signatureSize;
    const QByteArray messageData = message.toUtf8();
    crypto_sign_ed25519_detached(signature, &signatureSize, reinterpret_cast<const unsigned char *>(messageData.constData()), messageData.size(), secretKey);
    return QByteArray(reinterpret_cast<const char *>(signature), signatureSize);
}

#include "qweather.moc"
