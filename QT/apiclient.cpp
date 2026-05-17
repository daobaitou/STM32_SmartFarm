#include "apiclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_baseUrl("http://124.223.5.91")
    , m_pollTimer(new QTimer(this))
    , m_connected(false)
{
    connect(m_pollTimer, &QTimer::timeout, this, &ApiClient::fetchLatest);
}

void ApiClient::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
}

void ApiClient::startPolling(int intervalMs)
{
    fetchLatest();
    m_pollTimer->start(intervalMs);
}

void ApiClient::stopPolling()
{
    m_pollTimer->stop();
}

bool ApiClient::isPolling() const
{
    return m_pollTimer->isActive();
}

void ApiClient::fetchLatest()
{
    QUrl url(m_baseUrl + "/api/latest");
    QNetworkRequest request(url);
    request.setTransferTimeout(3000);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onLatestReply(reply);
    });
}

void ApiClient::fetchHistory(int hours)
{
    QUrl url(m_baseUrl + "/api/history");
    QUrlQuery query;
    query.addQueryItem("hours", QString::number(hours));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHistoryReply(reply);
    });
}

void ApiClient::sendControl(const QString &device, const QString &value)
{
    QUrl url(m_baseUrl + "/api/control/" + device);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["value"] = value;
    QJsonDocument doc(obj);
    m_manager->post(request, doc.toJson());
}

void ApiClient::sendThreshold(int low, int high)
{
    QUrl url(m_baseUrl + "/api/control/threshold");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["low"] = low;
    obj["high"] = high;
    QJsonDocument doc(obj);
    m_manager->post(request, doc.toJson());
}

void ApiClient::onLatestReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        if (m_connected) {
            m_connected = false;
            emit connectionChanged(false);
        }
        reply->deleteLater();
        return;
    }

    if (!m_connected) {
        m_connected = true;
        emit connectionChanged(true);
    }

    QByteArray data = reply->readAll();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        reply->deleteLater();
        return;
    }

    QJsonObject obj = doc.object();
    SensorData sd;
    sd.temperature = obj["temperature"].toDouble();
    sd.humidity = obj["humidity"].toDouble();
    sd.soil_moisture = obj["soil_moisture"].toInt();
    sd.soil_temp = obj["soil_temp"].toDouble();
    sd.light = obj["light"].toDouble();
    sd.pressure = obj["pressure"].toDouble();
    sd.co2 = obj["co2"].toInt();
    sd.flow_rate = obj["flow_rate"].toDouble();
    sd.total_volume = obj["total_volume"].toDouble();
    sd.timestamp = obj["timestamp"].toInteger();

    emit sensorDataReceived(sd);
    reply->deleteLater();
}

void ApiClient::onHistoryReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        reply->deleteLater();
        return;
    }

    QList<SensorData> result;
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        SensorData sd;
        sd.temperature = obj["temperature"].toDouble();
        sd.humidity = obj["humidity"].toDouble();
        sd.soil_moisture = obj["soil_moisture"].toInt();
        sd.soil_temp = obj["soil_temp"].toDouble();
        sd.light = obj["light"].toDouble();
        sd.pressure = obj["pressure"].toDouble();
        sd.co2 = obj["co2"].toInt();
        sd.flow_rate = obj["flow_rate"].toDouble();
        sd.total_volume = obj["total_volume"].toDouble();
        sd.timestamp = obj["timestamp"].toInteger();
        result.append(sd);
    }

    emit historyDataReceived(result);
    reply->deleteLater();
}