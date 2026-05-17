#ifndef SENSORMODEL_H
#define SENSORMODEL_H

#include <QObject>
#include <QDateTime>

struct SensorData {
    float temperature = 0;
    float humidity = 0;
    int soil_moisture = 0;
    float soil_temp = 0;
    float light = 0;
    float pressure = 0;
    int co2 = 0;
    float flow_rate = 0;
    float total_volume = 0;
    qint64 timestamp = 0;

    QDateTime dateTime() const {
        return QDateTime::fromSecsSinceEpoch(timestamp);
    }
};

#endif // SENSORMODEL_H