#ifndef SENSORCARD_H
#define SENSORCARD_H

#include <QWidget>
#include <QLabel>

class SensorCard : public QWidget
{
    Q_OBJECT
public:
    explicit SensorCard(const QString &name, const QString &unit,
                        const QString &color, QWidget *parent = nullptr);

    void setValue(double value);
    void setRange(double min, double max);

private:
    QLabel *m_nameLabel;
    QLabel *m_valueLabel;
    QLabel *m_rangeLabel;
    QString m_unit;
};

#endif // SENSORCARD_H