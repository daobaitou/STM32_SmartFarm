#include "database.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

Database::Database(QObject *parent)
    : QObject(parent)
{
}

bool Database::init(const QString &path)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                         .absoluteFilePath(path);
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) return false;

    QSqlQuery q;
    q.exec("CREATE TABLE IF NOT EXISTS sensor_data ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "timestamp INTEGER NOT NULL,"
           "temperature REAL, humidity REAL,"
           "soil_moisture INTEGER, soil_temp REAL,"
           "light REAL, pressure REAL,"
           "co2 INTEGER, flow_rate REAL, total_volume REAL)");
    return true;
}

void Database::insertSensorData(const SensorData &data)
{
    QSqlQuery q;
    q.prepare("INSERT INTO sensor_data (timestamp, temperature, humidity,"
              " soil_moisture, soil_temp, light, pressure, co2, flow_rate, total_volume)"
              " VALUES (?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(data.timestamp);
    q.addBindValue(data.temperature);
    q.addBindValue(data.humidity);
    q.addBindValue(data.soil_moisture);
    q.addBindValue(data.soil_temp);
    q.addBindValue(data.light);
    q.addBindValue(data.pressure);
    q.addBindValue(data.co2);
    q.addBindValue(data.flow_rate);
    q.addBindValue(data.total_volume);
    q.exec();
}

QList<SensorData> Database::queryHistory(int hours)
{
    QList<SensorData> result;
    qint64 since = QDateTime::currentSecsSinceEpoch() - hours * 3600;

    QSqlQuery q;
    q.prepare("SELECT * FROM sensor_data WHERE timestamp > ? ORDER BY timestamp ASC");
    q.addBindValue(since);
    q.exec();

    while (q.next()) {
        SensorData d;
        d.timestamp = q.value(1).toLongLong();
        d.temperature = q.value(2).toFloat();
        d.humidity = q.value(3).toFloat();
        d.soil_moisture = q.value(4).toInt();
        d.soil_temp = q.value(5).toFloat();
        d.light = q.value(6).toFloat();
        d.pressure = q.value(7).toFloat();
        d.co2 = q.value(8).toInt();
        d.flow_rate = q.value(9).toFloat();
        d.total_volume = q.value(10).toFloat();
        result.append(d);
    }
    return result;
}

int Database::totalCount()
{
    QSqlQuery q("SELECT COUNT(*) FROM sensor_data");
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool Database::exportCsv(const QString &filePath, int hours)
{
    QList<SensorData> data = queryHistory(hours);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "时间,温度,湿度,土壤湿度,土壤温度,光照,CO2,气压,流量,累计水量\n";

    for (const auto &d : data) {
        out << d.dateTime().toString("yyyy-MM-dd HH:mm:ss") << ","
            << d.temperature << "," << d.humidity << ","
            << d.soil_moisture << "," << d.soil_temp << ","
            << d.light << "," << d.co2 << ","
            << d.pressure << "," << d.flow_rate << ","
            << d.total_volume << "\n";
    }
    file.close();
    return true;
}