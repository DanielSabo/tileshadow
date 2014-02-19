#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    canvas = findChild<CanvasWidget*>("mainCanvas");
    setWindowTitle(QApplication::applicationDisplayName());

    QWidget *toolbox = findChild<QWidget *>("toolbox");
    QList<QWidget *>toolButtons = toolbox->findChildren<QWidget *>();

    for (int i = 0; i < toolButtons.size(); ++i) {
        connect(toolButtons[i], SIGNAL(clicked()), this, SLOT(setActiveTool()));
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showOpenCLInfo()
{
    if (infoWindow.isNull())
        infoWindow.reset(new SystemInfoDialog ());

    infoWindow->setWindowTitle(QApplication::applicationDisplayName() + " - System Information");

    if (infoWindow->isVisible())
        infoWindow->raise();
    else
        infoWindow->show();
}

void MainWindow::actionQuit()
{
    QApplication::quit();
}

void MainWindow::setActiveTool()
{
    QVariant toolNameProp = sender()->property("toolName");
    if (toolNameProp.type() == QVariant::String)
    {
        canvas->setActiveTool(toolNameProp.toString());
    }
    else
        qWarning() << sender()->objectName() << " has no toolName property.";
}
