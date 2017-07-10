#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include "toolfactory.h"
#include "nativeeventfilter.h"
#include "deviceselectdialog.h"
#include "batchprocessor.h"
#ifdef Q_OS_MAC
#include "machelpers.h"
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
    a.setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif
    a.setApplicationName("TileShadow");
    a.setApplicationDisplayName("TileShadow");
    a.setOrganizationName("TileShadow");
    a.setOrganizationDomain("danielsabo.bitbucket.org");

#ifdef Q_OS_MAC
    QSettings::setDefaultFormat(QSettings::NativeFormat);
    PlatformHelpers::macDisableAutoTabBar();
#else
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif

    QSettings appSettings;
    //FIXME: Remove this old setting
    appSettings.remove("EnableGLSharing");

    ToolFactory::initializeUserPaths();

    QCommandLineParser parser;
    QCommandLineOption batchFile("batch", "Command file to execute", "batchfile");
    parser.addOption(batchFile);

    parser.process(a);
    QString batchFilePath = parser.value(batchFile);

    std::unique_ptr<MainWindow> w;
    if (!batchFilePath.isEmpty())
    {
        BatchProcessor *batch = new BatchProcessor();
        QMetaObject::invokeMethod(batch, "execute", Qt::QueuedConnection, Q_ARG(QString, batchFilePath));
    }
    else
    {
        bool deviceValid = false;
        appSettings.value("OpenCL/Device").toString().toInt(&deviceValid);
        if ((!deviceValid) ||
            (a.queryKeyboardModifiers() & Qt::ShiftModifier))
        {
            DeviceSelectDialog().exec();
        }

        w.reset(new MainWindow());
        w->show();

        if (!parser.positionalArguments().empty())
            w->openFileRequest(parser.positionalArguments().at(0));
    }

#ifdef USE_NATIVE_EVENT_FILTER
    NativeEventFilter::install(&a);
#endif

    return a.exec();
}
