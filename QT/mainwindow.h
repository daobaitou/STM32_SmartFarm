#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include "apiclient.h"
#include "database.h"
#include "sensormodel.h"

class SensorCard;
class HistoryChart;
class ControlPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnect();
    void onSensorData(const SensorData &data);
    void onConnectionChanged(bool connected);
    void onExportCsv();
    void onSettings();

private:
    void createMenus();
    void createSensorCards();

    ApiClient *m_api;
    Database *m_db;

    SensorCard *m_cardTemp;
    SensorCard *m_cardHumidity;
    SensorCard *m_cardSoil;
    SensorCard *m_cardSoilTemp;
    SensorCard *m_cardLight;
    SensorCard *m_cardCO2;
    SensorCard *m_cardPressure;
    SensorCard *m_cardFlow;
    SensorCard *m_cardVolume;

    HistoryChart *m_chart;
    ControlPanel *m_control;
    QLabel *m_statusConn;
    QLabel *m_statusTime;
    QLabel *m_statusRecords;
};

#endif // MAINWINDOW_H