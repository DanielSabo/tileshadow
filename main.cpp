#include "mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>

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

    if ((!appSettings.contains("EnableGLSharing")) ||
        (a.queryKeyboardModifiers() & Qt::ShiftModifier))
    {
        QMessageBox query;
        query.setText("Enable OpenGL buffer sharing?");
        query.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        query.setDefaultButton(QMessageBox::Yes);

        int result = query.exec();

        if (result == QMessageBox::Yes)
            appSettings.setValue("EnableGLSharing", QVariant::fromValue<bool>(true));
        else
            appSettings.setValue("EnableGLSharing", QVariant::fromValue<bool>(false));
    }

    MainWindow w;
    w.show();

    return a.exec();
}
