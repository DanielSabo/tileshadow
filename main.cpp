#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include "deviceselectdialog.h"
#include "batchprocessor.h"

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

    QCommandLineParser parser;
    QCommandLineOption batchFile("batch", "Command file to execute", "batchfile");
    parser.addOption(batchFile);

    parser.process(a);
    QString batchFilePath = parser.value(batchFile);

    if (!batchFilePath.isEmpty())
    {
        BatchProcessor *batch = new BatchProcessor();
        QMetaObject::invokeMethod(batch, "execute", Qt::QueuedConnection, Q_ARG(QString, batchFilePath));
    }
    else
    {
        if ((!appSettings.contains("OpenCL/Device")) ||
            (a.queryKeyboardModifiers() & Qt::ShiftModifier))
        {
            DeviceSelectDialog().exec();
        }

        MainWindow *w = new MainWindow();
        w->show();
    }

    return a.exec();
}
