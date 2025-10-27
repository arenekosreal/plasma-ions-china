#include <KPluginFactory>
#include <KUnitConversion/Converter>
#include <KUnitConversion/Unit>
#include <KLocalizedString>
#include <QDateTime>
#include <QHttpHeaders>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLocale>
#include <QMap>
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
    const QMap<QString, NmcCnIon::ConditionIcons> conditionIconsMap = {
        {QStringLiteral("0"), windy && night ? ClearWindyNight : windy ? ClearWindyDay : night ? ClearNight : ClearDay},
        {QStringLiteral("1"), windy && night ? PartlyCloudyWindyNight : windy ? PartlyCloudyWindyDay : night ? PartlyCloudyNight : PartlyCloudyDay},
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
    if (img != QStringLiteral(INVALID_VALUE_STR)) {
        qWarning(IONENGINE_NMCCN) << "Failed to parse img id:" << img;
    }
    return NotAvailable;
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
    QNetworkRequest request;
    QUrl url(QStringLiteral(SEARCH_API));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), searchString);
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("%1").arg(searchLimit));
    query.addQueryItem(QStringLiteral("timestamp"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("_"), QStringLiteral("%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    qDebug(IONENGINE_NMCCN) << "Requesting url:" << url.toEncoded();
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, FORECAST_PAGE);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Accept, "application/json;charset=utf-8");
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
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Accept, "application/json;charset=utf-8");
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
        QStringLiteral(ION_NAME),
        dataCount > 0 ? QStringLiteral("valid") : QStringLiteral("invalid"),
        dataCount > 1 ? QStringLiteral("multiple") : QStringLiteral("single"),
    };
    if (dataCount == 0) {
        dataToSet += {source.split(QLatin1Char(sourceSep))[2]};
    }
    else {
        // PlaceInfoString format:
        // id|city|province|refererPath|lon?|lat?
        for (const QJsonValue placeInfoString : searchResult) {
            const QStringList placeInfoStringParts = placeInfoString.toString().split(QLatin1Char(placeInfoSep));
            const QStringList extraDataParts = {placeInfoStringParts[0], placeInfoStringParts[3], placeInfoStringParts[4], placeInfoStringParts[5]};
            dataToSet += {QStringLiteral("place"),
                          placeInfoStringParts[1] + QStringLiteral("-") + placeInfoStringParts[2],
                          QStringLiteral("extra"),
                          extraDataParts.join(QLatin1Char(extraDataSep)),
            };
        }
    }
    const QString dataStringToSet = dataToSet.join(QLatin1Char(sourceSep));
    qDebug(IONENGINE_NMCCN) << "Setting data:" << dataStringToSet << "for source:" << source;
    setData(source, QStringLiteral("validate"), dataStringToSet);
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
        data->insert(QStringLiteral("Credit"), i18n("Source: National Meteorological Center of China"));
        data->insert(QStringLiteral("Credit Url"), creditUrl);
        data->insert(QStringLiteral("Country"), i18n( "China"));
        const QJsonObject real = apiResponseData[QStringLiteral("real")].toObject();
        const QJsonObject station = real[QStringLiteral("station")].toObject();
        const QString stationId = station[QStringLiteral("code")].toString();
        data->insert(QStringLiteral("Place"), station[QStringLiteral("city")].toString());
        data->insert(QStringLiteral("Region"), station[QStringLiteral("province")].toString());
        data->insert(QStringLiteral("Station"), station[QStringLiteral("city")].toString());
        data->insert(QStringLiteral("Observation Period"), real[QStringLiteral("publish_time")].toString());
        const QJsonObject sunriseSunset = real[QStringLiteral("sunriseSunset")].toObject();
        const QDateTime sunset = QDateTime::fromString(sunriseSunset[QStringLiteral("sunset")].toString(), realDateTimeFormat);
        const QDateTime sunrise = QDateTime::fromString(sunriseSunset[QStringLiteral("sunrise")].toString(), realDateTimeFormat);
        const QJsonObject weather = real[QStringLiteral("weather")].toObject();
        const QJsonObject wind = real[QStringLiteral("wind")].toObject();
        const QDateTime now = QDateTime::currentDateTime();
        const bool currentIsNight = sunset <= now || now < sunrise;
        const ConditionIcons currentWeatherConditionIcon =
            getWeatherConditionIcon(weather[QStringLiteral("img")].toString(), wind[QStringLiteral("speed")].toDouble() > 1.6, currentIsNight);
        data->insert(QStringLiteral("Condition Icon"), getWeatherIcon(currentWeatherConditionIcon));
        data->insert(QStringLiteral("Current Conditions"), weather[QStringLiteral("info")].toString()),
        data->insert(QStringLiteral("Temperature"), weather[QStringLiteral("temperature")].toDouble());
        data->insert(QStringLiteral("Temperature Unit"), KUnitConversion::Celsius);
        data->insert(QStringLiteral("Windchill"), std::round(weather[QStringLiteral("feelst")].toDouble()));
        data->insert(QStringLiteral("Humidex"), std::round(weather[QStringLiteral("feelst")].toDouble()));
        data->insert(QStringLiteral("Wind Direction"), getWindDirectionString(wind[QStringLiteral("degree")].toDouble()));
        data->insert(QStringLiteral("Wind Speed"), wind[QStringLiteral("speed")].toDouble());
        data->insert(QStringLiteral("Wind Speed Unit"), KUnitConversion::MeterPerSecond);
        data->insert(QStringLiteral("Humidity"), weather[QStringLiteral("humidity")].toInt());
        data->insert(QStringLiteral("Pressure"), weather[QStringLiteral("airpressure")].toInt());
        data->insert(QStringLiteral("Pressure Unit"), KUnitConversion::Hectopascal);
        data->insert(QStringLiteral("Sunrise At"), sunrise.time());
        data->insert(QStringLiteral("Sunset At"), sunset.time());
        const QString forecastRelatedKeyTemplate = QStringLiteral("Short Forecast Day %1");
        const QStringList forecastRelatedValueTemplateParts = {
            QStringLiteral("%1"), QStringLiteral("%2"), QStringLiteral("%3"),
            QStringLiteral("%4"), QStringLiteral("%5"), QStringLiteral("%6"),
        };
        const QString forecastRelatedValueTemplate = forecastRelatedValueTemplateParts.join(QLatin1Char(sourceSep));
        const QJsonObject predict = apiResponseData[QStringLiteral("predict")].toObject();
        const QJsonArray detail = predict[QStringLiteral("detail")].toArray();
        for (int i = 0; i < detail.count(); i++) {
            const QString forecastRelatedKey = forecastRelatedKeyTemplate.arg(i);
            const QJsonObject detailObject = detail.at(i).toObject();

            QJsonObject day = detailObject[QStringLiteral("day")].toObject();
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
            const QJsonObject dayWeather = day[QStringLiteral("weather")].toObject();
            const QJsonObject dayWind = day[QStringLiteral("wind")].toObject();
            const double dayWeatherTemperature = dayWeather[QStringLiteral("temperature")].toString().toDouble();
            const QString dayWeatherIcon = getWeatherIcon(getWeatherConditionIcon(dayWeather[QStringLiteral("img")].toString(), false, false));

            const QJsonObject night = detailObject[QStringLiteral("night")].toObject();
            const QJsonObject nightWeather = night[QStringLiteral("weather")].toObject();
            const QJsonObject nightWind = night[QStringLiteral("wind")].toObject();
            const double nightWeatherTemperature = nightWeather[QStringLiteral("temperature")].toString().toDouble();
            const QString nightWeatherIcon = getWeatherIcon(getWeatherConditionIcon(nightWeather[QStringLiteral("img")].toString(), false, true));

            const QString detailTitle =
                i > 0 ? QDate::fromString(detailObject[QStringLiteral("date")].toString(), realDateFormat).toString(realMonthDayFormat) :
                        currentIsNight ? i18ndc(KDE_WEATHER_TRANSLATION_DOMAIN, "Short for Tonight", "Tonight") :
                                         i18ndc(KDE_WEATHER_TRANSLATION_DOMAIN, "Short for Today", "Today");
            const QString detailWeatherIcon = currentIsNight ? nightWeatherIcon : dayWeatherIcon;
            const QString detailWeatherInfo = currentIsNight ? nightWeather[QStringLiteral("info")].toString() : dayWeather[QStringLiteral("info")].toString();
            const QString detailWeatherMaxTemperature = i == 0 && currentIsNight && showNightDetailOnly ?
                QStringLiteral("N/U") : QStringLiteral("%1").arg(std::max(dayWeatherTemperature, nightWeatherTemperature));
            const QString detailWeatherMinTemperature = i == 0 && currentIsNight && showNightDetailOnly ?
                QStringLiteral("%1").arg(nightWeatherTemperature) : QStringLiteral("%1").arg(std::min(dayWeatherTemperature, nightWeatherTemperature));
            const QString forecastRelatedValue = forecastRelatedValueTemplate
                .arg(detailTitle)
                .arg(detailWeatherIcon)
                .arg(detailWeatherInfo)
                .arg(detailWeatherMaxTemperature)
                .arg(detailWeatherMinTemperature)
                .arg(QStringLiteral("N/U"));
            data->insert(forecastRelatedKey, forecastRelatedValue);
        }
        data->insert(QStringLiteral("Total Weather Days"), detail.count());
        const QJsonObject warnObject = real[QStringLiteral("warn")].toObject();
        updateWarnInfoCache(warnObject, stationId);
        const QList<WarnInfo> *warnInfos = warnInfoCache[stationId];
        const QString warningRelatedDescriptionKeyTemplate = QStringLiteral("Warning Description %1");
        const QString warningRelatedInfoKeyTemplate = QStringLiteral("Warning Info %1");
        for (int i=0; i<warnInfos->count(); i++) {
            const WarnInfo warnInfo = warnInfos->at(i);
            const QString warningRelatedDescriptionKey = warningRelatedDescriptionKeyTemplate.arg(i);
            const QString warningRelatedDescriptionValue = warnInfo.warnObject[QStringLiteral("alert")].toString();
            data->insert(warningRelatedDescriptionKey, warningRelatedDescriptionValue);
            const QString warningRelatedInfoKey = warningRelatedInfoKeyTemplate.arg(i);
            const QString warningRelatedInfoValue = QStringLiteral(API_BASE) + warnInfo.warnObject[QStringLiteral("url")].toString();
            data->insert(warningRelatedInfoKey, warningRelatedInfoValue);
        }
        data->insert(QStringLiteral("Total Warnings Issued"), warnInfos->count());
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
    QStringList splitSource = source.split(QLatin1Char(sourceSep));
    if (splitSource.count() >= 3) {
        const QString requestName = splitSource[1];
        if (requestName == QStringLiteral("validate")) {
            qDebug(IONENGINE_NMCCN) << "Responsing validate request...";
            connect(&networkAccessManager, &QNetworkAccessManager::finished,
                    this, [=](QNetworkReply *reply) {this->onSearchApiRequestFinished(reply, source);},
                    networkAccessManagerSlotConnectionType);
            requestSearchingPlacesApi(splitSource[2]);
            return true;
        }
        else if (requestName == QStringLiteral("weather") && splitSource.count() >= 4 && !splitSource[3].isEmpty()) {
            // splitSource[3] format:
            // id;refererPath;lon?;lat?
            const QStringList splitExtraData = splitSource[3].split(QLatin1Char(extraDataSep));
            if (splitExtraData.count() >= 4) {
                qDebug(IONENGINE_NMCCN) << "Responsing weather request...";
                const QString creditPage = QStringLiteral(FORECAST_CITY_PAGE) + splitExtraData[1];

                connect(&networkAccessManager, &QNetworkAccessManager::finished, this,
                        [=](QNetworkReply *reply) {this->onWeatherApiRequestFinished(reply, source, creditPage, true);},
                        networkAccessManagerSlotConnectionType);
                requestWeatherApi(splitExtraData[0], creditPage);
                return true;
            }
        }
    }
    QStringList mailformedValue = {QStringLiteral(ION_NAME), QStringLiteral("mailformed")};
    const QString value = mailformedValue.join(QLatin1Char(sourceSep));
    qDebug(IONENGINE_NMCCN) << "Responsing invalid request with:" << value;
    setData(source, QStringLiteral("validate"), value);
    return true;
}

void NmcCnIon::reset()
{
    updateAllSources();
}
#else // ION_LEGACY

K_PLUGIN_CLASS_WITH_JSON(NmcCnIon, "metadata.json");

Warnings::PriorityClass NmcCnIon::getWarnPriority(const QString &signallevel) const
{
    QMap<QString, Warnings::PriorityClass> priorityMaps = {
        {QStringLiteral("蓝色"), Warnings::Low},
        {QStringLiteral("黄色"), Warnings::Medium},
        {QStringLiteral("橙色"), Warnings::High},
        {QStringLiteral("红色"), Warnings::Extreme},
    };
    return priorityMaps.contains(signallevel) ? priorityMaps[signallevel] : Warnings::Low;
}

void NmcCnIon::onSearchApiRequestFinished(QNetworkReply* reply)
{
    if (locationsPromise->isCanceled()) {
        qDebug(IONENGINE_NMCCN) << "Searching locations is cancelled.";
    }
    else {
        qDebug(IONENGINE_NMCCN) << "Deserializing locations...";
        std::function<QJsonArray(QNetworkReply*)> wrapper = [=](QNetworkReply* r){return this->extractSearchApiResponse(r);};
        QJsonArray searchResult = handleNetworkReply(reply, wrapper);
        qDebug(IONENGINE_NMCCN) << "Deserialized data json array:" << searchResult;
        std::shared_ptr<Locations> locations = std::make_shared<Locations>();
        for (const QJsonValue placeInfoString: searchResult) {
            // PlaceInfoString format:
            // id|city|province|refererPath|lon?|lat?
            const QStringList placeInfoStringParts = placeInfoString.toString().split(QLatin1Char(placeInfoSep));
            const QStringList extraDataParts =
                {placeInfoStringParts[0], placeInfoStringParts[3], placeInfoStringParts[4], placeInfoStringParts[5]};
            const QStringList placeInfoParts = {
                QStringLiteral("place"),
                placeInfoStringParts[1] + QStringLiteral("-") + placeInfoStringParts[2],
                QStringLiteral("extra"),
                extraDataParts.join(QLatin1Char(extraDataSep))
            };
            Location location;
            bool lonOk, latOk;
            qreal lon = placeInfoStringParts[4].toDouble(&lonOk);
            qreal lat = placeInfoStringParts[5].toDouble(&latOk);
            if (lonOk && latOk) {
                location.setCoordinates(QPointF(lat, lon));
            }
            location.setStation(placeInfoStringParts[0]);
            location.setCode(placeInfoStringParts[0]);
            location.setDisplayName(placeInfoParts[1]);
            location.setPlaceInfo(extraDataParts.join(QLatin1Char(extraDataSep)));
            qDebug(IONENGINE_NMCCN) << "Adding location" << location.displayName();
            locations->addLocation(location);
        }
        qDebug(IONENGINE_NMCCN) << "Returning locations" << locations.get();
        locationsPromise->addResult(locations);
    }
    locationsPromise->finish();
    locationsPromise.reset();
}

void NmcCnIon::onWeatherApiRequestFinished(QNetworkReply* reply, const QString &extra, const bool &setNewPlaceInfo)
{
    if (forecastPromise->isCanceled()) {
        qDebug(IONENGINE_NMCCN) << "Getting forecast is cancelled.";
    }
    else {
        qDebug(IONENGINE_NMCCN) << "Deserializing forecasts...";
        std::function<QJsonObject(QNetworkReply*)> wrapper = [=](QNetworkReply* r){return this->extractWeatherApiResponse(r);};
        QJsonObject apiResponseData = handleNetworkReply(reply, wrapper);
        qDebug(IONENGINE_NMCCN) << "Deserialized data json object:" << apiResponseData;
        if (!apiResponseData.isEmpty()) {
            const QJsonObject real = apiResponseData[QStringLiteral("real")].toObject();
            const QJsonObject station_ = real[QStringLiteral("station")].toObject();

            MetaData metaData;
            metaData.setCredit(i18n("Source: National Meteorological Center of China"));
            metaData.setCreditURL(QStringLiteral(API_BASE) + station_[QStringLiteral("url")].toString());
            metaData.setTemperatureUnit(KUnitConversion::Celsius);
            metaData.setWindSpeedUnit(KUnitConversion::MeterPerSecond);
            metaData.setPressureUnit(KUnitConversion::Hectopascal);
            metaData.setRainfallUnit(KUnitConversion::Millimeter);

            Station station;
            const QString stationId = station_[QStringLiteral("code")].toString();
            station.setStation(station_[QStringLiteral("city")].toString());
            station.setPlace(station_[QStringLiteral("city")].toString());
            station.setRegion(station_[QStringLiteral("province")].toString());
            station.setCountry(i18n("China"));
            qDebug(IONENGINE_NMCCN) << "Using extra" << extra;
            if (setNewPlaceInfo) {
                station.setNewPlaceInfo(extra);
            }
            bool lonOk, latOk;
            const qreal lon = extra.split(QLatin1Char(extraDataSep))[2].toDouble(&lonOk);
            const qreal lat = extra.split(QLatin1Char(extraDataSep))[3].toDouble(&latOk);
            if (lonOk && latOk) {
                station.setCoordinates(lat, lon);
            }

            const QJsonObject predict = apiResponseData[QStringLiteral("predict")].toObject();
            const QJsonArray detail = predict[QStringLiteral("detail")].toArray();
            const QJsonObject currentDayDetail = detail[0].toObject();

            CurrentDay currentDay;
            const QJsonObject currentDayDetailDayObject = currentDayDetail[QStringLiteral("day")].toObject();
            bool currentDayHighTempOk, currentDayLowTempOk;
            const qreal currentDayHighTemp = currentDayDetailDayObject[QStringLiteral("weather")].toObject()
                                                                      [QStringLiteral("temperature")].toString()
                                             .toDouble(&currentDayHighTempOk);
            const bool currentDayDetailValid = updateLastValidDayCache(currentDayDetailDayObject, stationId);
            if (currentDayHighTempOk && currentDayDetailValid) {
                currentDay.setNormalHighTemp(currentDayHighTemp);
            }
            else if (lastValidDayCache.contains(stationId)) {
                qWarning(IONENGINE_NMCCN) << "Found invalid day report, using cached value instead...";
                const QJsonObject cachedDayDetailDayObject = *lastValidDayCache[stationId];
                const qreal cachedDayHighTemp = cachedDayDetailDayObject[QStringLiteral("weather")].toObject()
                                                                        [QStringLiteral("temperature")].toString()
                                                .toDouble();
                currentDay.setNormalHighTemp(cachedDayHighTemp);
            }
            const qreal currentDayLowTemp = currentDayDetail[QStringLiteral("night")].toObject()
                                                            [QStringLiteral("weather")].toObject()
                                                            [QStringLiteral("temperature")].toString()
                                            .toDouble(&currentDayLowTempOk);
            if (currentDayLowTempOk) {
                currentDay.setNormalLowTemp(currentDayLowTemp);
            }

            LastDay lastDay;
            QList<qreal> lastDayHourlyTemps;
            for(QJsonValue lastDayHourlyInfo: apiResponseData[QStringLiteral("passedchart")].toArray()) {
                lastDayHourlyTemps.append(lastDayHourlyInfo.toObject()[QStringLiteral("temperature")].toDouble());
            }
            if (lastDayHourlyTemps.count() > 0) {
                qreal min = *std::min_element(lastDayHourlyTemps.begin(), lastDayHourlyTemps.end());
                qreal max = *std::max_element(lastDayHourlyTemps.begin(), lastDayHourlyTemps.end());
                lastDay.setNormalHighTemp(max);
                lastDay.setNormalLowTemp(min);
            }

            LastObservation lastObservation;
            lastObservation.setObservationTimestamp(QDateTime::fromString(real[QStringLiteral("publish_time")].toString(), realDateFormat));
            const QJsonObject weather = real[QStringLiteral("weather")].toObject();
            lastObservation.setCurrentConditions(weather[QStringLiteral("info")].toString());
            const QJsonObject sunriseSunset = real[QStringLiteral("sunriseSunset")].toObject();
            const QDateTime sunset = QDateTime::fromString(sunriseSunset[QStringLiteral("sunset")].toString(), realDateTimeFormat);
            const QDateTime sunrise = QDateTime::fromString(sunriseSunset[QStringLiteral("sunrise")].toString(), realDateTimeFormat);
            const QDateTime now = QDateTime::currentDateTime();
            const bool currentIsNight = sunset <= now || now < sunrise;
            const QJsonObject wind = real[QStringLiteral("wind")].toObject();
            const qreal windSpeed = wind[QStringLiteral("speed")].toDouble();
            const bool currentIsWindy = windSpeed > 1.6; // wind faster than 1.6m/s means windy(to human)
            const ConditionIcons currentWeatherConditionIcons =
                getWeatherConditionIcon(weather[QStringLiteral("img")].toString(), currentIsWindy, currentIsNight);
            lastObservation.setConditionIcon(getWeatherIcon(currentWeatherConditionIcons));
            lastObservation.setTemperature(weather[QStringLiteral("temperature")].toDouble());
            lastObservation.setWindchill(std::round(weather[QStringLiteral("feelst")].toDouble()));
            lastObservation.setHumidex(std::round(weather[QStringLiteral("feelst")].toDouble()));
            lastObservation.setWindSpeed(windSpeed);
            lastObservation.setWindDirection(getWindDirectionString(wind[QStringLiteral("degree")].toDouble()));
            lastObservation.setPressure(weather[QStringLiteral("airpressure")].toDouble());
            lastObservation.setHumidity(weather[QStringLiteral("humidity")].toDouble());

            std::shared_ptr<FutureDays> futureDays = std::make_shared<FutureDays>();
            if (detail.count() > 1) {
                for (int i = 1; i < detail.count(); i++) {
                    const QJsonObject detailObject = detail[i].toObject();
                    const QDate date = QDate::fromString(detailObject[QStringLiteral("date")].toString(), realDateFormat);
                    qDebug(IONENGINE_NMCCN) << "Date is" << date;
                    FutureDayForecast futureDayForecast;
                    futureDayForecast.setMonthDay(date.day());
                    futureDayForecast.setWeekDay(QLocale::system().toString(date, QStringLiteral("ddd")));
                    const QJsonObject day = detailObject[QStringLiteral("day")].toObject()[QStringLiteral("weather")].toObject();
                    FutureForecast dayForecast;
                    dayForecast.setConditionIcon(getWeatherIcon(getWeatherConditionIcon(day[QStringLiteral("img")].toString(), false, false)));
                    dayForecast.setCondition(day[QStringLiteral("info")].toString());
                    dayForecast.setHighTemp(day[QStringLiteral("temperature")].toString().toDouble());
                    futureDayForecast.setDaytime(dayForecast);
                    const QJsonObject night = detailObject[QStringLiteral("night")].toObject()[QStringLiteral("weather")].toObject();
                    FutureForecast nightForecast;
                    nightForecast.setConditionIcon(getWeatherIcon(getWeatherConditionIcon(night[QStringLiteral("img")].toString(), false, true)));
                    nightForecast.setCondition(night[QStringLiteral("info")].toString());
                    nightForecast.setLowTemp(night[QStringLiteral("temperature")].toString().toDouble());
                    futureDayForecast.setNight(nightForecast);
                    qDebug(IONENGINE_NMCCN) << "Added future day forecast weekday" << futureDayForecast.weekDay() << "with monthday" << futureDayForecast.monthDay();
                    futureDays->addDay(futureDayForecast);
                }
            }

            std::shared_ptr<Warnings> warnings = std::make_shared<Warnings>();
            const QJsonObject warnObject = real[QStringLiteral("warn")].toObject();
            updateWarnInfoCache(warnObject, stationId);
            const QList<WarnInfo> warnInfos = *warnInfoCache[stationId];
            for (WarnInfo warnInfo : warnInfos) {
                const QJsonObject warnObject = warnInfo.warnObject;
                Warning warning(getWarnPriority(warnObject[QStringLiteral("signallevel")].toString()),
                                                warnObject[QStringLiteral("alert")].toString());
                warning.setTimestamp(warnInfo.startTime.toString(realDateTimeFormat));
                warning.setInfo(QStringLiteral(API_BASE) + warnObject[QStringLiteral("url")].toString());
                warnings->addWarning(warning);
            } 

            std::shared_ptr<Forecast> forecast = std::make_shared<Forecast>();
            forecast->setMetadata(metaData);
            forecast->setStation(station);
            forecast->setCurrentDay(currentDay);
            forecast->setLastDay(lastDay);
            forecast->setLastObservation(lastObservation);
            forecast->setFutureDays(futureDays);
            forecast->setWarnings(warnings);
            qDebug(IONENGINE_NMCCN) << "Returning forecast" << forecast.get();
            forecastPromise->addResult(forecast);
        }
    }
    forecastPromise->finish();
    forecastPromise.reset();
}

void NmcCnIon::findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &searchString)
{
    promise->start();
    if (promise->isCanceled()) {
        promise->finish();
        promise.reset();
        return;
    }
    qDebug(IONENGINE_NMCCN) << "Finding place" << searchString << "...";
    locationsPromise = promise;
    connect(&networkAccessManager, &QNetworkAccessManager::finished,
            this, &NmcCnIon::onSearchApiRequestFinished,
            networkAccessManagerSlotConnectionType);
    requestSearchingPlacesApi(searchString);
}

void NmcCnIon::fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo)
{
    promise->start();
    if (promise->isCanceled()) {
        promise->finish();
        promise.reset();
        return;
    }
    qDebug(IONENGINE_NMCCN) << "Fetching weather for place" << placeInfo << "...";
    forecastPromise = promise;
    const bool placeInfoIsLegacy = placeInfo.contains(QLatin1Char(placeInfoSep));
    // id|refererPath|lon|lat
    const QString extra = placeInfoIsLegacy ? placeInfo.split(QLatin1Char(placeInfoSep))[3] : placeInfo;
    const QStringList extraParts = extra.split(QLatin1Char(extraDataSep));
    const QString stationId = extraParts[0];
    const QString creditPage = QStringLiteral(FORECAST_CITY_PAGE) + extraParts[1];
    connect(&networkAccessManager, &QNetworkAccessManager::finished,
            this, [=](QNetworkReply* reply){this->onWeatherApiRequestFinished(reply, extra, placeInfoIsLegacy);},
            networkAccessManagerSlotConnectionType);
    requestWeatherApi(stationId, creditPage);
}
#endif // ION_LEGACY

#include "moc_nmccn.cpp"
#include "nmccn.moc"
