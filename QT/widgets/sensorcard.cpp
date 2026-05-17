#include "sensorcard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

SensorCard::SensorCard(const QString &name, const QString &unit,
                       const QString &color, QWidget *parent)
    : QWidget(parent)
    , m_unit(unit)
{
    setStyleSheet(QString(
        "SensorCard {"
        "  background: white;"
        "  border-left: 4px solid %1;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}").arg(color));

    setMinimumWidth(140);
    setMaximumHeight(80);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(2);

    m_nameLabel = new QLabel(name);
    m_nameLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; border: none;");

    m_valueLabel = new QLabel("-- " + unit);
    m_valueLabel->setStyleSheet("color: #2c3e50; font-size: 20px; font-weight: bold; border: none;");

    m_rangeLabel = new QLabel("");
    m_rangeLabel->setStyleSheet("color: #95a5a6; font-size: 10px; border: none;");

    layout->addWidget(m_nameLabel);
    layout->addWidget(m_valueLabel);
    layout->addWidget(m_rangeLabel);
}

void SensorCard::setValue(double value)
{
    m_valueLabel->setText(QString::number(value, 'f', 1) + " " + m_unit);
}

void SensorCard::setRange(double min, double max)
{
    m_rangeLabel->setText(QString("范围: %1 ~ %2").arg(min, 0, 'f', 1).arg(max, 0, 'f', 1));
}