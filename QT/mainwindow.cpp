#include "mainwindow.h"
#include "widgets/sensorcard.h"
#include "widgets/controlpanel.h"
#include "charts/historychart.h"
#include "settings/settingsdialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QSplitter>
#include <QGridLayout>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_api(new ApiClient(this))
    , m_db(new Database(this))
{
    setWindowTitle("SmartFarm - 智能农业监测系统");
    resize(1100, 700);

    m_db->init();

    createMenus();
    createSensorCards();

    m_chart = new HistoryChart(m_db, m_api);
    m_control = new ControlPanel(m_api);

    // 左侧：传感器卡片
    auto *cardScroll = new QScrollArea();
    auto *cardContainer = new QWidget();
    auto *cardLayout = new QGridLayout(cardContainer);
    cardLayout->setSpacing(8);
    cardLayout->setContentsMargins(8, 8, 8, 8);

    cardLayout->addWidget(m_cardTemp, 0, 0);
    cardLayout->addWidget(m_cardHumidity, 0, 1);
    cardLayout->addWidget(m_cardSoil, 1, 0);
    cardLayout->addWidget(m_cardSoilTemp, 1, 1);
    cardLayout->addWidget(m_cardLight, 2, 0);
    cardLayout->addWidget(m_cardCO2, 2, 1);
    cardLayout->addWidget(m_cardPressure, 3, 0);
    cardLayout->addWidget(m_cardFlow, 3, 1);
    cardLayout->addWidget(m_cardVolume, 4, 0, 1, 2);
    cardLayout->setRowStretch(5, 1);

    cardScroll->setWidget(cardContainer);
    cardScroll->setWidgetResizable(true);
    cardScroll->setMinimumWidth(300);
    cardScroll->setMaximumWidth(340);

    // 右侧：图表
    auto *rightWidget = new QWidget();
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_chart, 1);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(cardScroll);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    auto *centralWidget = new QWidget();
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(m_control);
    setCentralWidget(centralWidget);

    // 状态栏
    m_statusConn = new QLabel("未连接");
    m_statusTime = new QLabel("更新时间: --");
    m_statusRecords = new QLabel("数据库: 0条");
    statusBar()->addWidget(m_statusConn);
    statusBar()->addWidget(m_statusTime);
    statusBar()->addWidget(m_statusRecords);

    // 信号连接
    connect(m_api, &ApiClient::sensorDataReceived,
            this, &MainWindow::onSensorData);
    connect(m_api, &ApiClient::connectionChanged,
            this, &MainWindow::onConnectionChanged);

    // 加载QSS样式
    QFile qssFile(":/styles.qss");
    if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text))
        qApp->setStyleSheet(qssFile.readAll());

    // 自动连接
    QSettings s("SmartFarm", "SmartFarm");
    QString host = s.value("api/host").toString();
    if (!host.isEmpty()) {
        m_api->setBaseUrl(host);
        m_api->startPolling();
    }
}

MainWindow::~MainWindow() {}

void MainWindow::createMenus()
{
    auto *menuBar = this->menuBar();
    auto *connMenu = menuBar->addMenu("连接");
    auto *connAct = connMenu->addAction("连接服务器");
    connect(connAct, &QAction::triggered, this, &MainWindow::onConnect);
    auto *discAct = connMenu->addAction("断开");
    connect(discAct, &QAction::triggered, m_api, &ApiClient::stopPolling);

    auto *toolMenu = menuBar->addMenu("工具");
    auto *exportAct = toolMenu->addAction("导出CSV");
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportCsv);
    auto *settingsAct = toolMenu->addAction("设置");
    connect(settingsAct, &QAction::triggered, this, &MainWindow::onSettings);
}

void MainWindow::createSensorCards()
{
    m_cardTemp     = new SensorCard("空气温度",   "°C",    "#e74c3c");
    m_cardHumidity = new SensorCard("空气湿度",   "%",     "#3498db");
    m_cardSoil     = new SensorCard("土壤湿度",   "%",     "#8d6e63");
    m_cardSoilTemp = new SensorCard("土壤温度",   "°C",    "#e67e22");
    m_cardLight    = new SensorCard("光照强度",   "lux",   "#f39c12");
    m_cardCO2      = new SensorCard("CO2浓度",    "ppm",   "#27ae60");
    m_cardPressure = new SensorCard("大气压强",   "hPa",   "#9b59b6");
    m_cardFlow     = new SensorCard("水流速率",   "L/min", "#00bcd4");
    m_cardVolume   = new SensorCard("累计水量",   "L",     "#1565c0");
}

void MainWindow::onConnect()
{
    QSettings s("SmartFarm", "SmartFarm");
    QString host = s.value("api/host", "http://124.223.5.91").toString();
    m_api->setBaseUrl(host);
    m_api->startPolling();
    m_statusConn->setText("正在连接...");
}

void MainWindow::onSensorData(const SensorData &data)
{
    m_cardTemp->setValue(data.temperature);
    m_cardHumidity->setValue(data.humidity);
    m_cardSoil->setValue(data.soil_moisture);
    m_cardSoilTemp->setValue(data.soil_temp);
    m_cardLight->setValue(data.light);
    m_cardCO2->setValue(data.co2);
    m_cardPressure->setValue(data.pressure);
    m_cardFlow->setValue(data.flow_rate);
    m_cardVolume->setValue(data.total_volume);

    m_db->insertSensorData(data);

    m_statusTime->setText("更新: " + QDateTime::currentDateTime().toString("HH:mm:ss"));
    m_statusRecords->setText(QString("数据库: %1条").arg(m_db->totalCount()));
}

void MainWindow::onConnectionChanged(bool connected)
{
    m_statusConn->setText(connected ? "已连接" : "未连接");
    if (connected)
        m_chart->refresh();
}

void MainWindow::onExportCsv()
{
    QString path = QFileDialog::getSaveFileName(this, "导出CSV", "smartfarm_data.csv",
                                                 "CSV文件 (*.csv)");
    if (!path.isEmpty()) {
        if (m_db->exportCsv(path, 24))
            QMessageBox::information(this, "导出成功", "数据已导出到:\n" + path);
        else
            QMessageBox::warning(this, "导出失败", "无法写入文件");
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        if (m_api->isPolling()) {
            m_api->stopPolling();
            QTimer::singleShot(500, this, &MainWindow::onConnect);
        }
    }
}