#include <KLocalizedString>
#include <KPluginFactory>
#include <KUnitConversion/Unit>
#include <QUrlQuery>

#include "nmccn.hpp"
#include "nmccn_debug.hpp"

K_PLUGIN_CLASS_WITH_JSON(NmcCn, "metadata.json");

NmcCn::NmcCn(QObject *parent)
    : IonInterface(parent)
    , networkAccessManager(this)
{
    setInitialized(true);
}

NmcCn::~NmcCn()
{
    networkAccessManager.clearAccessCache();
}

bool NmcCn::updateIonSource(const QString &source)
{
    // source format:
    // ionname|validate|place_name|extra - Triggers validation of place
    // ionname|weather|place_name|extra - Triggers receiving weather of place
    // See also: https://techbase.kde.org/Projects/Plasma/Weather/Ions
    qDebug(IONENGINE_NMCCN) << "Update Ion source" << source;
    const QStringList splitedSource = source.split(sourcePartsSep);
    if (splitedSource.count() >= 3) {
        const QString requestName = splitedSource[1];
        if (requestName == QStringLiteral("validate")) {
            qDebug(IONENGINE_NMCCN) << "Responding validate request...";
            connect(
                &networkAccessManager,
                &QNetworkAccessManager::finished,
                this,
                [=](QNetworkReply *reply) {
                    const QJsonValue value = this->extractApiResponse(reply);
                    qDebug(IONENGINE_NMCCN) << "Got data:" << value;
                    if (value.isArray()) {
                        Q_EMIT this->foundPlaces(value.toArray());
                    } else {
                        qWarning(IONENGINE_NMCCN) << "Response is mailformed. Wants array, is" << value.type();
                    }
                },
                (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
            connect(
                this,
                &NmcCn::foundPlaces,
                this,
                [=](const QJsonArray &array) {
                    const int count = array.count();
                    QStringList dataToSetParts = {
                        QStringLiteral("nmccn"),
                        count > 0 ? QStringLiteral("valid") : QStringLiteral("invalid"),
                        count > 1 ? QStringLiteral("multiple") : QStringLiteral("single"),
                    };
                    if (count == 0) {
                        dataToSetParts += {splitedSource[2]};
                    } else {
                        // searchResult
                        // id|city|province|refererPath|lon|lat
                        for (const QJsonValue &value : array) {
                            const QStringList searchResultParts = value.toString().split(placeInfoSep),
                                              extraDataParts = {searchResultParts[0], searchResultParts[3], searchResultParts[4], searchResultParts[5]};
                            dataToSetParts += {
                                QStringLiteral("place"),
                                QStringLiteral("%1-%2").arg(searchResultParts[1]).arg(searchResultParts[2]),
                                QStringLiteral("extra"),
                                extraDataParts.join(extraDataPartsSep),
                            };
                        }
                    }
                    const QString dataToSet = dataToSetParts.join(sourcePartsSep);
                    qDebug(IONENGINE_NMCCN) << "Setting data" << dataToSet << "for source" << source;
                    setData(source, QStringLiteral("validate"), dataToSet);
                },
                (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
            findPlaces(splitedSource[2]);
            return true;
        } else if (requestName == QStringLiteral("weather") && splitedSource.count() >= 4 && !splitedSource[3].isEmpty()) {
            qDebug(IONENGINE_NMCCN) << "Responding weather request...";
            const QStringList splitedExtraData = splitedSource[3].split(extraDataPartsSep);
            if (splitedExtraData.count() >= 2) {
                const QString stationId = splitedExtraData[0], referer = apiBase + QStringLiteral("/publish/forecast") + splitedExtraData[1];
                qDebug(IONENGINE_NMCCN) << "Getting weather for id" << stationId << "with referer" << referer;
                connect(
                    &networkAccessManager,
                    &QNetworkAccessManager::finished,
                    this,
                    [=](QNetworkReply *reply) {
                        const QJsonValue value = this->extractApiResponse(reply);
                        qDebug(IONENGINE_NMCCN) << "Got data:" << value;
                        if (value.isObject()) {
                            Q_EMIT this->fetchedForecast(value.toObject());
                        } else {
                            qWarning(IONENGINE_NMCCN) << "Response is mailformed. Wants object, is" << value.type();
                        }
                    },
                    (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
                connect(
                    this,
                    &NmcCn::fetchedForecast,
                    this,
                    [=](const QJsonObject &object) {
                        Plasma5Support::DataEngine::Data data;
                        data.insert(QStringLiteral("Credit"), i18n("Source: National Meteorological Center of China"));
                        data.insert(QStringLiteral("Credit Url"), referer);
                        data.insert(QStringLiteral("Country"), i18n("China"));

                        data.insert(QStringLiteral("Place"), object[QStringLiteral("station")][QStringLiteral("city")].toString());
                        data.insert(QStringLiteral("Region"), object[QStringLiteral("station")][QStringLiteral("province")].toString());
                        data.insert(QStringLiteral("Station"), object[QStringLiteral("station")][QStringLiteral("city")].toString());
                        data.insert(QStringLiteral("Observation Period"), object[QStringLiteral("real")][QStringLiteral("publish_time")].toString());
                        const QDateTime sunrise = QDateTime::fromString(object[QStringLiteral("real")][QStringLiteral("sunriseSunset")][QStringLiteral("sunrise")].toString(), fullTimeFormat),
                                        sunset = QDateTime::fromString(object[QStringLiteral("real")][QStringLiteral("sunriseSunset")][QStringLiteral("sunset")].toString(), fullTimeFormat),
                                        now = QDateTime::currentDateTime();
                        const qreal windSpeed = object[QStringLiteral("real")][QStringLiteral("wind")][QStringLiteral("speed")].toDouble();
                        const bool currentIsNight = sunset <= now || now < sunrise,
                                   currentIsWindy = windSpeed > 1.6; // wind faster than 1.6m/s means windy(to human)
                        const ConditionIcons conditionIcon =
                            this->getWeatherConditionIcon(object[QStringLiteral("real")][QStringLiteral("weather")][QStringLiteral("img")].toString(), currentIsWindy, currentIsNight);
                        data.insert(QStringLiteral("Condition Icon"), this->getWeatherIcon(conditionIcon));
                        data.insert(QStringLiteral("Current Conditions"), object[QStringLiteral("real")][QStringLiteral("weather")][QStringLiteral("info")].toString());
                        data.insert(QStringLiteral("Temperature"), object[QStringLiteral("real")][QStringLiteral("weather")][QStringLiteral("temperature")].toDouble());
                        data.insert(QStringLiteral("Windchill"), object[QStringLiteral("real")][QStringLiteral("weather")][QStringLiteral("feelst")].toDouble());
                        data.insert(QStringLiteral("Humidex"), object[QStringLiteral("real")][QStringLiteral("weather")][QStringLiteral("feelst")].toDouble());
                        data.insert(QStringLiteral("Wind Direction"),
                                    this->getWindDirection(this->getWindDirection(object[QStringLiteral("real")][QStringLiteral("wind")][QStringLiteral("degree")].toDouble())));
                        data.insert(QStringLiteral("Wind Speed"), object[QStringLiteral("real")][QStringLiteral("wind")][QStringLiteral("speed")].toDouble());
                        data.insert(QStringLiteral("Wind Speed Unit"), KUnitConversion::MeterPerSecond);
                        data.insert(QStringLiteral("Humidity"), object[QStringLiteral("real")][QStringLiteral("weather")][QStringLiteral("humidity")].toInt());
                        data.insert(QStringLiteral("Pressure"), object[QStringLiteral("passedchart")].toArray().first()[QStringLiteral("pressure")].toDouble());
                        data.insert(QStringLiteral("Pressure Unit"), KUnitConversion::Hectopascal);
                        data.insert(QStringLiteral("Sunrise At"), sunrise.time());
                        data.insert(QStringLiteral("Sunset At"), sunset.time());
                        const QString forecastRelatedKeyTemplate = QStringLiteral("Short Forecast Day %1");
                        const QStringList forecastRelatedValueTemplateParts = {
                            QStringLiteral("%1"),
                            QStringLiteral("%2"),
                            QStringLiteral("%3"),
                            QStringLiteral("%4"),
                            QStringLiteral("%5"),
                            QStringLiteral("%6"),
                        };
                        const QString forecastRelatedValueTemplate = forecastRelatedValueTemplateParts.join(sourcePartsSep);
                        const QJsonArray details = object[QStringLiteral("predict")][QStringLiteral("detail")].toArray();
                        for (int i = 0; i < details.count(); i++) {
                            const QDate date = QDate::fromString(details[i][QStringLiteral("date")].toString(), fullDateFormat);
                            const QString dayInfo = details[i][QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("info")].toString(),
                                          dayImg = details[i][QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("img")].toString(),
                                          dayTemperature =
                                              details[i][QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("temperature")].toString(),
                                          nightInfo = details[i][QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("info")].toString(),
                                          nightImg = details[i][QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("img")].toString(),
                                          nightTemperature =
                                              details[i][QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("temperature")].toString();
                            QString title, img, info, maxTemperature, minTemperature;
                            if (currentIsNight) {
                                qDebug(IONENGINE_NMCCN) << "Using night report.";
                                title =
                                    i > 0 ? date.toString(dateFormat) : i18ndc(KDE_WEATHER_TRANSLATION_DOMAIN, "Short for Tonight", "Tonight");
                                img = details[i][QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("img")].toString();
                                info = details[i][QStringLiteral("night")][QStringLiteral("weather")][QStringLiteral("info")].toString();
                            } else {
                                qDebug(IONENGINE_NMCCN) << "Using day report.";
                                title = i > 0 ? date.toString(dateFormat) : i18ndc(KDE_WEATHER_TRANSLATION_DOMAIN, "Short for Today", "Today");
                                img = details[i][QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("img")].toString();
                                info = details[i][QStringLiteral("day")][QStringLiteral("weather")][QStringLiteral("info")].toString();
                            }
                            if (dayTemperature != invalidValue && nightTemperature != invalidValue) {
                                maxTemperature = QStringLiteral("%1").arg(std::max(dayTemperature.toDouble(), nightTemperature.toDouble()));
                                minTemperature = QStringLiteral("%1").arg(std::min(dayTemperature.toDouble(), nightTemperature.toDouble()));
                            } else if (dayTemperature != invalidValue) {
                                maxTemperature = dayTemperature;
                                minTemperature = QStringLiteral("N/U");
                            } else {
                                maxTemperature = QStringLiteral("N/U");
                                minTemperature = nightTemperature;
                            }
                            if (img != invalidValue && info != invalidValue) {
                                const QString forecastRelatedKey = forecastRelatedKeyTemplate.arg(i),
                                              forecastRelatedValue = forecastRelatedValueTemplate.arg(title)
                                                                         .arg(this->getWeatherIcon(this->getWeatherConditionIcon(img, false, currentIsNight)))
                                                                         .arg(info)
                                                                         .arg(maxTemperature)
                                                                         .arg(minTemperature);
                                data.insert(forecastRelatedKey, forecastRelatedValue);
                                qDebug(IONENGINE_NMCCN) << "Inserting" << forecastRelatedKey << "with value" << forecastRelatedValue;
                            } else {
                                qWarning(IONENGINE_NMCCN) << "Invalid img or info found. Skipping...";
                            }
                        }
                        data.insert(QStringLiteral("Total Weather Days"), details.count());
                        const QString alert = object[QStringLiteral("real")][QStringLiteral("warn")][QStringLiteral("alert")].toString(),
                                      urlPath = object[QStringLiteral("real")][QStringLiteral("warn")][QStringLiteral("url")].toString();
                        if (alert != invalidValue && urlPath != invalidValue) {
                            qDebug(IONENGINE_NMCCN) << "Found active warning" << object[QStringLiteral("real")][QStringLiteral("warn")].toObject();
                            const QString url = apiBase + urlPath;
                            data.insert(QStringLiteral("Warning Description 0"), alert);
                            data.insert(QStringLiteral("Warning Info 0"), url);
                            data.insert(QStringLiteral("Total Warnings Issued"), 1);
                        } else {
                            data.insert(QStringLiteral("Total Warnings Issued"), 0);
                        }
                        Q_EMIT this->cleanUpData(source);
                        qDebug(IONENGINE_NMCCN) << "Responding source" << source;
                        this->setData(source, data);
                    },
                    (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection));
                fetchForecast(stationId, referer);
                return true;
            } else {
                qWarning(IONENGINE_NMCCN) << "Re-search your city to fix invalid placeInfo" << splitedSource[3];
            }
        }
    }
    return true;
}

void NmcCn::reset()
{
    updateAllSources();
}

QJsonValue NmcCn::extractApiResponse(QNetworkReply *reply) const
{
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        qDebug(IONENGINE_NMCCN) << "Request successfully.";
        QJsonParseError error;
        const QJsonDocument response = QJsonDocument::fromJson(reply->readAll(), &error);
        if (response.isObject()) {
            const QJsonObject responseObj = response.object();
            switch (responseObj[QStringLiteral("code")].toInt(-1)) {
            case 0:
                return responseObj[QStringLiteral("data")];
            case -1:
                qWarning(IONENGINE_NMCCN) << "Failed to parse response at" << error.offset << "because" << error.errorString();
                break;
            default:
                qWarning(IONENGINE_NMCCN) << "Server responded a failure:" << responseObj[QStringLiteral("msg")].toString();
            }
        } else {
            qWarning(IONENGINE_NMCCN) << "Invalid response:" << response;
        }
    } else if (reply->isFinished()) {
        qWarning(IONENGINE_NMCCN) << "Request failed with error:" << reply->error();
    } else {
        qWarning(IONENGINE_NMCCN) << "Request has not been finished yet.";
    }
    QJsonValue defaultValue;
    return defaultValue;
}

IonInterface::ConditionIcons NmcCn::getWeatherConditionIcon(const QString &img, const bool windy, const bool night) const
{
    const QMap<QString, IonInterface::ConditionIcons> conditionIconsMap = {
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
    if (img != invalidValue) {
        qWarning(IONENGINE_NMCCN) << "Failed to parse img id:" << img;
    }
    return NotAvailable;
}

IonInterface::WindDirections NmcCn::getWindDirection(const float degree) const
{
    const float unit = 22.5;
    return degree >= 0 ? (IonInterface::WindDirections)(qRound((degree + unit / 2) / unit) % 16) : VR;
}

QString NmcCn::getWindDirection(const IonInterface::WindDirections windDirection) const
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
    qWarning(IONENGINE_NMCCN) << "Invalid wind direction" << windDirection;
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
    qDebug(IONENGINE_NMCCN) << "Generated search api url:" << url.toEncoded();
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, apiBase + QStringLiteral("/publish/forecast.html"));
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, userAgent);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json;charset=utf-8"));
    request.setHeaders(headers);
    qDebug(IONENGINE_NMCCN) << "Generated request headers:" << headers;
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
    qDebug(IONENGINE_NMCCN) << "Generated search api url:" << url.toEncoded();
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, referer);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, userAgent);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json;charset=utf-8"));
    request.setHeaders(headers);
    qDebug(IONENGINE_NMCCN) << "Generated request headers:" << headers;
    networkAccessManager.get(request);
}

#include "nmccn.moc"
