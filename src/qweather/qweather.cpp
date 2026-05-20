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

#include "qweather.hpp"
#include "qweather_debug.hpp"

K_PLUGIN_CLASS_WITH_JSON(QWeather, "metadata.json");

QWeather::QWeather(QObject *parent)
    : Ion(parent)
    , networkAccessManager(this)
{
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
                Station toBeSerialized;
                toBeSerialized.setRegion(region);
                toBeSerialized.setPlace(place);
                toBeSerialized.setStation(station);
                toBeSerialized.setCountry(country);
                toBeSerialized.setCoordinates(lat, lon);
                toBeSerialized.setNewPlaceInfo(locationId);
                Location toBeAdded;
                toBeAdded.setDisplayName(displayName);
                toBeAdded.setPlaceInfo(serialize(toBeSerialized));
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
    const Station deserialized = deserialize(placeInfo);

    QUrlQuery nowQuery;
    nowQuery.addQueryItem(QStringLiteral("location"), deserialized.newPlaceInfo().toString());
    const QNetworkRequest nowRequest = makeApiRequest(QStringLiteral("/v7/weather/now"), nowQuery);
    QNetworkReply *nowReply = networkAccessManager.get(nowRequest);
    QFuture<void> nowFinished = QtFuture::connect(nowReply, &QNetworkReply::finished);

    QUrlQuery daysQuery;
    daysQuery.addQueryItem(QStringLiteral("location"), deserialized.newPlaceInfo().toString());
    const QNetworkRequest daysRequest = makeApiRequest(QStringLiteral("/v7/weather/7d"), daysQuery);
    QNetworkReply *daysReply = networkAccessManager.get(daysRequest);
    QFuture<void> daysFinished = QtFuture::connect(daysReply, &QNetworkReply::finished);

    const QString warningRequestPath =
        QStringLiteral("/weatheralert/v1/current/%1/%2").arg(deserialized.latitude().toDouble(), 0, 'f', 2).arg(deserialized.longitude().toDouble(), 0, 'f', 2);
    const QNetworkRequest warningRequest = makeApiRequest(warningRequestPath);
    QNetworkReply *warningReply = networkAccessManager.get(warningRequest);
    QFuture<void> warningFinished = QtFuture::connect(warningReply, &QNetworkReply::finished);

    QUrlQuery indexQuery;
    indexQuery.addQueryItem(QStringLiteral("location"), deserialized.newPlaceInfo().toString());
    indexQuery.addQueryItem(QStringLiteral("type"), QString::number(IndexType::UV));
    const QNetworkRequest indexRequest = makeApiRequest(QStringLiteral("/v7/indices/1d"), indexQuery);
    QNetworkReply *indexReply = networkAccessManager.get(indexRequest);
    QFuture<void> indexFinished = QtFuture::connect(indexReply, &QNetworkReply::finished);

    QFuture<void> everythingFinished = QtFuture::whenAll(nowFinished, daysFinished, warningFinished, indexFinished);
    everythingFinished.then(this, [=]() {
        const QJsonObject nowResponse = extractResponse(nowReply);
        qDebug(WEATHER::ION::QWEATHER) << "Got now response" << nowResponse;
        if (nowResponse.isEmpty()) {
            promise->finish();
            onNetworkError();
            return;
        }
        const QJsonObject daysResponse = extractResponse(daysReply);
        qDebug(WEATHER::ION::QWEATHER) << "Got days response" << daysResponse;
        if (daysResponse.isEmpty()) {
            promise->finish();
            onNetworkError();
            return;
        }
        const QJsonObject warningResponse = extractResponse(warningReply);
        qDebug(WEATHER::ION::QWEATHER) << "Got warning response" << warningResponse;
        if (warningResponse.isEmpty()) {
            promise->finish();
            onNetworkError();
            return;
        }
        const QJsonObject indexResponse = extractResponse(indexReply);
        qDebug(WEATHER::ION::QWEATHER) << "Got index response" << indexResponse;
        if (indexResponse.isEmpty()) {
            promise->finish();
            onNetworkError();
            return;
        }
        std::shared_ptr<Forecast> forecast = std::make_shared<Forecast>();
        QString credit;
        const QJsonArray sources = nowResponse[QStringLiteral("refer")][QStringLiteral("sources")].toArray();
        if (!sources.isEmpty()) {
            const int count = sources.count();
            for (int i = 0; i < count; i++) {
                credit += sources.at(i).toString();
                if (i != count - 1)
                    credit += QStringLiteral(", ");
            }
        } else {
            credit = QStringLiteral("QWeather");
        }
        forecast->setMetadata(getMetaData(i18n("Source: %1", credit), nowResponse[QStringLiteral("fxLink")].toString()));
        forecast->setStation(stripNewPlaceInfo(deserialized));
        forecast->setLastObservation(getLastObservation(nowResponse, getIndexValue(indexResponse, IndexType::UV, -1)));
        std::shared_ptr<FutureDays> futureDays = std::make_shared<FutureDays>();
        updateFutureDays(futureDays, daysResponse);
        forecast->setFutureDays(futureDays);
        std::shared_ptr<Warnings> warnings = std::make_shared<Warnings>();
        updateWarnings(warnings, warningResponse);
        forecast->setWarnings(warnings);
        promise->addResult(forecast);
        promise->finish();
    });
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
    const QString signature = QString::fromUtf8(privateKey.signMessage(toBeSigned).toBase64(base64Options));
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

const MetaData QWeather::getMetaData(const QString &credit, const QString &creditUrl)
{
    MetaData metaData;
    metaData.setCredit(credit);
    metaData.setCreditURL(creditUrl);
    metaData.setTemperatureUnit(temperatureUnit);
    metaData.setWindSpeedUnit(windSpeedUnit);
    metaData.setVisibilityUnit(visibilityUnit);
    metaData.setPressureUnit(pressureUnit);
    metaData.setHumidityUnit(humidityUnit);
    metaData.setRainfallUnit(preciptionUnit);
    metaData.setSnowfallUnit(preciptionUnit);
    metaData.setPrecipUnit(preciptionUnit);
    return metaData;
}

int QWeather::getIndexValue(const QJsonObject &indexResponse, const IndexType indexType, int defaultValue)
{
    for (const QJsonValue &index : indexResponse[QStringLiteral("daily")].toArray()) {
        const int type = index[QStringLiteral("type")].toString().toInt();
        if (type == indexType) {
            bool ok;
            const int level = index[QStringLiteral("level")].toString().toInt(&ok);
            return ok ? level : defaultValue;
        }
    }
    return defaultValue;
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

void QWeather::updateFutureDays(std::shared_ptr<FutureDays> futureDays, const QJsonObject &futureDaysResponse)
{
    if (futureDays == nullptr) {
        qWarning(WEATHER::ION::QWEATHER) << "futureDays is nullptr.";
        return;
    }
    QLocale locale;
    for (const QJsonValue &daily : futureDaysResponse[QStringLiteral("daily")].toArray()) {
        FutureDayForecast forecast;
        FutureForecast daytime, night;
        const QDate fxDate = QDate::fromString(daily[QStringLiteral("fxDate")].toString(), Qt::ISODate);
        qDebug(WEATHER::ION::QWEATHER) << "Adding forecast for date" << fxDate;
        qDebug(WEATHER::ION::QWEATHER) << "Raw date is" << daily[QStringLiteral("fxDate")];
        qDebug(WEATHER::ION::QWEATHER) << "Days in month is" << fxDate.daysInMonth() << "; Day of week is" << fxDate.dayOfWeek() << "; Localized day of week is"
                                       << locale.dayName(fxDate.dayOfWeek());
        const int windScaleDay = daily[QStringLiteral("windScaleDay")].toString().split(QStringLiteral("-")).last().toInt();
        const int windScaleNight = daily[QStringLiteral("windScaleNight")].toString().split(QStringLiteral("-")).last().toInt();
        forecast.setMonthDay(fxDate.day());
        forecast.setWeekDay(locale.dayName(fxDate.dayOfWeek()));
        daytime.setConditionIcon(Ion::getWeatherIcon(getWeatherIcon(daily[QStringLiteral("iconDay")].toString(), windScaleDay >= 2)));
        daytime.setCondition(daily[QStringLiteral("textDay")].toString());
        daytime.setHighTemp(daily[QStringLiteral("tempMax")].toString().toDouble());
        forecast.setDaytime(daytime);
        night.setConditionIcon(Ion::getWeatherIcon(getWeatherIcon(daily[QStringLiteral("iconNight")].toString(), windScaleNight >= 2)));
        night.setCondition(daily[QStringLiteral("textNight")].toString());
        night.setLowTemp(daily[QStringLiteral("tempMin")].toString().toDouble());
        forecast.setNight(night);
        futureDays->addDay(forecast);
    }
}

void QWeather::updateWarnings(std::shared_ptr<Warnings> warnings, const QJsonObject &warningsResponse)
{
    if (warnings == nullptr) {
        qWarning(WEATHER::ION::QWEATHER) << "warnings is nullptr.";
        return;
    }
    for (const QJsonValue &alert : warningsResponse[QStringLiteral("alerts")].toArray()) {
        const QDateTime issuedTime = QDateTime::fromString(alert[QStringLiteral("issuedTime")].toString(), Qt::ISODate);
        const QString description = alert[QStringLiteral("headline")].toString();
        const QString info = alert[QStringLiteral("description")].toString();
        Warning warning(getPriority(alert[QStringLiteral("severity")].toString()), description);
        warning.setInfo(info);
        warning.setTimestamp(issuedTime.toString());
        warnings->addWarning(warning);
    }
}

const LastObservation QWeather::getLastObservation(const QJsonObject &response, const int uvIndex) const
{
    const int windScale = response[QStringLiteral("now")][QStringLiteral("windScale")].toString().split(QStringLiteral("-")).last().toInt();
    const double temperature = response[QStringLiteral("now")][QStringLiteral("temp")].toString().toDouble();
    const double windSpeed = response[QStringLiteral("now")][QStringLiteral("windSpeed")].toString().toDouble();
    const int humidity = response[QStringLiteral("now")][QStringLiteral("humidity")].toString().toInt();
    bool ok;
    double dewpoint = response[QStringLiteral("now")][QStringLiteral("dew")].toString().toDouble(&ok);
    dewpoint = ok ? dewpoint : getDewpoint(temperature, humidity);
    const qreal humidex = getHumidex(temperature, dewpoint);
    const qreal visibility = response[QStringLiteral("now")][QStringLiteral("vis")].toString().toDouble();
    LastObservation ret;
    ret.setObservationTimestamp(QDateTime::fromString(response[QStringLiteral("now")][QStringLiteral("obsTime")].toString(), Qt::ISODate));
    ret.setCurrentConditions(response[QStringLiteral("now")][QStringLiteral("text")].toString());
    ret.setConditionIcon(Ion::getWeatherIcon(getWeatherIcon(response[QStringLiteral("now")][QStringLiteral("icon")].toString(), windScale >= 2)));
    ret.setTemperature(temperature);
    ret.setWindchill(getWindChill(temperature, windSpeed));
    ret.setHeatIndex(getHeatIndexFromHumidity(temperature, humidity));
    ret.setHumidex(humidex);
    ret.setHumidex(getHumidex(humidex));
    ret.setWindSpeed(windSpeed);
    ret.setWindDirection(getWindDirectionIcon(getWindDirectionIcon(response[QStringLiteral("now")][QStringLiteral("wind360")].toString().toDouble())));
    ret.setVisibility(visibility);
    ret.setVisibility(getVisibility(visibility));
    ret.setPressure(response[QStringLiteral("now")][QStringLiteral("pressure")].toString().toDouble());
    if (uvIndex >= 0)
        ret.setUVIndex(uvIndex);
    ret.setHumidity(humidity);
    ret.setDewpoint(dewpoint);
    return ret;
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
// TODO: Calculating instead comparing.
Ion::WindDirections QWeather::getWindDirectionIcon(const double degree) const
{
    const double unit = 360.0 / 16;
    if (degree < unit * 0) {
        qWarning(WEATHER::ION::QWEATHER) << "Invalid degree" << degree;
        return Ion::VR;
    } else if (degree < unit * 0 + unit / 2) {
        return Ion::N;
    } else if (degree < unit * 1 + unit / 2) {
        return Ion::NNE;
    } else if (degree < unit * 2 + unit / 2) {
        return Ion::NE;
    } else if (degree < unit * 3 + unit / 2) {
        return Ion::ENE;
    } else if (degree < unit * 4 + unit / 2) {
        return Ion::E;
    } else if (degree < unit * 5 + unit / 2) {
        return Ion::ESE;
    } else if (degree < unit * 6 + unit / 2) {
        return Ion::SE;
    } else if (degree < unit * 7 + unit / 2) {
        return Ion::SSE;
    } else if (degree < unit * 8 + unit / 2) {
        return Ion::S;
    } else if (degree < unit * 9 + unit / 2) {
        return Ion::SSW;
    } else if (degree < unit * 10 + unit / 2) {
        return Ion::SW;
    } else if (degree < unit * 11 + unit / 2) {
        return Ion::WSW;
    } else if (degree < unit * 12 + unit / 2) {
        return Ion::W;
    } else if (degree < unit * 13 + unit / 2) {
        return Ion::WNW;
    } else if (degree < unit * 14 + unit / 2) {
        return Ion::NW;
    } else if (degree < unit * 15 + unit / 2) {
        return Ion::NNW;
    } else if (degree < unit * 16) {
        return Ion::N;
    } else {
        qInfo(WEATHER::ION::QWEATHER) << "Found degree greater than 360:" << degree;
        return getWindDirectionIcon(std::fmod(degree, 360));
    }
}

// A simple copy of QString Ion::getWindDirectionIcon(const QMap<QString, WindDirections> &windDirList, const QString &windDirection) const
const QString QWeather::getWindDirectionIcon(const Ion::WindDirections windDirection) const
{
    switch (windDirection) {
    case Ion::N:
        return QStringLiteral("N");
    case Ion::NNE:
        return QStringLiteral("NNE");
    case Ion::NE:
        return QStringLiteral("NE");
    case Ion::ENE:
        return QStringLiteral("ENE");
    case Ion::E:
        return QStringLiteral("E");
    case Ion::SSE:
        return QStringLiteral("SSE");
    case Ion::SE:
        return QStringLiteral("SE");
    case Ion::ESE:
        return QStringLiteral("ESE");
    case Ion::S:
        return QStringLiteral("S");
    case Ion::NNW:
        return QStringLiteral("NNW");
    case Ion::NW:
        return QStringLiteral("NW");
    case Ion::WNW:
        return QStringLiteral("WNW");
    case Ion::W:
        return QStringLiteral("W");
    case Ion::SSW:
        return QStringLiteral("SSW");
    case Ion::SW:
        return QStringLiteral("SW");
    case Ion::WSW:
        return QStringLiteral("WSW");
    case Ion::VR:
        return QStringLiteral("VR"); // For now, we'll make a variable wind icon later on
    }

    // No icon available, use 'X'
    return QString();
}

qreal QWeather::getWindChill(const qreal temperature, const qreal windSpeed)
{
    const qreal temperatureNormalized = KUnitConversion::Value(temperature, temperatureUnit).convertTo(KUnitConversion::Celsius).number();
    const qreal windSpeedNormalized = KUnitConversion::Value(windSpeed, windSpeedUnit).convertTo(KUnitConversion::KilometerPerHour).number();
    // https://en.wikipedia.org/wiki/Wind_chill#North_American_and_United_Kingdom_wind_chill_index
    double pow = qPow(windSpeedNormalized, 0.16);
    return 13.12 + 0.6215 * temperatureNormalized - 11.37 * pow + 0.3965 * temperatureNormalized * pow;
}

qreal QWeather::getHeatIndexFromHumidity(const qreal temperature, const qreal humidity)
{
    const qreal temperatureNormalized = KUnitConversion::Value(temperature, temperatureUnit).convertTo(KUnitConversion::Celsius).number();
    const qreal humidityNormalized = KUnitConversion::Value(humidity, humidityUnit).convertTo(KUnitConversion::Percent).number();
    if (temperatureNormalized < 26.66 || humidityNormalized < 40) {
        qDebug(WEATHER::ION::QWEATHER) << "Temperature or Humidity is too low.";
        return 0;
    }
    // https://en.wikipedia.org/wiki/Heat_index#Formula
    const double c1 = -8.78469475556, c2 = 1.61139411, c3 = 2.33854883889, c4 = -0.14611605, c5 = -0.012308094, c6 = -0.0164248277778, c7 = 2.211732e-3,
                 c8 = 7.2546e-4, c9 = -3.582e-6;
    return c1 + c2 * temperatureNormalized + c3 * humidityNormalized + c4 * temperatureNormalized * humidityNormalized + c5 * qPow(temperatureNormalized, 2)
        + c6 * qPow(humidityNormalized, 2) + c7 + qPow(temperatureNormalized, 2) * humidityNormalized + c8 * temperatureNormalized * qPow(humidityNormalized, 2)
        + c9 * qPow(temperatureNormalized, 2) * qPow(humidityNormalized, 2);
}

const QString QWeather::getHumidex(const qreal humidexValue)
{
    // https://en.wikipedia.org/wiki/Humidex
    if (humidexValue <= 29)
        return i18n("Little to no discomfort");
    else if (humidexValue <= 39)
        return i18n("Some discomfort");
    else if (humidexValue <= 45)
        return i18n("Great discomfort; avoid exertion");
    else
        return i18n("Dangerous; heat stroke quite possible");
}

qreal QWeather::getHumidex(const qreal temperature, const qreal dewpoint)
{
    const qreal temperatureNormalized = KUnitConversion::Value(temperature, temperatureUnit).convertTo(KUnitConversion::Celsius).number();
    const qreal dewpointNormalized = KUnitConversion::Value(dewpoint, temperatureUnit).convertTo(KUnitConversion::Kelvin).number();
    // https://en.wikipedia.org/wiki/Humidex#Computation_formula
    const qreal e = 6.11 * qExp(5417.7530 * (1 / 273.16 - 1 / dewpointNormalized));
    return temperatureNormalized + 0.5555 * (e - 10.0);
}

qreal QWeather::getDewpoint(const qreal temperature, const qreal humidity)
{
    const qreal temperatureNormalized = KUnitConversion::Value(temperature, temperatureUnit).convertTo(KUnitConversion::Celsius).number();
    const qreal humidityNormalized = KUnitConversion::Value(humidity, humidityUnit).convertTo(KUnitConversion::Percent).number();
    // https://en.wikipedia.org/wiki/Dew_point#Calculating_the_dew_point
    const double b = 17.625, c = 243.04;
    const qreal gramma = qLn(humidityNormalized / 100) + b * temperatureNormalized / (c + temperatureNormalized);
    return c * gramma / (b - gramma);
}

Warnings::PriorityClass QWeather::getPriority(const QString &severity)
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

const QString QWeather::getVisibility(const qreal visibilityValue)
{
    const qreal visibilityValueNormalized = KUnitConversion::Value(visibilityValue, visibilityUnit).convertTo(KUnitConversion::Meter).number();
    // https://www.cma.gov.cn/zfxxgk/gknr/flfgbz/bz/202209/P020220921580375165432.pdf
    if (visibilityValueNormalized < 50)
        return i18n("Extremely Poor");
    else if (visibilityValueNormalized < 500)
        return i18n("Poor");
    else if (visibilityValueNormalized < 1000)
        return i18n("Relatively Poor");
    else if (visibilityValueNormalized < 2000)
        return i18n("General");
    else if (visibilityValueNormalized < 10000)
        return i18n("Good");
    else
        return i18n("Excellent");
}

const QString serialize(const Station &station)
{
    QJsonObject json;
    json[QStringLiteral("station")] = QJsonValue::fromVariant(station.station());
    json[QStringLiteral("place")] = QJsonValue::fromVariant(station.place());
    json[QStringLiteral("region")] = QJsonValue::fromVariant(station.region());
    json[QStringLiteral("country")] = QJsonValue::fromVariant(station.country());
    json[QStringLiteral("latitude")] = QJsonValue::fromVariant(station.latitude());
    json[QStringLiteral("longitude")] = QJsonValue::fromVariant(station.longitude());
    json[QStringLiteral("placeInfo")] = QJsonValue::fromVariant(station.newPlaceInfo());
    return QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

const Station deserialize(const QString &serialized)
{
    const QJsonDocument json = QJsonDocument::fromJson(serialized.toUtf8());
    Station station;
    station.setStation(json[QStringLiteral("station")].toString());
    station.setPlace(json[QStringLiteral("place")].toString());
    station.setRegion(json[QStringLiteral("region")].toString());
    station.setCountry(json[QStringLiteral("country")].toString());
    station.setCoordinates(json[QStringLiteral("latitude")].toDouble(), json[QStringLiteral("longitude")].toDouble());
    station.setNewPlaceInfo(json[QStringLiteral("placeInfo")].toString());
    return station;
}

const Station stripNewPlaceInfo(const Station &station)
{
    Station ret;
    ret.setStation(station.station().toString());
    ret.setPlace(station.place().toString());
    ret.setRegion(station.region().toString());
    ret.setCountry(station.country().toString());
    ret.setCoordinates(station.latitude().toDouble(), station.longitude().toDouble());
    return ret;
}

#include "qweather.moc"
