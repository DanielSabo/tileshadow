#include "mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include "deviceselectdialog.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("TileShadow");
    a.setApplicationDisplayName("TileShadow");
    a.setOrganizationName("TileShadow");
    a.setOrganizationDomain("danielsabo.bitbucket.org");
#ifdef Q_OS_MAC
    QSettings::setDefaultFormat(QSettings::NativeFormat);
#else
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif

    QSettings appSettings;
    //FIXME: Remove this old setting
    appSettings.remove("EnableGLSharing");

    if ((!appSettings.contains("OpenCL/Device")) ||
        (a.queryKeyboardModifiers() & Qt::ShiftModifier))
    {
        DeviceSelectDialog().exec();
    }

    MainWindow w;
    w.show();

    return a.exec();
}
