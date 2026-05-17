#include "historychart.h"
#include "database.h"
#include "apiclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QGraphicsLayout>

HistoryChart::HistoryChart(Database *db, ApiClient *api, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_api(api)
    , m_chart(new QChart())
    , m_series(new QLineSeries())
{
    m_fields = {
        {"temperature",   "空气温度", QString::fromUtf8("°C"),   QColor("#e74c3c")},
        {"humidity",      "空气湿度", "%",     QColor("#3498db")},
        {"soil_moisture", "土壤湿度", "%",     QColor("#8d6e63")},
        {"soil_temp",     "土壤温度", QString::fromUtf8("°C"),    QColor("#e67e22")},
        {"light",         "光照强度", "lux",   QColor("#f39c12")},
        {"co2",           "CO2浓度",  "ppm",   QColor("#27ae60")},
        {"pressure",      "大气压强", "hPa",   QColor("#9b59b6")},
        {"flow_rate",     "水流速率", "L/min", QColor("#00bcd4")},
        {"total_volume",  "累计水量", "L",     QColor("#1565c0")},
    };

    auto *layout = new QVBoxLayout(this);

    auto *toolbar = new QHBoxLayout();
    m_fieldCombo = new QComboBox();
    for (const auto &f : m_fields)
        m_fieldCombo->addItem(f.label);
    connect(m_fieldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryChart::refresh);

    m_timeCombo = new QComboBox();
    m_timeCombo->addItems({"1小时", "6小时", "24小时", "3天"});
    m_timeCombo->setCurrentIndex(2);
    connect(m_timeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryChart::refresh);

    toolbar->addWidget(m_fieldCombo);
    toolbar->addWidget(m_timeCombo);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    setupChart();
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_chartView);

    connect(m_api, &ApiClient::historyDataReceived,
            this, &HistoryChart::onHistoryReceived);
}

void HistoryChart::setupChart()
{
    m_series->setColor(m_fields[0].color);
    m_chart->addSeries(m_series);
    m_chart->legend()->hide();
    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setBackgroundRoundness(4);

    auto *axisX = new QDateTimeAxis();
    axisX->setFormat("HH:mm");
    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_series->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    m_chart->addAxis(axisY, Qt::AlignLeft);
    m_series->attachAxis(axisY);
}

void HistoryChart::refresh()
{
    int hours[] = {1, 6, 24, 72};
    m_api->fetchHistory(hours[m_timeCombo->currentIndex()]);
}

void HistoryChart::onHistoryReceived(const QList<SensorData> &data)
{
    int idx = m_fieldCombo->currentIndex();
    const auto &field = m_fields[idx];

    m_series->clear();
    m_series->setColor(field.color);

    double minVal = 1e9, maxVal = -1e9;
    for (const auto &d : data) {
        qreal value = 0;
        if (field.key == "temperature")    value = d.temperature;
        else if (field.key == "humidity")   value = d.humidity;
        else if (field.key == "soil_moisture") value = d.soil_moisture;
        else if (field.key == "soil_temp")  value = d.soil_temp;
        else if (field.key == "light")      value = d.light;
        else if (field.key == "co2")        value = d.co2;
        else if (field.key == "pressure")   value = d.pressure;
        else if (field.key == "flow_rate")  value = d.flow_rate;
        else if (field.key == "total_volume") value = d.total_volume;

        m_series->append(d.dateTime().toMSecsSinceEpoch(), value);
        minVal = qMin(minVal, value);
        maxVal = qMax(maxVal, value);
    }

    auto axes = m_chart->axes(Qt::Vertical);
    if (!axes.isEmpty()) {
        auto *axisY = qobject_cast<QValueAxis*>(axes.first());
        axisY->setTitleText(field.unit);
        if (maxVal > minVal)
            axisY->setRange(minVal * 0.95, maxVal * 1.05);
    }
}