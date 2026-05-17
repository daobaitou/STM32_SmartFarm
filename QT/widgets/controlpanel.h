#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QPushButton>
#include <QSpinBox>

class ApiClient;

class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanel(ApiClient *api, QWidget *parent = nullptr);

private slots:
    void onModeAuto();
    void onModeManual();
    void onPumpToggle();
    void onFanToggle();
    void onWindowToggle();
    void onThresholdSet();

private:
    ApiClient *m_api;
    QPushButton *m_btnAuto;
    QPushButton *m_btnManual;
    QPushButton *m_btnPump;
    QPushButton *m_btnFan;
    QPushButton *m_btnWindow;
    QSpinBox *m_spinLow;
    QSpinBox *m_spinHigh;
};

#endif // CONTROLPANEL_H