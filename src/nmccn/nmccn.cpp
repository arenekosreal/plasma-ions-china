#include <KLocalizedString>
#include <KPluginFactory>
#include <KUnitConversion/Unit>
#include <QUrlQuery>

#include "nmccn.hpp"
#include "nmccn_debug.hpp"

K_PLUGIN_CLASS_WITH_JSON(NmcCn, "metadata.json");

NmcCn::NmcCn(QObject *parent)
    : Ion(parent)
    , networkAccessManager(this)
{
}

NmcCn::~NmcCn()
{
    networkAccessManager.clearAccessCache();
}

void NmcCn::findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &searchString)
{
    promise->start();
    if (!promise->isCanceled()) {
        qDebug(WEATHER::ION::NMCCN) << "Finding place:" << searchString;
        connect(
            &networkAccessManager,
            &QNetworkAccessManager::finished,
            this,
            [=](QNetworkReply *reply) {
                if (!promise->isCanceled()) {
                    const QJsonValue value = this->extractApiResponse(reply);
                    qDebug(WEATHER::ION::NMCCN) << "Got data:" << value;
                    if (value.isArray()) {
                        Q_EMIT this->foundPlaces(value.toArray());
                    } else {
                        qWarning(WEATHER::ION::NMCCN) << "Response is mailformed. Wants array, is" << value.type();
                    }
                } else {
                    qDebug(WEATHER::ION::NMCCN) << "Search request has been cancelled.";
                }
                promise->finish();
            },
            (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
        connect(
            this,
            &NmcCn::foundPlaces,
            this,
            [=](const QJsonArray &array) {
                std::shared_ptr<Locations> locations = std::make_shared<Locations>();
                for (const QJsonValue &resultValue : array) {
                    // searchResult:
                    // id|city|province|refererPath|lon|lat
                    const QString searchResult = resultValue.toString();
                    const QStringList searchResultParts = searchResult.split(placeInfoSep);
                    const QString placeId = searchResultParts[0];
                    const QString placeDisplayName = QStringLiteral("%1-%2").arg(searchResultParts[2]).arg(searchResultParts[1]);
                    bool lonOk, latOk;
                    const qreal lon = searchResultParts[4].toDouble(&lonOk), lat = searchResultParts[5].toDouble(&latOk);
                    Location location;
                    location.setStation(placeId);
                    location.setCode(placeId);
                    location.setDisplayName(placeDisplayName);
                    location.setPlaceInfo(searchResult);
                    if (lonOk && latOk) {
                        location.setCoordinates(QPointF(lat, lon));
                    }
                    qDebug(WEATHER::ION::NMCCN) << "Adding location" << location.displayName() << "from search string:" << searchResult;
                    locations->addLocation(location);
                }
                promise->addResult(locations);
            },
            (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
        findPlaces(searchString);
    } else {
        qDebug(WEATHER::ION::NMCCN) << "Search request has been cancelled.";
        promise->finish();
    }
}

void NmcCn::fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo)
{
    promise->start();
    if (!promise->isCanceled()) {
        qDebug(WEATHER::ION::NMCCN) << "Fetching forecast for place info" << placeInfo;
        const QStringList placeInfoParts = placeInfo.split(placeInfoSep);
        if (placeInfoParts.count() >= 6) {
            connect(
                &networkAccessManager,
                &QNetworkAccessManager::finished,
                this,
                [=](QNetworkReply *reply) {
                    if (!promise->isCanceled()) {
                        const QJsonValue value = this->extractApiResponse(reply);
                        qDebug(WEATHER::ION::NMCCN) << "Got data:" << value;
                        if (value.isObject()) {
                            Q_EMIT this->fetchedForecast(value.toObject());
                        } else {
                            qWarning(WEATHER::ION::NMCCN) << "Response is mailformed. Wants object, is" << value.type();
                        }
                    } else {
                        qDebug(WEATHER::ION::NMCCN) << "Fetch request has been cancelled.";
                    }
                    promise->finish();
                },
                (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
            const QString stationId = placeInfoParts[0], referer = apiBase + QStringLiteral("/publish/forecast") + placeInfoParts[3];
            qDebug(WEATHER::ION::NMCCN) << "Referer path for station" << stationId << "is" << placeInfoParts[3];
            connect(
                this,
                &NmcCn::fetchedForecast,
                this,
                [=](const QJsonObject &object) {
                    std::shared_ptr<Forecast> forecast = std::make_shared<Forecast>();

                    MetaData metaData;
                    metaData.setCredit(i18n("Source: National Meteorological Center of China"));
                    metaData.setCreditURL(referer);
                    metaData.setTemperatureUnit(KUnitConversion::Celsius);
                    metaData.setWindSpeedUnit(KUnitConversion::MeterPerSecond);
                    metaData.setPressureUnit(KUnitConversion::Hectopascal);
                    metaData.setRainfallUnit(KUnitConversion::Millimeter);
                    forecast->setMetadata(metaData);

                    Station station;
                    station.setStation(object[QStringLiteral("real")][QStringLiteral("station")][QStringLiteral("city")].toString());
                    station.setPlace(object[QStringLiteral("real")][QStringLiteral("station")][QStringLiteral("city")].toString());
                    station.setRegion(object[QStringLiteral("real")][QStringLiteral("station")][QStringLiteral("province")].toString());
                    station.setCountry(i18n("China"));
                    bool lonOk, latOk;
                    const qreal lon = placeInfoParts[4].toDouble(&lonOk), lat = placeInfoParts[5].toDouble(&latOk);
                    if (lonOk && latOk) {
                        station.setCoordinates(lon, lat);
                    }
                    forecast->setStation(station);

                    LastDay lastDay;
                    LastObservation lastObservation;
                    const QJsonArray passedchart = object[QStringLiteral("passedchart")].toArray();
                    if (passedchart.count() > 0) {
                        for (const QJsonValue &value : passedchart) {
                            const qreal temperature = value[QStringLiteral("temperature")].toDouble();
                            if (temperature > lastDay.normalHighTemp().toDouble()) {
                                lastDay.setNormalHighTemp(temperature);
                            }
                            if (temperature < lastDay.normalLowTemp().toDouble()) {
                                lastDay.setNormalLowTemp(temperature);
                            }
                        }
                        const qreal pressure = passedchart.first()[QStringLiteral("pressure")].toDouble();
                        lastObservation.setPressure(pressure);
                    }
                    lastObservation.setObservationTimestamp(QDateTime::fromString(object[QStringLiteral("real")][QStringLiteral("publish_time")].toString(), fullTimeFormat));
                    const QJsonObject weather = object[QStringLiteral("real")][QStringLiteral("weather")].toObject();
                    lastObservation.setCurrentConditions(weather[QStringLiteral("info")].toString());
                    const QDateTime sunrise = QDateTime::fromString(object[QStringLiteral("real")][QStringLiteral("sunriseSunset")][QStringLiteral("sunrise")].toString(), fullTimeFormat),
                                    sunset = QDateTime::fromString(object[QStringLiteral("real")][QStringLiteral("sunriseSunset")][QStringLiteral("sunset")].toString(), fullTimeFormat),
                                    now = QDateTime::currentDateTime();
                    const qreal windSpeed = object[QStringLiteral("real")][QStringLiteral("wind")][QStringLiteral("speed")].toDouble();
                    const bool currentIsNight = sunset <= now || now < sunrise,
                               currentIsWindy = windSpeed > 1.6; // wind faster than 1.6m/s means windy(to human)
                    const ConditionIcons conditionIcon =
                        this->getWeatherConditionIcon(weather[QStringLiteral("img")].toString(), currentIsWindy, currentIsNight);
                    lastObservation.setConditionIcon(this->getWeatherIcon(conditionIcon));
                    lastObservation.setTemperature(weather[QStringLiteral("temperature")].toDouble());
                    lastObservation.setWindchill(weather[QStringLiteral("feelst")].toDouble());
                    lastObservation.setHumidex(weather[QStringLiteral("feelst")].toDouble());
                    lastObservation.setWindSpeed(windSpeed);
                    const QString windDirection = this->getWindDirection(this->getWindDirection(object[QStringLiteral("real")][QStringLiteral("wind")][QStringLiteral("degree")].toDouble()));
                    lastObservation.setWindDirection(windDirection);
                    lastObservation.setHumidity(weather[QStringLiteral("humidity")].toDouble());
                    forecast->setLastObservation(lastObservation);
                    forecast->setLastDay(lastDay);

                    std::shared_ptr<FutureDays> futureDays = std::make_shared<FutureDays>();
                    const QJsonArray details = object[QStringLiteral("predict")][QStringLiteral("detail")].toArray();
                    for (const QJsonValue &detail : details) {
                        FutureDayForecast futureDayForecast;
                        const QDate date = QDate::fromString(detail[QStringLiteral("date")].toString(), fullDateFormat);
                        const QString dayInfo = detail[QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("info")].toString(),
                                      dayImg = detail[QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("img")].toString(),
                                      dayTemperature = detail[QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("temperature")].toString();
                        if (dayInfo != invalidValue && dayImg != invalidValue && dayTemperature != invalidValue) {
                            qDebug(WEATHER::ION::NMCCN) << "Adding day detail" << detail[QStringLiteral("day")].toObject() << "for date" << date;
                            FutureForecast dayTime;
                            dayTime.setConditionIcon(this->getWeatherIcon(this->getWeatherConditionIcon(dayImg, false, false)));
                            dayTime.setCondition(dayInfo);
                            dayTime.setHighTemp(dayTemperature.toInt());
                            futureDayForecast.setDaytime(dayTime);
                        } else {
                            qWarning(WEATHER::ION::NMCCN) << "Skipping invalid day detail" << detail[QStringLiteral("day")].toObject() << "for date" << date;
                        }
                        const QString nightInfo = detail[QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("info")].toString(),
                                      nightImg = detail[QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("img")].toString(),
                                      nightTemperature = detail[QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("temperature")].toString();
                        if (nightInfo != invalidValue && nightImg != invalidValue && nightTemperature != invalidValue) {
                            qDebug(WEATHER::ION::NMCCN) << "Adding night detail" << detail[QStringLiteral("night")].toObject() << "for date" << date;
                            FutureForecast nightTime;
                            nightTime.setConditionIcon(this->getWeatherIcon(this->getWeatherConditionIcon(nightImg, false, true)));
                            nightTime.setCondition(nightInfo);
                            nightTime.setLowTemp(nightTemperature.toInt());
                            futureDayForecast.setNight(nightTime);
                        } else {
                            qWarning(WEATHER::ION::NMCCN)
                                << "Skipping invalid night detail" << detail[QStringLiteral("night")].toObject() << "for date" << date;
                        }
                        futureDays->addDay(futureDayForecast);
                    }
                    forecast->setFutureDays(futureDays);

                    std::shared_ptr<Warnings> warnings = std::make_shared<Warnings>();
                    const QString warnAlert = object[QStringLiteral("real")][QStringLiteral("warn")][QStringLiteral("alert")].toString(),
                                  warnSignallevel = object[QStringLiteral("real")][QStringLiteral("warn")][QStringLiteral("signallevel")].toString(),
                                  warnUrlPath = object[QStringLiteral("real")][QStringLiteral("warn")][QStringLiteral("url")].toString();
                    if (warnAlert != invalidValue && warnSignallevel != invalidValue && warnUrlPath != invalidValue) {
                        const QString warnUrl = apiBase + warnUrlPath;
                        Warning warning(this->getWarnPriority(warnSignallevel), warnAlert);
                        warning.setInfo(warnUrl);
                        warnings->addWarning(warning);
                    } else {
                        qInfo(WEATHER::ION::NMCCN) << "No valid warning object found.";
                        qDebug(WEATHER::ION::NMCCN) << "Ignoring invalid warning object" << object[QStringLiteral("real")][QStringLiteral("warn")].toObject();
                    }

                    promise->addResult(forecast);
                },
                (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
            fetchForecast(stationId, referer);
        } else {
            qWarning(WEATHER::ION::NMCCN) << "Re-search your city to fix invalid placeInfo" << placeInfo;
        }
    } else {
        qDebug(WEATHER::ION::NMCCN) << "Fetch request has been cancelled.";
        promise->finish();
    }
}

Warnings::PriorityClass NmcCn::getWarnPriority(const QString &signallevel) const
{
    const QMap<QString, Warnings::PriorityClass> prioritiesMap = {
        {QStringLiteral("蓝色"), Warnings::Low},
        {QStringLiteral("黄色"), Warnings::Medium},
        {QStringLiteral("橙色"), Warnings::High},
        {QStringLiteral("红色"), Warnings::Extreme},
    };
    return prioritiesMap.value(signallevel, Warnings::Low);
}

QJsonValue NmcCn::extractApiResponse(QNetworkReply *reply) const
{
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        qDebug(WEATHER::ION::NMCCN) << "Request successfully.";
        QJsonParseError error;
        const QJsonDocument response = QJsonDocument::fromJson(reply->readAll(), &error);
        if (response.isObject()) {
            const QJsonObject responseObj = response.object();
            switch (responseObj[QStringLiteral("code")].toInt(-1)) {
            case 0:
                return responseObj[QStringLiteral("data")];
            case -1:
                qWarning(WEATHER::ION::NMCCN) << "Failed to parse response at" << error.offset << "because" << error.errorString();
                break;
            default:
                qWarning(WEATHER::ION::NMCCN) << "Server responded a failure:" << responseObj[QStringLiteral("msg")].toString();
            }
        } else {
            qWarning(WEATHER::ION::NMCCN) << "Invalid response:" << response;
        }
    } else if (reply->isFinished()) {
        qWarning(WEATHER::ION::NMCCN) << "Request failed with error:" << reply->error();
    } else {
        qWarning(WEATHER::ION::NMCCN) << "Request has not been finished yet.";
    }
    QJsonValue defaultValue;
    return defaultValue;
}

Ion::ConditionIcons NmcCn::getWeatherConditionIcon(const QString &img, const bool windy, const bool night) const
{
    const QMap<QString, Ion::ConditionIcons> conditionIconsMap = {
        {QStringLiteral("0"),
         windy && night ? ClearWindyNight
             : windy    ? ClearWindyDay
             : night    ? ClearNight
                        : ClearDay},
        {QStringLiteral("1"),
         windy && night ? PartlyCloudyWindyNight
             : windy    ? PartlyCloudyWindyDay
             : night    ? PartlyCloudyNight
                        : PartlyCloudyDay},
        {QStringLiteral("2"), windy ? OvercastWindy : Overcast},
        {QStringLiteral("3"), night ? ChanceShowersNight : ChanceShowersDay},
        {QStringLiteral("4"), night ? ChanceThunderstormNight : ChanceThunderstormDay},
        {QStringLiteral("6"), RainSnow},
        {QStringLiteral("7"), LightRain},
        {QStringLiteral("8"), Rain},
        {QStringLiteral("9"), Thunderstorm},
        {QStringLiteral("10"), Thunderstorm},
        {QStringLiteral("13"), night ? ChanceSnowNight : ChanceSnowDay},
        {QStringLiteral("14"), LightSnow},
        {QStringLiteral("15"), Snow},
    };

    if (conditionIconsMap.contains(img)) {
        return conditionIconsMap[img];
    }
    if (img == invalidValue) {
        qWarning(WEATHER::ION::NMCCN) << "Found invalid img.";
    } else {
        qWarning(WEATHER::ION::NMCCN) << "Found unknown img" << img;
    }
    return NotAvailable;
}

Ion::WindDirections NmcCn::getWindDirection(const float degree) const
{
    const float unit = 360.0 / 16;
    if(degree >= unit * 0 && degree < unit * 0 + unit / 2) {
        return N;
    }
    else if(degree < unit * 1 + unit / 2) {
        return NNE;
    }
    else if(degree < unit * 2 + unit / 2) {
        return NE;
    }
    else if(degree < unit * 3 + unit / 2) {
        return ENE;
    }
    else if(degree < unit * 4 + unit / 2) {
        return E;
    }
    else if(degree < unit * 5 + unit / 2) {
        return ESE;
    }
    else if(degree < unit * 6 + unit / 2) {
        return SE;
    }
    else if(degree < unit * 7 + unit / 2) {
        return SSE;
    }
    else if(degree < unit * 8 + unit / 2) {
        return S;
    }
    else if(degree < unit * 9 + unit / 2) {
        return SSW;
    }
    else if(degree < unit * 10 + unit / 2) {
        return SW;
    }
    else if(degree < unit * 11 + unit / 2) {
        return WSW;
    }
    else if(degree < unit * 12 + unit / 2) {
        return W;
    }
    else if(degree < unit * 13 + unit / 2) {
        return WNW;
    }
    else if(degree < unit * 14 + unit / 2) {
        return NW;
    }
    else if(degree < unit * 15 + unit / 2) {
        return NNW;
    }
    else if(degree < unit * 16){
        return N;
    }
    else {
        qWarning(WEATHER::ION::NMCCN) << "Invalid degree" << degree;
        return VR;
    }
}

QString NmcCn::getWindDirection(const Ion::WindDirections windDirection) const
{
    const QList<QString> windDirectionStrings = {
        QStringLiteral("N"),
        QStringLiteral("NNE"),
        QStringLiteral("NE"),
        QStringLiteral("ENE"),
        QStringLiteral("E"),
        QStringLiteral("SSE"),
        QStringLiteral("SE"),
        QStringLiteral("ESE"),
        QStringLiteral("S"),
        QStringLiteral("NNW"),
        QStringLiteral("NW"),
        QStringLiteral("WNW"),
        QStringLiteral("W"),
        QStringLiteral("SSW"),
        QStringLiteral("SW"),
        QStringLiteral("WSW"),
        QStringLiteral("VR"),
    };
    if (windDirectionStrings.count() > windDirection) {
        return windDirectionStrings[windDirection];
    }
    qWarning(WEATHER::ION::NMCCN) << "Invalid wind direction" << windDirection;
    return windDirectionStrings.last();
}

void NmcCn::findPlaces(const QString &searchString, const int searchLimit)
{
    QNetworkRequest request;
    QUrl url(apiBase + QStringLiteral("/essearch/api/autocomplete"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), searchString);
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("%1").arg(searchLimit));
    query.addQueryItem(QStringLiteral("timestamp"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("_"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    qDebug(WEATHER::ION::NMCCN) << "Generated search api url:" << url.toEncoded();
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, apiBase + QStringLiteral("/publish/forecast.html"));
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, userAgent);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json;charset=utf-8"));
    request.setHeaders(headers);
    qDebug(WEATHER::ION::NMCCN) << "Generated request headers:" << headers;
    networkAccessManager.get(request);
}

void NmcCn::fetchForecast(const QString &stationId, const QString &referer)
{
    QNetworkRequest request;
    QUrl url(apiBase + QStringLiteral("/rest/weather"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("stationid"), stationId);
    query.addQueryItem(QStringLiteral("_"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    qDebug(WEATHER::ION::NMCCN) << "Generated request api url:" << url.toEncoded();
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, referer);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, userAgent);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json;charset=utf-8"));
    request.setHeaders(headers);
    qDebug(WEATHER::ION::NMCCN) << "Generated request headers:" << headers;
    networkAccessManager.get(request);
}

#include "nmccn.moc"
