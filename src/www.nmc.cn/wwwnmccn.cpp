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
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>

#include "wwwnmccn.hpp"

WwwNmcCnIon::WwwNmcCnIon(QObject *parent)
    : IonInterface(parent)
{
    networkAccessManager.setParent(this);
    setInitialized(true);
}

WwwNmcCnIon::~WwwNmcCnIon()
{
#ifdef ION_LEGACY
    reset();
#else // ION_LEGACY
    warnInfoCache.clear();
#endif // ION_LEGACY
}

WwwNmcCnIon::ConditionIcons WwwNmcCnIon::getWeatherConditionIcon(const QString &img, const bool windy, const bool night) const
{
    bool ok;
    const int imgId = img.toInt(&ok);
    if (img != INVALID_VALUE_STR && ok) {
        switch (imgId)
        {
            case 0:
                if (windy && night) {
                    return WwwNmcCnIon::ConditionIcons::ClearWindyNight;
                }
                else if (windy) {
                    return WwwNmcCnIon::ConditionIcons::ClearWindyDay;
                }
                else if (night) {
                    return WwwNmcCnIon::ConditionIcons::ClearNight;
                }
                else {
                    return WwwNmcCnIon::ConditionIcons::ClearDay;
                }
            case 1:
                if (windy && night) {
                    return WwwNmcCnIon::ConditionIcons::PartlyCloudyWindyNight;
                }
                else if (windy) {
                    return WwwNmcCnIon::ConditionIcons::PartlyCloudyWindyDay;
                }
                else if (night) {
                    return WwwNmcCnIon::ConditionIcons::PartlyCloudyNight;
                }
                else {
                    return WwwNmcCnIon::ConditionIcons::PartlyCloudyDay;
                }
            case 2:
                if (windy) {
                    return WwwNmcCnIon::ConditionIcons::OvercastWindy;
                }
                else {
                    return WwwNmcCnIon::ConditionIcons::Overcast;
                }
            case 3:
                if (night) {
                    return WwwNmcCnIon::ConditionIcons::ChanceShowersNight;
                }
                else {
                    return WwwNmcCnIon::ConditionIcons::ChanceShowersDay;
                }
            case 4:
                if (night) {
                    return WwwNmcCnIon::ConditionIcons::ChanceThunderstormNight;
                }
                else {
                    return WwwNmcCnIon::ConditionIcons::ChanceThunderstormDay;
                }
            //case 5:
                // TODO
            //case 6:
                // TODO
            case 7:
                return WwwNmcCnIon::ConditionIcons::LightRain;
            case 8:
                return WwwNmcCnIon::ConditionIcons::Rain;
            case 9:
            case 10:
                return WwwNmcCnIon::ConditionIcons::Thunderstorm;
            default:
                return WwwNmcCnIon::ConditionIcons::NotAvailable;
        }
    }
    else {
        qDebug(IONENGINE_WWWNMCCN) << "Failed to parse img id: " << img;
        return WwwNmcCnIon::ConditionIcons::NotAvailable;
    }
}

QNetworkReply *WwwNmcCnIon::requestSearchingPlacesApi(const QString &searchString, const int searchLimit)
{
    const QString encodedSearchString = searchString.toUtf8().toPercentEncoding();
    QNetworkRequest request;
    QUrl url(SEARCH_API);
    QUrlQuery query;
    query.addQueryItem("q", encodedSearchString);
    query.addQueryItem("limit", ((QString)"%1").arg(searchLimit));
    query.addQueryItem("timestamp", ((QString)"%1").arg(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem("_", ((QString)"%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    qDebug(IONENGINE_WWWNMCCN) << "Requesting url:" << url;
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, FORECAST_PAGE);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::ContentType, "application/json;charset=utf-8");
    request.setHeaders(headers);
    qDebug(IONENGINE_WWWNMCCN) << "Requesting headers:" << headers;
    return networkAccessManager.get(request);
}

QNetworkReply *WwwNmcCnIon::requestWeatherApi(const QString &stationId, const QString &referer)
{
    QNetworkRequest request;
    QUrl url(WEATHER_API);
    QUrlQuery query;
    query.addQueryItem("stationid", stationId);
    query.addQueryItem("_", ((QString)"%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, referer);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::ContentType, "application/json;charset=utf-8");
    request.setHeaders(headers);
    return networkAccessManager.get(request);
}

QNetworkReply *WwwNmcCnIon::requestWebPage(const QUrl &webPage)
{
    QNetworkRequest request;
    request.setUrl(webPage);
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, FORECAST_PAGE);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::ContentType, "text/html;charset=utf-8");
    request.setHeaders(headers);
    return networkAccessManager.get(request);
}

QJsonArray WwwNmcCnIon::extractSearchApiResponse(QNetworkReply *reply)
{
    qDebug(IONENGINE_WWWNMCCN) << "Setting searching data based on reply of url: " << reply->url();
    QJsonParseError error;
    const QJsonObject responseObject = QJsonDocument::fromJson(reply->readAll(), &error).object();
    switch (responseObject["code"].toInt(-1)) {
        case 0:
            return responseObject["data"].toArray();
        case -1:
            qFatal(IONENGINE_WWWNMCCN) << "Failed to parse json at" << error.offset << "because:" << error.errorString();
        default:
            qFatal(IONENGINE_WWWNMCCN) << "API response invalid: " << responseObject["msg"].toString();
    }
    QJsonArray ret;
    return ret;
}

QJsonObject WwwNmcCnIon::extractWeatherApiResponse(QNetworkReply *reply)
{
    qDebug(IONENGINE_WWWNMCCN) << "Setting weather api data based on reply of url: " << reply->url();
    QJsonParseError error;
    const QJsonObject responseObject = QJsonDocument::fromJson(reply->readAll(), &error).object();
    switch (responseObject["code"].toInt(-1)) {
        case 0:
            return responseObject["data"].toObject();
        case -1:
            qFatal(IONENGINE_WWWNMCCN) << "Failed to parse json at" << error.offset << "because:" << error.errorString();
        default:
            qFatal(IONENGINE_WWWNMCCN) << "API response invalid: " << responseObject["msg"].toString();
    }
    QJsonObject ret;
    return ret;
}

QList<HourlyInfo> WwwNmcCnIon::extractWebPage(QNetworkReply *reply)
{
    qDebug(IONENGINE_WWWNMCCN) << "Setting weather api data based on reply of url: " << reply->url();
    QList<HourlyInfo> ret;
    const char *url = reply->url().toString().toLocal8Bit().data();
    const QByteArray pageContent = reply->readAll();
    const xmlDocPtr pageDoc = htmlReadMemory(pageContent, pageContent.size(), url, nullptr, HTML_PARSE_NOERROR);
    const xmlXPathContextPtr xPathContext = xmlXPathNewContext(pageDoc);
    const xmlXPathObjectPtr day0DivElementXPath = xmlXPathEvalExpression((xmlChar *)"//div[@id=\"day0\"]/div[contains(@class, \"hour3\")]", xPathContext);
    if (day0DivElementXPath->nodesetval->nodeNr >= 9) {
        bool returnList = true;
        for (int i=0; i<day0DivElementXPath->nodesetval->nodeNr; i++) {
            const xmlNodePtr node = day0DivElementXPath->nodesetval->nodeTab[i];
            const QString classPropValue = (char *)xmlGetProp(node, (xmlChar *)"class");
            HourlyInfo info;

            // 23:00
            info.time = classPropValue.contains("hbg") ?
                        QDateTime::currentDateTime() :
                        QDateTime::currentDateTime().addDays(1);
            const xmlNodePtr timeNode = &(node->children[0]);
            const QString timeContent = ((QString)(char *)timeNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found time in page: " << timeContent;
            info.time.setTime(QTime::fromString(timeContent, realTimeFormat));
            xmlFreeNode(timeNode);
            if (!info.time.isValid()) {
                returnList = false;
                break;
            }
            
            // https://image.nmc.cn/assets/img/w/40x40/3/0.png
            const xmlNodePtr imgNode = &(node->children[1].children[0]);
            const QUrl src = (QString)(char *)xmlGetProp(imgNode, (xmlChar *)"src");
            qDebug(IONENGINE_WWWNMCCN) << "Found img src: " << src;
            const QString fileName = src.fileName();
            info.img = fileName.left(fileName.lastIndexOf('.'));
            xmlFreeNode(imgNode);

            // -
            // 0.9mm
            const xmlNodePtr rainNode = &(node->children[2]);
            const QString rainContent = ((QString)(char *)rainNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found rain: " << rainContent;
            bool rainOk;
            info.rain = rainContent == "-" ? 0 : rainContent.left(rainContent.length() - 2).toDouble(&rainOk);
            xmlFreeNode(rainNode);
            if (rainContent != "-" && !rainOk) {
                returnList = false;
                break;
            }

            // 28.1℃
            const xmlNodePtr temperatureNode = &(node->children[3]);
            const QString temperatureContent = ((QString)(char *)temperatureNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found temperature: " << temperatureContent;
            bool temperatureOk;
            info.temperature = temperatureContent.left(temperatureContent.length() - 1).toDouble(&temperatureOk);
            xmlFreeNode(temperatureNode);
            if (!temperatureOk) {
                returnList = false;
                break;
            }

            // 2.9m/s
            const xmlNodePtr windSpeedNode = &(node->children[4]);
            const QString windSpeedContent = ((QString)(char *)windSpeedNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found wind speed: " << windSpeedContent;
            bool windSpeedOk;
            info.windSpeed = windSpeedContent.left(windSpeedContent.length() - 3).toDouble(&windSpeedOk);
            xmlFreeNode(windSpeedNode);
            if (!windSpeedOk) {
                returnList = false;
                break;
            }

            // 北风
            const xmlNodePtr windDirectNode = &(node->children[5]);
            const QString windDirectContent = ((QString)(char *)windDirectNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found wind direct: " << windDirectContent;
            info.windDirect = windDirectContent;
            xmlFreeNode(windDirectNode);

            // 963.7hPa
            const xmlNodePtr atmosphericPressureNode = &(node->children[6]);
            const QString atmosphericPressureContent = ((QString)(char *)atmosphericPressureNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found atmospheric pressure: " << atmosphericPressureContent;
            bool atmosphericPressureOk;
            info.atmosphericPressure = atmosphericPressureContent.left(atmosphericPressureContent.length() - 3).toDouble(&atmosphericPressureOk);
            xmlFreeNode(atmosphericPressureNode);
            if (!atmosphericPressureOk) {
                returnList = false;
                break;
            }

            // 81.1%
            const xmlNodePtr humidityNode = &(node->children[7]);
            const QString humidityContent = ((QString)(char *)humidityNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found humidity: " << humidityContent;
            bool humidityOk;
            info.humidity = humidityContent.left(humidityContent.length() - 1).toDouble(&humidityOk);
            xmlFreeNode(humidityNode);
            if (!humidityOk) {
                returnList = false;
                break;
            }

            // 10.1%
            const xmlNodePtr possibilityNode = &(node->children[8]);
            const QString possibilityContent = ((QString)(char *)possibilityNode->content).trimmed();
            qDebug(IONENGINE_WWWNMCCN) << "Found possibility: " << possibilityContent;
            bool possibilityOk;
            info.possibility = possibilityContent.left(possibilityContent.length() - 1).toDouble(&possibilityOk);
            xmlFreeNode(possibilityNode);
            if (!possibilityOk) {
                returnList = false;
                break;
            }

            ret.append(info);
        }
        if (!returnList) {
            ret.clear();
        }
    }
    xmlFreeDoc(pageDoc);
    xmlCleanupParser();
    return ret;
}

bool WwwNmcCnIon::updateWarnInfoCache(const QJsonObject &warnObject, const QString &stationId)
{
    QRegularExpression invalidValueRegex("^" INVALID_VALUE_STR "$");
    const QString warnObjectAlert = warnObject["alert"].toString().remove(invalidValueRegex);
    const QString warnObjectCity = warnObject["city"].toString().remove(invalidValueRegex);
    const QString warnObjectFmeans = warnObject["fmeans"].toString().remove(invalidValueRegex);
    const QString warnObjectIssueContent = warnObject["issuecontent"].toString().remove(invalidValueRegex);
    const QString warnObjectPic = warnObject["pic"].toString().remove(invalidValueRegex);
    const QString warnObjectPic2 = warnObject["pic2"].toString().remove(invalidValueRegex);
    const QString warnObjectProvince = warnObject["province"].toString().remove(invalidValueRegex);
    const QString warnObjectSignalLevel = warnObject["signallevel"].toString().remove(invalidValueRegex);
    const QString warnObjectSignalType = warnObject["signaltype"].toString().remove(invalidValueRegex);
    const QString warnObjectUrl = warnObject["url"].toString().remove(invalidValueRegex);
    const bool warnObjectValid = !warnObjectAlert.isEmpty() && !warnObjectCity.isEmpty() && !warnObjectFmeans.isEmpty() &&
                                 !warnObjectIssueContent.isEmpty() && !warnObjectPic.isEmpty() && !warnObjectPic2.isEmpty() &&
                                 !warnObjectProvince.isEmpty() && !warnObjectSignalLevel.isEmpty() && !warnObjectSignalType.isEmpty() &&
                                 !warnObjectUrl.isEmpty();
    bool warnExists = false;
    if (!warnInfoCache.contains(stationId)) {
        QList<WarnInfo> *warnInfos = new QList<WarnInfo>;
        warnInfoCache.insert(stationId, warnInfos); 
    }
    QList<WarnInfo> *warnInfos = warnInfoCache[stationId];
    const QDateTime now = QDateTime::currentDateTime();
    for (int i=0; i<warnInfos->count();i++) {
        const WarnInfo warnInfo = warnInfos->at(i);
        const QString warnSignalType = warnInfo.warnObject["signaltype"].toString();
        const QString warnSignalLevel = warnInfo.warnObject["signallevel"].toString();
        if (now > warnInfo.startTime.date().endOfDay()) {
            const QString warnDescription = warnSignalType + "-" + warnSignalLevel;
            qDebug(IONENGINE_WWWNMCCN) << "Removing outdated warn: " << warnDescription;
                warnInfos->remove(i);
            }
        else if (warnSignalType == warnObjectSignalType && warnSignalLevel == warnObjectSignalLevel) {
            warnExists = true;
        }
    }
    warnInfos->squeeze();
    if (!warnExists && warnObjectValid) {
        WarnInfo warn;
        warn.warnObject = warnObject;
        warn.startTime = QDateTime::currentDateTime();
        warnInfos->append(warn);
        return true;
    }
    return false;
}

#ifdef ION_LEGACY

K_PLUGIN_CLASS_WITH_JSON(WwwNmcCnIon, "metadata.legacy.json");

void WwwNmcCnIon::onSearchApiRequestFinished(QNetworkReply *reply, const QString &source)
{
    std::function<QJsonArray(QNetworkReply*)> wrapper = [=](QNetworkReply* r){return this->extractSearchApiResponse(r);};
    QJsonArray searchResult = handleNetworkReply(reply, wrapper);
    qDebug(IONENGINE_WWWNMCCN) << "Deserialized data json array:" << searchResult;
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
    qDebug(IONENGINE_WWWNMCCN) << "Setting data:" << dataStringToSet << "for source:" << source;
    setData(source, "validate", dataStringToSet);
}

void WwwNmcCnIon::onWeatherApiRequestFinished(QNetworkReply *reply, const QString &source, const QString &creditUrl, const bool callSetData)
{
    std::function<QJsonObject(QNetworkReply*)> wrapper = [=](QNetworkReply* r){return this->extractWeatherApiResponse(r);};
    QJsonObject apiResponseData = handleNetworkReply(reply, wrapper);
    qDebug(IONENGINE_WWWNMCCN) << "Deserialized data json object:" << apiResponseData;
    if (!apiResponseData.isEmpty()) {
        if (!dataCache.contains(source)) {
            Plasma5Support::DataEngine::Data *emptyData = new Plasma5Support::DataEngine::Data;
            dataCache.insert(source, emptyData);
        }
        Plasma5Support::DataEngine::Data *data = dataCache[source];
        data->insert("Credit", ION_NAME);
        data->insert("Credit Url", creditUrl);
        data->insert("Country", i18n("China"));
        const QJsonObject real = apiResponseData["real"].toObject();
        const QJsonObject station = real["station"].toObject();
        data->insert("Place", station["city"].toString());
        data->insert("Region", station["province"].toString());
        data->insert("Station", station["city"].toString());
        data->insert("Observation Period", real["publish_time"].toString());
        const QJsonObject sunriseSunset = real["sunriseSunset"].toObject();
        const QDateTime sunset = QDateTime::fromString(sunriseSunset["sunset"].toString(), realDateTimeFormat);
        const QDateTime publishTime = QDateTime::fromString(real["publish_time"].toString(), realDateTimeFormat);
        const QDateTime sunrise = QDateTime::fromString(sunriseSunset["sunrise"].toString(), realDateTimeFormat);
        const QJsonObject weather = real["weather"].toObject();
        const QJsonObject wind = real["wind"].toObject();
        data->insert("Current Conditions", getWeatherConditionIcon(weather["img"].toString(),
                                                                  sunset <= publishTime && publishTime <= sunrise,
                                                                  wind["speed"].toDouble() > 0));
        data->insert("Temperature", weather["temperature"].toDouble());
        data->insert("Temperature Unit", KUnitConversion::Celsius);
        data->insert("Windchill", std::round(weather["feelst"].toDouble()));
        data->insert("Humidex", std::round(weather["feelst"].toDouble()));
        data->insert("Wind Direction", wind["direct"].toString());
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
            const QJsonObject day = detailObject["day"].toObject();
            const QJsonObject dayWeather = day["weather"].toObject();
            const QJsonObject dayWind = day["wind"].toObject();
            const bool dayWindy = dayWind["power"].toString() != "无持续风向";
            const double dayWeatherTemperature = dayWeather["temperature"].toString().toDouble();
            const QJsonObject night = detailObject["night"].toObject();
            const QJsonObject nightWeather = night["weather"].toObject();
            const QJsonObject nightWind = night["wind"].toObject();
            const bool nightWindy = nightWind["power"].toString() != "无持续风向";
            const double nightWeatherTemperature = nightWeather["temperature"].toString().toDouble();
            const QString dayWeatherIcon = getWeatherIcon(getWeatherConditionIcon(dayWeather["img"].toString(), dayWindy, false));
            if (i > 0) {
                const QString dayWeatherForecastRelatedValue =
                    forecastRelatedValueTemplate
                        .arg(QDate::fromString(detailObject["date"].toString(), realDateFormat).day())
                        .arg(dayWeatherIcon)
                        .arg(dayWeather["info"].toString())
                        .arg(std::max(dayWeatherTemperature, nightWeatherTemperature))
                        .arg(std::min(dayWeatherTemperature, nightWeatherTemperature))
                        .arg("N/U");
                data->insert(forecastRelatedKey, dayWeatherForecastRelatedValue);
            }
            else {                
                const QString dayWeatherForecastRelatedValue =
                    forecastRelatedValueTemplate
                        .arg(i18n("Day"))
                        .arg(dayWeatherIcon)
                        .arg(dayWeather["info"].toString())
                        .arg(dayWeatherTemperature)
                        .arg("N/U")
                        .arg("N/U");
                data->insert(forecastRelatedKey, dayWeatherForecastRelatedValue);
                const QString nightWeatherIcon = getWeatherIcon(getWeatherConditionIcon(nightWeather["img"].toString(), nightWindy, true));
                const QString nightWeatherForecastRelatedValue =
                    forecastRelatedValueTemplate
                        .arg(i18n("Night"))
                        .arg(nightWeatherIcon)
                        .arg(nightWeather["info"].toString())
                        .arg(nightWeatherTemperature)
                        .arg("N/U")
                        .arg("N/U");
                data->insert(forecastRelatedKey, nightWeatherForecastRelatedValue);
            }
        }
        data->insert("Total Weather Days", detail.count());
        const QString stationId = station["code"].toString();
        const QJsonObject warnObject = real["warn"].toObject();
        updateWarnInfoCache(warnObject, stationId);
        const QList<WarnInfo> *warnInfos = warnInfoCache[stationId];
        const QString warningRelatedDescriptionKeyTemplate = "Warning Description %1";
        const QString warningRelatedInfoKeyTemplate = "Warning Info %1";
        for (int i=0; i<warnInfos->count(); i++) {
            const WarnInfo warnInfo = warnInfos->at(i);
            const QString warningRelatedDescriptionKey = warningRelatedDescriptionKeyTemplate.arg(i);
            const QString warningRelatedDescriptionValue = warnInfo.warnObject["signaltype"].toString() + "-" + warnInfo.warnObject["signallevel"].toString();
            data->insert(warningRelatedDescriptionKey, warningRelatedDescriptionValue);
            const QString warningRelatedInfoKey = warningRelatedInfoKeyTemplate.arg(i);
            const QString warningRelatedInfoValue = API_BASE + warnInfo.warnObject["url"].toString();
            data->insert(warningRelatedInfoKey, warningRelatedInfoValue);
        }
        data->insert("Total Warnings Issued", warnInfos->count());
        if (callSetData) {
            Q_EMIT cleanUpData(source);
            const QStringList weatherSourceParts = {ION_NAME, "weather", source.split(sourceSep)[2], source.split(sourceSep)[3]};
            const QString weatherSource = weatherSourceParts.join(sourceSep);
            qDebug(IONENGINE_WWWNMCCN) << "Responding source: " << source;
            setData(source, *data);
            dataCache.remove(source);
        }
    }
}

void WwwNmcCnIon::onWebPageRequestFinished(QNetworkReply *reply, const QString &source, const bool callSetData)
{
    std::function<QList<HourlyInfo>(QNetworkReply*)> wrapper = [=](QNetworkReply* r){return this->extractWebPage(r);};
    QList<HourlyInfo> hourlyInfos = handleNetworkReply(reply, wrapper);
    
}

bool WwwNmcCnIon::updateIonSource(const QString &source)
{
    // source format:
    // ionname|validate|place_name|extra - Triggers validation of place
    // ionname|weather|place_name|extra - Triggers receiving weather of place
    // See also: https://techbase.kde.org/Projects/Plasma/Weather/Ions
    qDebug(IONENGINE_WWWNMCCN) << "Update source: " << source;
    const char extraDataSep = ';';
    QStringList splitSource = source.split(sourceSep);
    if (splitSource.count() >= 3) {
        const QString requestName = splitSource[1];
        const Qt::ConnectionType networkAccessManagerSlotConnectionType = (Qt::ConnectionType)(Qt::AutoConnection | Qt::SingleShotConnection);
        if (requestName == "validate") {
            qDebug(IONENGINE_WWWNMCCN) << "Responsing validate request...";
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
                qDebug(IONENGINE_WWWNMCCN) << "Responsing weather request...";
                const QString creditPage = FORECAST_CITY_PAGE + splitExtraData[1];

                /*
                connect(&networkAccessManager, &QNetworkAccessManager::finished, this,
                        [=](QNetworkReply *reply) {this->onWebPageRequestFinished(reply, source, false);},
                        networkAccessManagerSlotConnectionType);
                requestWebPage(creditPage);
                */
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
    qDebug(IONENGINE_WWWNMCCN) << "Responsing invalid request with: " << value;
    setData(source, "validate", value);
    return true;
}

void WwwNmcCnIon::reset()
{
    warnInfoCache.clear();
    dataCache.clear();
}
#else // ION_LEGACY

K_PLUGIN_CLASS_WITH_JSON(WwwNmcCnIon, "metadata.json");

void WwwNmcCnIon::findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Locations>>> promise, const QString &searchString)
{
    
}

void WwwNmcCnIon::fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo)
{
    
}
#endif // ION_LEGACY

#include "moc_wwwnmccn.cpp"
#include "wwwnmccn.moc"
