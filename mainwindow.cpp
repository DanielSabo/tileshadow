#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    canvas = findChild<CanvasWidget*>("mainCanvas");
    setWindowTitle(QApplication::applicationDisplayName());
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
