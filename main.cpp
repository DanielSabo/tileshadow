#include "mainwindow.h"
#include <QApplication>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("TileShadow");
    a.setOrganizationName("TileShadow");
    a.setOrganizationDomain("danielsabo.bitbucket.org");
#ifdef Q_OS_MAC
    QSettings::setDefaultFormat(QSettings::NativeFormat);
#else
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif

    MainWindow w;
    w.show();

    return a.exec();
}
