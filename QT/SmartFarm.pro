QT       += core gui widgets charts sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SmartFarm
TEMPLATE = app
CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    apiclient.cpp \
    sensormodel.cpp \
    database.cpp \
    widgets/sensorcard.cpp \
    widgets/controlpanel.cpp \
    charts/historychart.cpp \
    settings/settingsdialog.cpp

HEADERS += \
    mainwindow.h \
    apiclient.h \
    sensormodel.h \
    database.h \
    widgets/sensorcard.h \
    widgets/controlpanel.h \
    charts/historychart.h \
    settings/settingsdialog.h

RESOURCES += resources/resources.qrc