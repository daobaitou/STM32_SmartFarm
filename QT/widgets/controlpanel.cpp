#include "controlpanel.h"
#include "apiclient.h"
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>

ControlPanel::ControlPanel(ApiClient *api, QWidget *parent)
    : QWidget(parent)
    , m_api(api)
{
    auto *mainLayout = new QHBoxLayout(this);

    auto *modeGroup = new QGroupBox("模式");
    auto *modeLayout = new QHBoxLayout(modeGroup);
    m_btnAuto = new QPushButton("自动");
    m_btnManual = new QPushButton("手动");
    m_btnAuto->setCheckable(true);
    m_btnManual->setCheckable(true);
    m_btnAuto->setChecked(true);
    connect(m_btnAuto, &QPushButton::clicked, this, &ControlPanel::onModeAuto);
    connect(m_btnManual, &QPushButton::clicked, this, &ControlPanel::onModeManual);
    modeLayout->addWidget(m_btnAuto);
    modeLayout->addWidget(m_btnManual);
    mainLayout->addWidget(modeGroup);

    auto *devGroup = new QGroupBox("设备控制");
    auto *devLayout = new QHBoxLayout(devGroup);
    m_btnPump = new QPushButton("水泵 OFF");
    m_btnFan = new QPushButton("风扇 OFF");
    m_btnWindow = new QPushButton("窗户 CLOSE");
    m_btnPump->setCheckable(true);
    m_btnFan->setCheckable(true);
    m_btnWindow->setCheckable(true);
    connect(m_btnPump, &QPushButton::clicked, this, &ControlPanel::onPumpToggle);
    connect(m_btnFan, &QPushButton::clicked, this, &ControlPanel::onFanToggle);
    connect(m_btnWindow, &QPushButton::clicked, this, &ControlPanel::onWindowToggle);
    devLayout->addWidget(m_btnPump);
    devLayout->addWidget(m_btnFan);
    devLayout->addWidget(m_btnWindow);
    mainLayout->addWidget(devGroup);

    auto *thGroup = new QGroupBox("灌溉阈值");
    auto *thLayout = new QHBoxLayout(thGroup);
    thLayout->addWidget(new QLabel("低:"));
    m_spinLow = new QSpinBox();
    m_spinLow->setRange(0, 100);
    m_spinLow->setValue(30);
    thLayout->addWidget(m_spinLow);
    thLayout->addWidget(new QLabel("高:"));
    m_spinHigh = new QSpinBox();
    m_spinHigh->setRange(0, 100);
    m_spinHigh->setValue(70);
    thLayout->addWidget(m_spinHigh);
    auto *btnSet = new QPushButton("设置");
    connect(btnSet, &QPushButton::clicked, this, &ControlPanel::onThresholdSet);
    thLayout->addWidget(btnSet);
    mainLayout->addWidget(thGroup);
}

void ControlPanel::onModeAuto()
{
    m_api->sendControl("mode", "AUTO");
    m_btnAuto->setChecked(true);
    m_btnManual->setChecked(false);
}

void ControlPanel::onModeManual()
{
    m_api->sendControl("mode", "MANUAL");
    m_btnManual->setChecked(true);
    m_btnAuto->setChecked(false);
}

void ControlPanel::onPumpToggle()
{
    bool on = m_btnPump->isChecked();
    m_api->sendControl("pump", on ? "ON" : "OFF");
    m_btnPump->setText(on ? "水泵 ON" : "水泵 OFF");
}

void ControlPanel::onFanToggle()
{
    bool on = m_btnFan->isChecked();
    m_api->sendControl("fan", on ? "ON" : "OFF");
    m_btnFan->setText(on ? "风扇 ON" : "风扇 OFF");
}

void ControlPanel::onWindowToggle()
{
    bool open = m_btnWindow->isChecked();
    m_api->sendControl("window", open ? "OPEN" : "CLOSE");
    m_btnWindow->setText(open ? "窗户 OPEN" : "窗户 CLOSE");
}

void ControlPanel::onThresholdSet()
{
    m_api->sendThreshold(m_spinLow->value(), m_spinHigh->value());
}