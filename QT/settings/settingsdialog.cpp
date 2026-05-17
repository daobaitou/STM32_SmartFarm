#include "settingsdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("设置");
    setMinimumWidth(350);

    auto *layout = new QFormLayout(this);

    m_hostEdit = new QLineEdit("http://124.223.5.91");
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(80);
    m_deviceEdit = new QLineEdit("farm_001");

    layout->addRow("服务器地址:", m_hostEdit);
    layout->addRow("端口:", m_portSpin);
    layout->addRow("设备ID:", m_deviceEdit);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, [this]() { saveSettings(); accept(); });
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(btns);

    loadSettings();
}

QString SettingsDialog::brokerHost() const { return m_hostEdit->text(); }
int SettingsDialog::brokerPort() const { return m_portSpin->value(); }
QString SettingsDialog::deviceId() const { return m_deviceEdit->text(); }

void SettingsDialog::loadSettings()
{
    QSettings s("SmartFarm", "SmartFarm");
    m_hostEdit->setText(s.value("api/host", "http://124.223.5.91").toString());
    m_portSpin->setValue(s.value("api/port", 80).toInt());
    m_deviceEdit->setText(s.value("api/device", "farm_001").toString());
}

void SettingsDialog::saveSettings()
{
    QSettings s("SmartFarm", "SmartFarm");
    s.setValue("api/host", m_hostEdit->text());
    s.setValue("api/port", m_portSpin->value());
    s.setValue("api/device", m_deviceEdit->text());
}