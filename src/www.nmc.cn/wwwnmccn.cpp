#include <KPluginFactory>
#include <KUnitConversion/Converter>
#include <KUnitConversion/Unit>
#include <KLocalizedString>
#include <QDateTime>
#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTime>
#include <QUrlQuery>
#include <QWaitCondition>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>

#include "wwwnmccn.hpp"
#include "wwwnmccn_debug.hpp"

WwwNmcCnIon::WwwNmcCnIon(QObject *parent)
    : IonInterface(parent)
{
    networkAccessManager->setParent(this);
    connect(networkAccessManager, &QNetworkAccessManager::finished, this, &WwwNmcCnIon::onNetworkRequestFinished);
    setInitialized(true);
}

WwwNmcCnIon::~WwwNmcCnIon()
{
}

WwwNmcCnIon::ConditionIcons WwwNmcCnIon::getWeatherConditionIcon(const QString &img, bool windy, bool night) const
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

void WwwNmcCnIon::onNetworkRequestFinished(QNetworkReply *reply)
{
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        QVariant contentType = reply->header(QNetworkRequest::ContentTypeHeader);
        if (contentType.isValid()) {
            const QString urlWithoutQuery = reply->url().scheme() + "://" + reply->url().host() + reply->url().path();
            if (contentType == "application/json") {
                const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
                if (document.isObject()) {
                    const QJsonObject object = document.object();
                    const int code = object["code"].toInt(-1);
                    const QString msg = object["msg"].toString();
                    if (code == 0) {
                        const QJsonValue data = object["data"];
                        if (data.isArray() && urlWithoutQuery == SEARCH_API) {
                            // It is searching stations
                            const QUrlQuery searchStringQuery(reply->url());
                            if (searchStringQuery.hasQueryItem("q")) {   
                                const QString q = searchStringQuery.queryItemValue("q");
                                StationSearchApiResponse stationSearchApiResponse;
                                stationSearchApiResponse.code = code;
                                stationSearchApiResponse.msg = msg;
                                for (const QJsonValue station: data.toArray()) {
                                    if (station.isString()) {
                                        stationSearchApiResponse.data.append(station.toString());
                                    }
                                }
                                readWriteLock.lockForWrite();
                                searchedStations.insert(q, &stationSearchApiResponse);
                                readWriteLock.unlock();
                            }
                            else {
                                qFatal(IONENGINE_WWWNMCCN) << "No search string found in url query.";
                            }
                        }
                        else if (data.isObject() && urlWithoutQuery == WEATHER_API) {
                            // It is requesting weather api
                            const QJsonObject dataObject = data.toObject();
                            WeatherApiResponse response;
                            response.msg = msg;
                            response.code = code;
                            // data.real
                            const QJsonObject real = dataObject["real"].toObject();
                            // data.real.station
                            const QJsonObject realStation = real["station"].toObject();
                            response.data.real.station.code = realStation["code"].toString();
                            response.data.real.station.province = realStation["province"].toString();
                            response.data.real.station.city = realStation["city"].toString();
                            response.data.real.station.url = realStation["url"].toString();
                            // data.real.publish_time
                            response.data.real.publish_time = QDateTime::fromString(real["publish_time"].toString());
                            // data.real.weather
                            const QJsonObject realWeather = real["weather"].toObject();
                            response.data.real.weather.temperature = realWeather["temperature"].toDouble();
                            response.data.real.weather.temperatureDiff = realWeather["temperatureDiff"].toDouble();
                            response.data.real.weather.airpressure = realWeather["airpressure"].toInt();
                            response.data.real.weather.humidity = realWeather["humidity"].toInt();
                            response.data.real.weather.rain = realWeather["rain"].toDouble();
                            response.data.real.weather.rcomfort = realWeather["rcomfort"].toInt();
                            response.data.real.weather.icomfort = realWeather["icomfort"].toInt();
                            response.data.real.weather.info = realWeather["info"].toString();
                            response.data.real.weather.img = realWeather["img"].toString();
                            response.data.real.weather.feelst = realWeather["feelst"].toDouble();
                            // data.real.wind
                            const QJsonObject realWind = real["wind"].toObject();
                            response.data.real.wind.direct = realWind["direct"].toString();
                            response.data.real.wind.degree = realWind["degree"].toDouble();
                            response.data.real.wind.power = realWind["power"].toString();
                            response.data.real.wind.speed = realWind["speed"].toDouble();                            
                            // data.real.warn
                            const QJsonObject realWarn = real["warn"].toObject();
                            response.data.real.warn.alert = realWarn["alert"].toString();
                            response.data.real.warn.pic = realWarn["pic"].toString();
                            response.data.real.warn.province = realWarn["province"].toString();
                            response.data.real.warn.city = realWarn["city"].toString();
                            response.data.real.warn.url = realWarn["url"].toString();
                            response.data.real.warn.issuecontent = realWarn["issuecontent"].toString();
                            response.data.real.warn.fmeans = realWarn["fmeans"].toString();
                            response.data.real.warn.signaltype = realWarn["signaltype"].toString();
                            response.data.real.warn.signallevel = realWarn["signallevel"].toString();
                            response.data.real.warn.pic2 = realWarn["pic2"].toString();
                            // API does not provide this, faking one.
                            response.data.real.warn.expireTime = QDate::currentDate().endOfDay();
                            // data.real.sunriseSunset
                            const QJsonObject realSunriseSunset = real["sunriseSunset"].toObject();
                            response.data.real.sunriseSunset.sunrise = QDateTime::fromString(realSunriseSunset["sunrise"].toString());
                            response.data.real.sunriseSunset.sunset = QDateTime::fromString(realSunriseSunset["sunset"].toString());
                            // data.predict
                            const QJsonObject predict = dataObject["predict"].toObject();
                            // data.predict.station
                            const QJsonObject predictStation = predict["station"].toObject();
                            response.data.predict.station.code = predictStation["code"].toString();
                            response.data.predict.station.province = predictStation["province"].toString();
                            response.data.predict.station.city = predictStation["city"].toString();
                            response.data.predict.station.url = predictStation["url"].toString();
                            // data.predict.publish_time
                            response.data.predict.publish_time = QDateTime::fromString(predict["publish_time"].toString());
                            // data.predict.detail
                            response.data.predict.detail.clear();
                            for (const QJsonValue detailValue: predict["detail"].toArray()) {
                                const QJsonObject detailObject = detailValue.toObject();

                                DetailInfo detail;
                                detail.date = QDate::fromString(detailObject["date"].toString());
                                detail.pt = QDateTime::fromString(detailObject["pt"].toString());
                                const QJsonObject detailDay = detailValue["day"].toObject();
                                const QJsonObject detailDayWeather = detailDay["weather"].toObject();
                                detail.day.weather.info = detailDayWeather["info"].toString();
                                detail.day.weather.img = detailDayWeather["img"].toString();
                                detail.day.weather.temperature = detailDayWeather["temperature"].toDouble();
                                const QJsonObject detailDayWind = detailDay["wind"].toObject();
                                detail.day.wind.direct = detailDayWind["direct"].toString();
                                detail.day.wind.power = detailDayWind["power"].toString();
                                const QJsonObject detailNight = detailValue["night"].toObject();
                                const QJsonObject detailNightWeather = detailNight["weather"].toObject();
                                detail.night.weather.info = detailNightWeather["info"].toString();
                                detail.night.weather.img = detailNightWeather["img"].toString();
                                detail.night.weather.temperature = detailNightWeather["temperature"].toDouble();
                                const QJsonObject detailNightWind = detailNight["wind"].toObject();
                                detail.night.wind.direct = detailNightWind["direct"].toString();
                                detail.night.wind.power = detailNightWind["power"].toString();
                                detail.precipitation = detailObject["precipitation"].toDouble();
                                response.data.predict.detail.append(detail);
                            }
                            // data.air
                            const QJsonObject air = dataObject["air"].toObject();
                            response.data.air.forecasttime = QDateTime::fromString(air["forecasttime"].toString());
                            response.data.air.aqi = air["aqi"].toInt();
                            response.data.air.aq = air["aq"].toInt();
                            response.data.air.text = air["text"].toString();
                            response.data.air.aqiCode = air["aqiCode"].toString();
                            // data.tempchart
                            const QJsonArray tempChart = data["tempchart"].toArray();
                            response.data.tempchart.clear();
                            for (const QJsonValue tempChartValue: tempChart) {
                                QJsonObject tempChartObject = tempChartValue.toObject();
                                TempChartInfo chart;
                                chart.time = QDate::fromString(tempChartObject["time"].toString());
                                chart.max_temp = tempChartObject["max_temp"].toDouble();
                                chart.min_temp = tempChartObject["min_temp"].toDouble();
                                chart.day_img = tempChartObject["day_img"].toString();
                                chart.day_text = tempChartObject["day_text"].toString();
                                chart.night_img = tempChartObject["night_img"].toString();
                                chart.night_text = tempChartObject["night_text"].toString();
                                response.data.tempchart.append(chart);
                            }
                            readWriteLock.lockForWrite();
                            weathers.insert(response.data.real.station.code, &response);
                            readWriteLock.unlock();
                        }
                        else {
                            qFatal(IONENGINE_WWWNMCCN) << "Invalid data type: " << data.type() << " with requesting url " << urlWithoutQuery;
                        }
                    }
                    else {
                        qFatal(IONENGINE_WWWNMCCN) << "API response invalid: " << msg;
                    } 
                }
            }
            else if (contentType == "text/html" && urlWithoutQuery.startsWith(FORECAST_CITY_PAGE)) {
                // It is requesting weather webpage
                const QByteArray data = reply->readAll();
                const htmlDocPtr doc = htmlReadMemory(data, data.length(), nullptr, nullptr, HTML_PARSE_NOERROR);
                if (doc != NULL) {
                    const xmlXPathContextPtr context = xmlXPathNewContext(doc);
                    if (context != NULL) {
                        const xmlXPathObjectPtr stationIdElementXPath = xmlXPathEvalExpression((xmlChar *)"//input[@name=\"stationId\"]", context);
                        const xmlNodePtr stationIdElement = stationIdElementXPath->nodesetval->nodeTab[0];
                        const QString stationId = ((QString)((char *)xmlGetProp(stationIdElement, (xmlChar *)"value"))).trimmed();
                        HourlyInfoList hourlyInfos;
                        qDebug(IONENGINE_WWWNMCCN) << "Found stationId: " << stationId << " from web page.";
                        xmlFreeNode(stationIdElement);
                        xmlXPathFreeObject(stationIdElementXPath);
                        const xmlXPathObjectPtr day0DivElementXPath = xmlXPathEvalExpression((xmlChar *)"//div[@id=\"day0\"]/div[contains(@class, \"hour3\")]", context);
                        for (int i = 0; i < day0DivElementXPath->nodesetval->nodeNr; i++) {
                            const xmlNodePtr node = day0DivElementXPath->nodesetval->nodeTab[i];
                            const QString classProp = (char *)xmlGetProp(node, (xmlChar *)"class");
                            HourlyInfo hourlyInfo;
                            // 23:00
                            hourlyInfo.time = classProp.contains("hbg") ? QDateTime::currentDateTime() : QDateTime::currentDateTime().addDays(1);
                            const xmlNodePtr timeNode = &(node->children[0]);
                            const QString timeContent = ((QString)((char *)timeNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found time: " << timeContent;
                            const QTime time = QTime::fromString(timeContent);
                            hourlyInfo.time.setTime(time);
                            // https://image.nmc.cn/assets/img/w/40x40/3/0.png
                            const xmlNodePtr imgNode = &(node->children[1].children[0]);
                            const QUrl src = (QString)((char *)xmlGetProp(imgNode, (xmlChar *)"src"));
                            qDebug(IONENGINE_WWWNMCCN) << "Found img src: " << src;
                            const QString fileName = src.fileName();
                            hourlyInfo.img = fileName.left(fileName.lastIndexOf("."));
                            xmlFreeNode(imgNode);
                            // -
                            const xmlNodePtr rainNode = &(node->children[2]);
                            const QString rainContent = ((QString)((char *)rainNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found rain: " << rainContent;
                            bool rainOk;
                            const double rain = rainContent.toDouble(&rainOk);
                            hourlyInfo.rain = rainOk ? rain: -1; 
                            xmlFreeNode(rainNode);
                            // 28.1℃
                            const xmlNodePtr temperatureNode = &(node->children[3]);
                            const QString temperatureContent = ((QString)((char *)temperatureNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found temperature: " << temperatureContent;
                            bool temperatureOk;
                            const double temperature = temperatureContent.left(fileName.length() - 1).toDouble(&temperatureOk);
                            hourlyInfo.temperature = temperatureOk ? temperature : -1;
                            xmlFreeNode(temperatureNode);
                            // 2.9m/s
                            const xmlNodePtr windSpeedNode = &(node->children[4]);
                            const QString windSpeedContent = ((QString)((char *)windSpeedNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found wind speed: " << windSpeedContent;
                            bool windSpeedOk;
                            const double windSpeed = windSpeedContent.left(windSpeedContent.length() - 3).toDouble(&windSpeedOk);
                            hourlyInfo.windSpeed = windSpeedOk ? windSpeed : -1;
                            xmlFreeNode(windSpeedNode);
                            // 北风
                            const xmlNodePtr windDirectNode = &(node->children[5]);
                            const QString windDirectContent = ((QString)((char *)windDirectNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found wind direct: " << windDirectContent;
                            hourlyInfo.windDirect = windDirectContent;
                            xmlFreeNode(windDirectNode);
                            // 963.7hPa
                            const xmlNodePtr pressureNode = &(node->children[6]);
                            const QString pressureContent = ((QString)((char *)pressureNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found pressure: " << pressureContent;
                            bool pressureOk;
                            const double pressure = pressureContent.left(pressureContent.length() - 3).toDouble(&pressureOk);
                            hourlyInfo.pressure = pressureOk ? pressure : -1;
                            xmlFreeNode(pressureNode);
                            // 81.1%
                            const xmlNodePtr humidityNode = &(node->children[7]);
                            const QString humidityContent = ((QString)((char *)humidityNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found humidity: " << humidityContent;
                            bool humidityOk;
                            const double humidity = humidityContent.left(humidityContent.length() - 1).toDouble(&humidityOk);
                            hourlyInfo.humidity = humidityOk ? humidity : -1;
                            xmlFreeNode(humidityNode);
                            // 10.1%
                            const xmlNodePtr possibilityNode = &(node->children[8]);
                            const QString possibilityContent = ((QString)((char *)possibilityNode->content)).trimmed();
                            qDebug(IONENGINE_WWWNMCCN) << "Found possibility:: " << possibilityContent;
                            bool possibilityOk;
                            const double possibility = possibilityContent.left(possibilityContent.length() - 1).toDouble(&possibilityOk);
                            hourlyInfo.possibility = possibilityOk ? possibility : -1;
                            xmlFreeNode(possibilityNode);
                            xmlFreeNode(node);

                            hourlyInfos.append(hourlyInfo);
                        }
                        xmlXPathFreeObject(day0DivElementXPath);
                        readWriteLock.lockForWrite();
                        stationIdMap[reply->url()] = stationId;
                        hourlyWeathers.insert(stationId, &hourlyInfos);
                        readWriteLock.unlock();
                    }
                    xmlXPathFreeContext(context);
                }
                xmlFreeDoc(doc);
                xmlCleanupParser();
            }
            else {
                qFatal(IONENGINE_WWWNMCCN) << "Unable to handle response type " << contentType << " from url " << urlWithoutQuery;
            }
        }
        else {
            qFatal(IONENGINE_WWWNMCCN) << "Invalid Content-Type header: " << contentType;
        }
    }
    else if (reply->isFinished()) {
        qFatal(IONENGINE_WWWNMCCN) << "Request error: " << reply->error();
    }
    else {
        qFatal(IONENGINE_WWWNMCCN) << "Request is not finished.";
    }
    reply->deleteLater();
}

StationSearchApiResponse WwwNmcCnIon::searchPlacesApi(const QString &searchString, const int searchLimit)
{
    const QString encodedSearchString = searchString.toUtf8().toPercentEncoding();
    QNetworkRequest request;
    QUrl url = (QString)SEARCH_API;
    QUrlQuery query;
    query.addQueryItem("q", encodedSearchString);
    query.addQueryItem("limit", ((QString)"%1").arg(searchLimit));
    query.addQueryItem("timestamp", ((QString)"%1").arg(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem("_", ((QString)"%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, FORECAST_PAGE);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    request.setHeaders(headers);
    QNetworkReply* _ = networkAccessManager->get(request);
    readWriteLock.lockForRead();
    StationSearchApiResponse response = *searchedStations[searchString];
    readWriteLock.unlock();
    return response;
}

WeatherApiResponse WwwNmcCnIon::searchWeatherApi(const QString &stationId, const QUrl &referer)
{
    QNetworkRequest request;
    QUrl url = (QString)WEATHER_API;
    QUrlQuery query;
    query.addQueryItem("stationid", stationId);
    query.addQueryItem("_", ((QString)"%1").arg(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    request.setUrl(url);
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, referer.toString());
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    request.setHeaders(headers);
    QNetworkReply* _ = networkAccessManager->get(request);
    readWriteLock.lockForRead();
    WeatherApiResponse response = *weathers[stationId];
    readWriteLock.unlock();
    return response;
}

HourlyInfoList WwwNmcCnIon::extractWebPage(const QUrl &webPage)
{
    QNetworkRequest request;
    request.setUrl(webPage);
    QHttpHeaders headers;
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Referer, FORECAST_PAGE);
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::UserAgent, USER_AGENT);
    request.setHeaders(headers);
    QNetworkReply* _ = networkAccessManager->get(request);
    readWriteLock.lockForRead();
    HourlyInfoList response = *hourlyWeathers[stationIdMap[webPage]];
    readWriteLock.unlock();
    return response;
}

#ifdef ION_LEGACY

K_PLUGIN_CLASS_WITH_JSON(WwwNmcCnIon, "metadata.legacy.json");

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
        if (requestName == "validate") {
            StationSearchApiResponse response = searchPlacesApi(splitSource[2]);
            QStringList dataValue = {ION_NAME};
            if (response.code == 0) {
                const int stationCount = response.data.count();
                dataValue += {
                    stationCount > 0 ? "valid" : "invalid",
                    stationCount > 1 ? "multiple" : "single",
                };
                bool hasResult = false;
                if (stationCount > 0) {
                    for (const QString &data : response.data) {
                        // Search result format:
                        // id|city|province|refererPath|lon?|lat?
                        const QStringList splitData = data.split(placeInfoSep);
                        if (splitData.count() >= 6) {
                            hasResult = true;
                            const QStringList extraData = {splitData[0], splitData[3], splitData[4], splitData[5]};
                            dataValue += {"place", splitData[1] + "-" + splitData[2], "extra", extraData.join(extraDataSep)};
                        }
                    }
                }
                if (!hasResult) {
                    dataValue += {splitSource[2]};
                }
            }
            else {
                qFatal(IONENGINE_WWWNMCCN) << "API response invalid: " << response.msg;
                dataValue += {"invalid", "single", splitSource[2]};
            }
            const QString value = dataValue.join(sourceSep);
            qDebug(IONENGINE_WWWNMCCN) << "Responsing validate request with: " << value;
            setData(source, "validate", value);
            return true;
        }
        else if (requestName == "weather" && splitSource.count() >= 4 && ! splitSource[3].isEmpty()) {
            const QStringList splitExtraData = splitSource[3].split(extraDataSep);
            if (splitExtraData.count() >= 4) {
                const QString creditPage = API_BASE + splitExtraData[1];
                HourlyInfoList hourly = extractWebPage(creditPage);
                WeatherApiResponse apiResponse = searchWeatherApi(splitExtraData[0], creditPage);
                qDebug(IONENGINE_WWWNMCCN) << "Responsing weather request...";

                Plasma5Support::DataEngine::Data data;
                if (apiResponse.code == 0 && hourly.count() > 0) {
                    const QString stationId = apiResponse.data.real.station.code;
                    data.insert("Credit", "www.nmc.cn");
                    data.insert("Credit Url", creditPage);
                    data.insert("Country", i18n("China"));
                    data.insert("Place", apiResponse.data.real.station.city);
                    data.insert("Region", apiResponse.data.real.station.province);
                    data.insert("Station", apiResponse.data.real.station.city);
                    data.insert("Observation Period", apiResponse.data.real.publish_time);
                    const bool night = apiResponse.data.real.sunriseSunset.sunset <= apiResponse.data.real.publish_time &&
                                       apiResponse.data.real.publish_time <= apiResponse.data.real.sunriseSunset.sunrise;
                    data.insert("Current Conditions", getWeatherConditionIcon(apiResponse.data.real.weather.img,
                                                                              apiResponse.data.real.wind.speed > 0,
                                                                              night));
                    data.insert("Temperature", apiResponse.data.real.weather.temperature);
                    data.insert("Temperature Unit", KUnitConversion::Celsius);
                    data.insert("Windchill", std::round(apiResponse.data.real.weather.feelst));
                    data.insert("Humidex", std::round(apiResponse.data.real.weather.feelst));
                    data.insert("Wind Direction", apiResponse.data.real.wind.direct);
                    data.insert("Wind Speed", std::round(apiResponse.data.real.wind.speed));
                    data.insert("Wind Speed Unit", KUnitConversion::MeterPerSecond);
                    data.insert("Humidity", apiResponse.data.real.weather.humidity);
                    data.insert("Pressure", apiResponse.data.real.weather.airpressure);
                    data.insert("Pressure Unit", KUnitConversion::Hectopascal);
                    data.insert("Sunrise At", apiResponse.data.real.sunriseSunset.sunrise);
                    data.insert("Sunset At", apiResponse.data.real.sunriseSunset.sunset);
                    const QString forecastRelatedKeyTemplate = "Short Forecast Day %1";
                    const QStringList forecastRelatedValueTemplates = {"%1", "%2", "%3", "%4", "%5", "%6"};
                    const QString forecastRelatedValueTemplate = forecastRelatedValueTemplates.join(sourceSep);
                    for (int i = 0; i < apiResponse.data.predict.detail.count(); i++) {
                        const QString forecastRelatedKey = forecastRelatedKeyTemplate.arg(i);
                        const DetailInfo detail = apiResponse.data.predict.detail.at(i);
                        if (i > 0) {
                            const QString forecastRelatedValue = forecastRelatedValueTemplate.arg(detail.date.day())
                                                               .arg(getWeatherIcon(getWeatherConditionIcon(detail.day.weather.img, detail.day.wind.power != "无持续风向", false)))
                                                               .arg(detail.day.weather.info)
                                                               .arg(std::max(detail.day.weather.temperature, detail.night.weather.temperature))
                                                               .arg(std::min(detail.day.weather.temperature, detail.night.weather.temperature))
                                                               .arg("N/U");
                            data.insert(forecastRelatedKey, forecastRelatedValue);
                        }
                        else {
                            const QString forecastRelatedValueDay = forecastRelatedValueTemplate.arg(i18n("Day"))
                                                                  .arg(getWeatherIcon(getWeatherConditionIcon(detail.day.weather.img, detail.day.wind.direct != "无持续风向", false)))
                                                                  .arg(detail.day.weather.info)
                                                                  .arg(detail.day.weather.temperature)
                                                                  .arg("N/U")
                                                                  .arg("N/U");
                            data.insert(forecastRelatedKey, forecastRelatedValueDay);
                            const QString forecastRelatedValueNight = forecastRelatedValueTemplate.arg(i18n("Night"))
                                                                    .arg(getWeatherIcon(getWeatherConditionIcon(detail.night.weather.img, detail.night.wind.direct != "无持续风向", true)))
                                                                    .arg(detail.night.weather.info)
                                                                    .arg(detail.night.weather.temperature)
                                                                    .arg("N/U")
                                                                    .arg("N/U");
                            data.insert(forecastRelatedKey, forecastRelatedValueNight);
                        }
                    }
                    data.insert("Total Weather Days", apiResponse.data.predict.detail.count());
                    const QDateTime now = QDateTime::currentDateTime();
                    WarnInfoList *warns = activeWarnings[stationId];
                    for (int i=0; i<warns->count(); i++) {
                        const WarnInfo warn = warns->at(i);
                        if (now > warn.expireTime) {
                            qDebug(IONENGINE_WWWNMCCN) << "Removing outdated warning: " << warn.signaltype << "-" << warn.signallevel;
                            warns->remove(i);
                        }
                    }
                    warns->squeeze();
                    const QString weatherWarningRelatedDescriptionKeyTemplate = "Warning Description %1";
                    const QString weatherWarningRelatedInfoKeyTemplate = "Warning Info %1";
                    const bool warnValid = !apiResponse.data.real.warn.alert.isEmpty() &&
                                           apiResponse.data.real.warn.alert != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.city.isEmpty() &&
                                           apiResponse.data.real.warn.city != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.fmeans.isEmpty() &&
                                           apiResponse.data.real.warn.fmeans != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.issuecontent.isEmpty() &&
                                           apiResponse.data.real.warn.issuecontent != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.pic.isEmpty() &&
                                           apiResponse.data.real.warn.pic != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.pic2.isEmpty() &&
                                           apiResponse.data.real.warn.pic2 != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.province.isEmpty() &&
                                           apiResponse.data.real.warn.province != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.signallevel.isEmpty() &&
                                           apiResponse.data.real.warn.signallevel != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.signaltype.isEmpty() &&
                                           apiResponse.data.real.warn.signaltype != INVALID_VALUE_STR &&
                                           !apiResponse.data.real.warn.url.isEmpty() &&
                                           apiResponse.data.real.warn.url != INVALID_VALUE_STR &&
                                           apiResponse.data.real.warn.expireTime.isValid() &&
                                           apiResponse.data.real.warn.expireTime > now;
                    if (warnValid) {
                        warns->append(apiResponse.data.real.warn);
                    }
                    qDebug(IONENGINE_WWWNMCCN) << "Adding " << activeWarnings.count() << " warnings...";
                    for (int i = 0;i < warns->count(); i++) {
                        const WarnInfo warn = warns->at(i);
                        const QString weatherWarningRelatedDescriptionKey = weatherWarningRelatedDescriptionKeyTemplate.arg(i);
                        const QString weatherWarningRelatedInfoKey = weatherWarningRelatedInfoKeyTemplate.arg(i);
                        const QString weatherWarningRelatedInfoValue = API_BASE + warn.url;
                        const QString weatherWarningRelatedDescriptionValue = warn.signaltype + "-" + warn.signallevel;
                        data.insert(weatherWarningRelatedDescriptionKey, weatherWarningRelatedDescriptionValue);
                        data.insert(weatherWarningRelatedInfoKey, weatherWarningRelatedInfoValue);
                    }
                    data.insert("Total Warnings Issued", activeWarnings.count());
                }
                else if (apiResponse.code == 0) {
                    qFatal(IONENGINE_WWWNMCCN) << "Failed to get hourly report from webpage.";
                    data.insert("Place", i18n("N/A"));
                }
                else {
                    qFatal(IONENGINE_WWWNMCCN) << "API response invalid: " << apiResponse.msg;
                    data.insert("Place", i18n("N/A"));
                }

                Q_EMIT cleanUpData(source);
                QStringList weatherSourceValues = {ION_NAME, "weather", splitSource[2]};
                const QString weatherSource = weatherSourceValues.join(sourceSep);
                qDebug(IONENGINE_WWWNMCCN) << "Responsing weather source: " << weatherSource;
                qDebug(IONENGINE_WWWNMCCN) << "Response data: " << data;
                setData(weatherSource, data);
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
    networkAccessManager->clearConnectionCache();
    readWriteLock.lockForWrite();
    activeWarnings.clear();
    weathers.clear();
    hourlyWeathers.clear();
    searchedStations.clear();
    stationIdMap.clear();
    readWriteLock.unlock();
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
