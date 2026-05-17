#ifndef HISTORYCHART_H
#define HISTORYCHART_H

#include <QWidget>
#include <QChartView>
#include <QLineSeries>
#include <QComboBox>
#include "sensormodel.h"

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

class Database;
class ApiClient;

class HistoryChart : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryChart(Database *db, ApiClient *api, QWidget *parent = nullptr);

public slots:
    void refresh();
    void onHistoryReceived(const QList<SensorData> &data);

private:
    void setupChart();

    QChartView *m_chartView;
    QComboBox *m_fieldCombo;
    QComboBox *m_timeCombo;
    QChart *m_chart;
    QLineSeries *m_series;
    Database *m_db;
    ApiClient *m_api;

    struct FieldDef {
        QString key;
        QString label;
        QString unit;
        QColor color;
    };
    QVector<FieldDef> m_fields;
};

#endif // HISTORYCHART_H