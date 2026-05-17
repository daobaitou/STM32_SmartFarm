#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QString brokerHost() const;
    int brokerPort() const;
    QString deviceId() const;

    void loadSettings();
    void saveSettings();

private:
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QLineEdit *m_deviceEdit;
};

#endif // SETTINGSDIALOG_H