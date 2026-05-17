#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "sensormodel.h"

class ApiClient : public QObject
{
    Q_OBJECT
public:
    explicit ApiClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &url);
    void startPolling(int intervalMs = 2000);
    void stopPolling();
    bool isPolling() const;

    void fetchLatest();
    void fetchHistory(int hours);
    void sendControl(const QString &device, const QString &value);
    void sendThreshold(int low, int high);

signals:
    void sensorDataReceived(const SensorData &data);
    void historyDataReceived(const QList<SensorData> &data);
    void connectionChanged(bool connected);
    void errorOccurred(const QString &error);

private slots:
    void onLatestReply(QNetworkReply *reply);
    void onHistoryReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    QString m_baseUrl;
    QTimer *m_pollTimer;
    bool m_connected;
};

#endif // APICLIENT_H