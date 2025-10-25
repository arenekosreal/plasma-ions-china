#include <KPluginFactory>
#include <KUnitConversion/Converter>
#include <KUnitConversion/Unit>
#include <KLocalizedString>
#include <QDateTime>
#include <QHttpHeaders>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTime>
#include <QUrlQuery>

#include "nmccn.hpp"
#include "nmccn_debug.hpp"

NmcCnIon::NmcCnIon(QObject *parent)
    : IonInterface(parent)
{
    networkAccessManager.setParent(this);
#ifdef ION_LEGACY
    setInitialized(true);
#endif // ION_LEGACY
}

NmcCnIon::~NmcCnIon()
{
    warnInfoCache.clear();
    lastValidDayCache.clear();
    networkAccessManager.clearAccessCache();
#ifdef ION_LEGACY
    dataCache.clear();
#endif // ION_LEGACY
}

template<typename T>
T NmcCnIon::handleNetworkReply(QNetworkReply *reply, std::function<T(QNetworkReply*)> callable)
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

template
QJsonArray NmcCnIon::handleNetworkReply<QJsonArray>(QNetworkReply* reply, std::function<QJsonArray(QNetworkReply*)> callable);

template
QJsonObject NmcCnIon::handleNetworkReply<QJsonObject>(QNetworkReply* reply, std::function<QJsonObject(QNetworkReply*)> callable);

NmcCnIon::ConditionIcons NmcCnIon::getWeatherConditionIcon(const QString &img, const bool windy, const bool night) const
{
    bool ok;
    const int imgId = img.toInt(&ok);
    if (img != QStringLiteral(INVALID_VALUE_STR) && ok) {
        switch (imgId)
        {
            case 0:
                if (windy && night) {
                    return ClearWindyNight;
                }
                else if (windy) {
                    return ClearWindyDay;
                }
                else if (night) {
                    return ClearNight;
                }
                else {
                    return ClearDay;
                }
            case 1:
                if (windy && night) {
                    return PartlyCloudyWindyNight;
                }
                else if (windy) {
                    return PartlyCloudyWindyDay;
                }
                else if (night) {
                    return PartlyCloudyNight;
                }
                else {
                    return PartlyCloudyDay;
                }
            case 2:
                if (windy) {
                    return OvercastWindy;
                }
                else {
                    return Overcast;
                }
            case 3:
                if (night) {
                    return ChanceShowersNight;
                }
                else {
                    return ChanceShowersDay;
                }
            case 4:
                if (night) {
                    return ChanceThunderstormNight;
                }
                else {
                    return ChanceThunderstormDay;
                }
            //case 5:
                // TODO
            //case 6:
                // TODO
            case 7:
                return LightRain;
            case 8:
                return Rain;
            case 9:
            case 10:
                return Thunderstorm;
            default:
                return NotAvailable;
        }
    }
    else {
        qWarning(IONENGINE_NMCCN) << "Failed to parse img id:" << img;
        return NotAvailable;
    }
}

QString NmcCnIon::getWindDirectionString(const float degree) const
{
    if (degree <= 11.25 && degree > 0) {
        return QStringLiteral("N");
    }
    else if (degree <= 33.75 && degree > 11.25) {
        return QStringLiteral("NNE");
    }
    else if (degree <= 56.25 && degree > 33.75) {
        return QStringLiteral("NE");
    }
    else if (degree <= 78.75 && degree > 56.25) {
        return QStringLiteral("ENE");
    }
    else if (degree <= 101.25 && degree > 78.75) {
        return QStringLiteral("E");
    }
    else if (degree <= 123.75 && degree > 101.25) {
        return QStringLiteral("ESE");
    }
    else if (degree <= 146.25 && degree > 123.75) {
        return QStringLiteral("SE");
    }
    else if (degree <= 168.75 && degree > 146.25) {
        return QStringLiteral("SSE");
    }
    else if (degree <= 191.25 && degree > 168.75) {
        return QStringLiteral("S");
    }
    else if (degree <= 213.75 && degree > 191.25) {
        return QStringLiteral("SSW");
    }
    else if (degree <= 236.25 && degree > 213.75) {
        return QStringLiteral("SW");
    }
    else if (degree <= 258.75 && degree > 236.25) {
        return QStringLiteral("WSW");
    }
    else if (degree <= 281.25 && degree > 258.75) {
        return QStringLiteral("W");
    }
    else if (degree <= 303.75 && degree > 281.25) {
        return QStringLiteral("WNW");
    }
    else if (degree <= 326.25 && degree > 303.75) {
        return QStringLiteral("NW");
    }
    else if (degree <= 348.75 && degree > 326.25) {
        return QStringLiteral("NNW");
    }
    else if (degree <= 360 && degree > 348.75) {
        return QStringLiteral("N");
    }
    else {
        qWarning(IONENGINE_NMCCN) << "Invalid degree:" << degree;
        return QStringLiteral("VR");
    }
}

QNetworkReply *NmcCnIon::requestSearchingPlacesApi(const QString &searchString, const int searchLimit)
{
    const QString encodedSearchString = QString::fromLocal8Bit(searchString.toUtf8().toPercentEncoding());
    QNetworkRequest request;
    QUrl url(QStringLiteral(SEARCH_API));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), encodedSearchString);
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("%1").arg(searchLimit));
    query.addQueryItem(QStringLiteral("timestamp"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("_"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    qDebug(IONENGINE_NMCCN) << "Requesting url:" << url;
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, FORECAST_PAGE);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::ContentType, "application/json;charset=utf-8");
    request.setHeaders(headers);
    qDebug(IONENGINE_NMCCN) << "Requesting headers:" << headers;
    return networkAccessManager.get(request);
}

QNetworkReply *NmcCnIon::requestWeatherApi(const QString &stationId, const QString &referer)
{
    QNetworkRequest request;
    QUrl url(QStringLiteral(WEATHER_API));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("stationid"), stationId);
    query.addQueryItem(QStringLiteral("_"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, referer);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::ContentType, "application/json;charset=utf-8");
    request.setHeaders(headers);
    return networkAccessManager.get(request);
}

QJsonArray NmcCnIon::extractSearchApiResponse(QNetworkReply *reply)
{
    qDebug(IONENGINE_NMCCN) << "Setting searching data based on reply of url:" << reply->url();
    QJsonParseError error;
    const QJsonObject responseObject = QJsonDocument::fromJson(reply->readAll(), &error).object();
    switch (responseObject[QStringLiteral("code")].toInt(-1)) {
        case 0:
            return responseObject[QStringLiteral("data")].toArray();
        case -1:
            qFatal(IONENGINE_NMCCN) << "Failed to parse json at" << error.offset << "because:" << error.errorString();
            break;
        default:
            qFatal(IONENGINE_NMCCN) << "API response invalid:" << responseObject[QStringLiteral("msg")].toString();
            break;
    }
    QJsonArray ret;
    return ret;
}

QJsonObject NmcCnIon::extractWeatherApiResponse(QNetworkReply *reply)
{
    qDebug(IONENGINE_NMCCN) << "Setting weather api data based on reply of url:" << reply->url();
    QJsonParseError error;
    const QJsonObject responseObject = QJsonDocument::fromJson(reply->readAll(), &error).object();
    switch (responseObject[QStringLiteral("code")].toInt(-1)) {
        case 0:
            return responseObject[QStringLiteral("data")].toObject();
        case -1:
            qFatal(IONENGINE_NMCCN) << "Failed to parse json at" << error.offset << "because:" << error.errorString();
            break;
        default:
            qFatal(IONENGINE_NMCCN) << "API response invalid:" << responseObject[QStringLiteral("msg")].toString();
            break;
    }
    QJsonObject ret;
    return ret;
}

bool NmcCnIon::updateWarnInfoCache(const QJsonObject &warnObject, const QString &stationId)
{
    QRegularExpression invalidValueRegex(QStringLiteral("^" INVALID_VALUE_STR "$"));
    const QString warnObjectAlert = warnObject[QStringLiteral("alert")].toString().remove(invalidValueRegex);
    const QString warnObjectIssueContent = warnObject[QStringLiteral("issuecontent")].toString().remove(invalidValueRegex);
    const QString warnObjectProvince = warnObject[QStringLiteral("province")].toString().remove(invalidValueRegex);
    const QString warnObjectSignalLevel = warnObject[QStringLiteral("signallevel")].toString().remove(invalidValueRegex);
    const QString warnObjectSignalType = warnObject[QStringLiteral("signaltype")].toString().remove(invalidValueRegex);
    const QString warnObjectUrl = warnObject[QStringLiteral("url")].toString().remove(invalidValueRegex);
    const bool warnObjectValid = !warnObjectAlert.isEmpty() && !warnObjectIssueContent.isEmpty() &&
                                 !warnObjectProvince.isEmpty() && !warnObjectSignalLevel.isEmpty() &&
                                 !warnObjectSignalType.isEmpty() && !warnObjectUrl.isEmpty();
    if (!warnInfoCache.contains(stationId)) {
        QList<WarnInfo> *warnInfos = new QList<WarnInfo>;
        warnInfoCache.insert(stationId, warnInfos); 
    }
    QList<WarnInfo> *warnInfos = warnInfoCache[stationId];
    const QDateTime now = QDateTime::currentDateTime();
    for (int i=0; i<warnInfos->count();i++) {
        const WarnInfo warnInfo = warnInfos->at(i);
        const QString warnSignalType = warnInfo.warnObject[QStringLiteral("signaltype")].toString();
        const QString warnSignalLevel = warnInfo.warnObject[QStringLiteral("signallevel")].toString();
        if (now > warnInfo.startTime.date().endOfDay()) {
            qDebug(IONENGINE_NMCCN) << "Removing outdated warn:" << warnInfo.warnObject << "started at" << warnInfo.startTime;
            warnInfos->remove(i);
        }
        else if (warnObjectValid && warnSignalType == warnObjectSignalType) {
            qDebug(IONENGINE_NMCCN) << "Removing existing warn:" << warnInfo.warnObject << "started at" << warnInfo.startTime;
            warnInfos->remove(i);
        }
    }
    warnInfos->squeeze();

    if (warnObjectValid) {
        WarnInfo warn;
        warn.warnObject = warnObject;
        warn.startTime = now;
        warnInfos->append(warn);
        qDebug(IONENGINE_NMCCN) << "Adding" << warnObject << "to cache.";
        return true;
    }
    qDebug(IONENGINE_NMCCN) << "Warn object" << warnObject << "is not a valid one, skip adding to cache.";
    return false;
}

bool NmcCnIon::updateLastValidDayCache(const QJsonObject &day, const QString &stationId)
{
    const QRegularExpression invalidValueRegex(QStringLiteral("^" INVALID_VALUE_STR "$"));
    const QJsonObject dayWeather = day[QStringLiteral("weather")].toObject();
    const QString dayWeatherInfo = dayWeather[QStringLiteral("info")].toString().remove(invalidValueRegex);
    const QString dayWeatherImg = dayWeather[QStringLiteral("img")].toString().remove(invalidValueRegex);
    const QString dayWeatherTemperature = dayWeather[QStringLiteral("temperature")].toString().remove(invalidValueRegex);
    const QJsonObject dayWind = day[QStringLiteral("wind")].toObject();
    const QString dayWindDirect = dayWind[QStringLiteral("direct")].toString().remove(invalidValueRegex);
    const QString dayWindPower = dayWind[QStringLiteral("power")].toString().remove(invalidValueRegex);
    const bool dayValid = !dayWeatherInfo.isEmpty() && !dayWeatherImg.isEmpty() && !dayWeatherTemperature.isEmpty() &&
                          !dayWindDirect.isEmpty() && !dayWindPower.isEmpty();
    if (dayValid) {
        qDebug(IONENGINE_NMCCN) << "Day value" << day << "is valid, adding to cache...";
        QJsonObject *dayCopy = new QJsonObject(day);
        lastValidDayCache.insert(stationId, dayCopy);
        return true;
    }
    qDebug(IONENGINE_NMCCN) << "Day value" << day << "is not valid, skip adding to cache...";
    return false;
}

#ifdef ION_LEGACY

K_PLUGIN_CLASS_WITH_JSON(NmcCnIon, "metadata.legacy.json");

void NmcCnIon::onSearchApiRequestFinished(QNetworkReply *reply, const QString &source)
{
    std::function<QJsonArray(QNetworkReply*)> wrapper = [=](QNetworkReply* r){return this->extractSearchApiResponse(r);};
    QJsonArray searchResult = handleNetworkReply(reply, wrapper);
    qDebug(IONENGINE_NMCCN) << "Deserialized data json array:" << searchResult;
    const int dataCount = searchResult.count();
    QStringList dataToSet = {
        ION_NAME,
        dataCount > 0 ? "valid" : "invalid",
        dataCount > 1 ? "multiple" : "single",
    };
    if (dataCount == 0) {
        dataToSet += {source.split(sourceSep)[2]};
    }
    else {
        // PlaceInfoString format:
        // id|city|province|refererPath|lon?|lat?
        for (const QJsonValue placeInfoString : searchResult) {
            const QStringList placeInfoStringParts = placeInfoString.toString().split(placeInfoSep);
            const QStringList extraDataParts = {placeInfoStringParts[0], placeInfoStringParts[3], placeInfoStringParts[4], placeInfoStringParts[5]};
            dataToSet += {"place", placeInfoStringParts[1] + "-" + placeInfoStringParts[2], "extra", extraDataParts.join(extraDataSep)};
        }
    }
    const QString dataStringToSet = dataToSet.join(sourceSep);
    qDebug(IONENGINE_NMCCN) << "Setting data:" << dataStringToSet << "for source:" << source;
    setData(source, "validate", dataStringToSet);
    reply->deleteLater();
}

void NmcCnIon::onWeatherApiRequestFinished(QNetworkReply *reply, const QString &source, const QString &creditUrl, const bool callSetData)
{
    std::function<QJsonObject(QNetworkReply*)> wrapper = [=](QNetworkReply* r){return this->extractWeatherApiResponse(r);};
    QJsonObject apiResponseData = handleNetworkReply(reply, wrapper);
    qDebug(IONENGINE_NMCCN) << "Deserialized data json object:" << apiResponseData;
    if (!apiResponseData.isEmpty()) {
        if (!dataCache.contains(source)) {
            Plasma5Support::DataEngine::Data *emptyData = new Plasma5Support::DataEngine::Data;
            dataCache.insert(source, emptyData);
        }
        Plasma5Support::DataEngine::Data *data = dataCache[source];
        data->insert("Credit", i18n("Source: National Meteorological Center of China"));
        data->insert("Credit Url", creditUrl);
        data->insert("Country", i18n( "China"));
        const QJsonObject real = apiResponseData["real"].toObject();
        const QJsonObject station = real["station"].toObject();
        const QString stationId = station["code"].toString();
        data->insert("Place", station["city"].toString());
        data->insert("Region", station["province"].toString());
        data->insert("Station", station["city"].toString());
        data->insert("Observation Period", real["publish_time"].toString());
        const QJsonObject sunriseSunset = real["sunriseSunset"].toObject();
        const QDateTime sunset = QDateTime::fromString(sunriseSunset["sunset"].toString(), realDateTimeFormat);
        const QDateTime sunrise = QDateTime::fromString(sunriseSunset["sunrise"].toString(), realDateTimeFormat);
        const QJsonObject weather = real["weather"].toObject();
        const QJsonObject wind = real["wind"].toObject();
        const QDateTime now = QDateTime::currentDateTime();
        const bool currentIsNight = sunset <= now || now < sunrise;
        const ConditionIcons currentWeatherConditionIcon =
            getWeatherConditionIcon(weather["img"].toString(), wind["speed"].toDouble() > 1.6, currentIsNight);
        data->insert("Condition Icon", getWeatherIcon(currentWeatherConditionIcon));
        data->insert("Current Conditions", weather["info"].toString()),
        data->insert("Temperature", weather["temperature"].toDouble());
        data->insert("Temperature Unit", KUnitConversion::Celsius);
        data->insert("Windchill", std::round(weather["feelst"].toDouble()));
        data->insert("Humidex", std::round(weather["feelst"].toDouble()));
        data->insert("Wind Direction", getWindDirectionString(wind["degree"].toDouble()));
        data->insert("Wind Speed", wind["speed"].toDouble());
        data->insert("Wind Speed Unit", KUnitConversion::MeterPerSecond);
        data->insert("Humidity", weather["humidity"].toInt());
        data->insert("Pressure", weather["airpressure"].toInt());
        data->insert("Pressure Unit", KUnitConversion::Hectopascal);
        data->insert("Sunrise At", sunrise.time());
        data->insert("Sunset At", sunset.time());
        const QString forecastRelatedKeyTemplate = "Short Forecast Day %1";
        const QString forecastRelatedValueTemplate = (QStringList){"%1", "%2", "%3", "%4", "%5", "%6"}.join(sourceSep);
        const QJsonObject predict = apiResponseData["predict"].toObject();
        const QJsonArray detail = predict["detail"].toArray();
        for (int i = 0; i < detail.count(); i++) {
            const QString forecastRelatedKey = forecastRelatedKeyTemplate.arg(i);
            const QJsonObject detailObject = detail.at(i).toObject();

            QJsonObject day = detailObject["day"].toObject();
            bool showNightDetailOnly = false;
            if (i==0 && !updateLastValidDayCache(day, stationId)) {
                if (lastValidDayCache.contains(stationId)) {
                    qWarning(IONENGINE_NMCCN) << "Found invalid day report, using cached value instead...";
                    day = *lastValidDayCache[stationId];
                }
                else {
                    qWarning(IONENGINE_NMCCN) << "Found invalid day report and no cached value.";
                    showNightDetailOnly = true;
                }
            }
            const QJsonObject dayWeather = day["weather"].toObject();
            const QJsonObject dayWind = day["wind"].toObject();
            const double dayWeatherTemperature = dayWeather["temperature"].toString().toDouble();
            const QString dayWeatherIcon = getWeatherIcon(getWeatherConditionIcon(dayWeather["img"].toString(), false, false));

            const QJsonObject night = detailObject["night"].toObject();
            const QJsonObject nightWeather = night["weather"].toObject();
            const QJsonObject nightWind = night["wind"].toObject();
            const double nightWeatherTemperature = nightWeather["temperature"].toString().toDouble();
            const QString nightWeatherIcon = getWeatherIcon(getWeatherConditionIcon(nightWeather["img"].toString(), false, true));

            const QString detailTitle =
                i > 0 ? QDate::fromString(detailObject["date"].toString(), realDateFormat).toString(realMonthDayFormat) :
                        currentIsNight ? i18ndc(KDE_WEATHER_TRANSLATION_DOMAIN, "Short for Tonight", "Tonight") :
                                         i18ndc(KDE_WEATHER_TRANSLATION_DOMAIN, "Short for Today", "Today");
            const QString detailWeatherIcon = currentIsNight ? nightWeatherIcon : dayWeatherIcon;
            const QString detailWeatherInfo = currentIsNight ? nightWeather["info"].toString() : dayWeather["info"].toString();
            const QString detailWeatherMaxTemperature = i == 0 && currentIsNight && showNightDetailOnly ?
                "N/U" : QString("%1").arg(std::max(dayWeatherTemperature, nightWeatherTemperature));
            const QString detailWeatherMinTemperature = i == 0 && currentIsNight && showNightDetailOnly ?
                QString("%1").arg(nightWeatherTemperature) : QString("%1").arg(std::min(dayWeatherTemperature, nightWeatherTemperature));
            const QString forecastRelatedValue = forecastRelatedValueTemplate
                .arg(detailTitle)
                .arg(detailWeatherIcon)
                .arg(detailWeatherInfo)
                .arg(detailWeatherMaxTemperature)
                .arg(detailWeatherMinTemperature)
                .arg("N/U");
            data->insert(forecastRelatedKey, forecastRelatedValue);
        }
        data->insert("Total Weather Days", detail.count());
        const QJsonObject warnObject = real["warn"].toObject();
        updateWarnInfoCache(warnObject, stationId);
        const QList<WarnInfo> *warnInfos = warnInfoCache[stationId];
        const QString warningRelatedDescriptionKeyTemplate = "Warning Description %1";
        const QString warningRelatedInfoKeyTemplate = "Warning Info %1";
        for (int i=0; i<warnInfos->count(); i++) {
            const WarnInfo warnInfo = warnInfos->at(i);
            const QString warningRelatedDescriptionKey = warningRelatedDescriptionKeyTemplate.arg(i);
            const QString warningRelatedDescriptionValue = warnInfo.warnObject["alert"].toString();
            data->insert(warningRelatedDescriptionKey, warningRelatedDescriptionValue);
            const QString warningRelatedInfoKey = warningRelatedInfoKeyTemplate.arg(i);
            const QString warningRelatedInfoValue = API_BASE + warnInfo.warnObject["url"].toString();
            data->insert(warningRelatedInfoKey, warningRelatedInfoValue);
        }
        data->insert("Total Warnings Issued", warnInfos->count());
        if (callSetData) {
            Q_EMIT cleanUpData(source);
            qDebug(IONENGINE_NMCCN) << "Responding source:" << source;
            setData(source, *data);
            dataCache.remove(source);
        }
    }
    reply->deleteLater();
}

bool NmcCnIon::updateIonSource(const QString &source)
{
    // source format:
    // ionname|validate|place_name|extra - Triggers validation of place
    // ionname|weather|place_name|extra - Triggers receiving weather of place
    // See also: https://techbase.kde.org/Projects/Plasma/Weather/Ions
    qDebug(IONENGINE_NMCCN) << "Update source:" << source;
    const char extraDataSep = ';';
    QStringList splitSource = source.split(sourceSep);
    if (splitSource.count() >= 3) {
        const QString requestName = splitSource[1];
        const Qt::ConnectionType networkAccessManagerSlotConnectionType = (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection);
        if (requestName == "validate") {
            qDebug(IONENGINE_NMCCN) << "Responsing validate request...";
            connect(&networkAccessManager, &QNetworkAccessManager::finished,
                    this, [=](QNetworkReply *reply) {this->onSearchApiRequestFinished(reply, source);},
                    networkAccessManagerSlotConnectionType);
            requestSearchingPlacesApi(splitSource[2]);
            return true;
        }
        else if (requestName == "weather" && splitSource.count() >= 4 && !splitSource[3].isEmpty()) {
            // splitSource[3] format:
            // id;refererPath;lon?;lat?
            const QStringList splitExtraData = splitSource[3].split(extraDataSep);
            if (splitExtraData.count() >= 4) {
                qDebug(IONENGINE_NMCCN) << "Responsing weather request...";
                const QString creditPage = FORECAST_CITY_PAGE + splitExtraData[1];

                connect(&networkAccessManager, &QNetworkAccessManager::finished, this,
                        [=](QNetworkReply *reply) {this->onWeatherApiRequestFinished(reply, source, creditPage, true);},
                        networkAccessManagerSlotConnectionType);
                requestWeatherApi(splitExtraData[0], creditPage);
                return true;
            }
        }
    }
    QStringList mailformedValue = {ION_NAME, "mailformed"};
    const QString value = mailformedValue.join(sourceSep);
    qDebug(IONENGINE_NMCCN) << "Responsing invalid request with:" << value;
    setData(source, "validate", value);
    return true;
}

void NmcCnIon::reset()
{
    updateAllSources();
}
#else // ION_LEGACY

K_PLUGIN_CLASS_WITH_JSON(NmcCnIon, "metadata.json");

void NmcCnIon::findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &searchString)
{
    
}

void NmcCnIon::fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo)
{
    
}
#endif // ION_LEGACY

#include "moc_nmccn.cpp"
#include "nmccn.moc"
