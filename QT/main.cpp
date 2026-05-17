#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("SmartFarm");
    a.setOrganizationName("SmartFarm");

    MainWindow w;
    w.show();

    return a.exec();
}