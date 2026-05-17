#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include "sensormodel.h"

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    bool init(const QString &path = "smartfarm.db");

    void insertSensorData(const SensorData &data);
    QList<SensorData> queryHistory(int hours);
    int totalCount();
    bool exportCsv(const QString &filePath, int hours);

private:
    QSqlDatabase m_db;
};

#endif // DATABASE_H