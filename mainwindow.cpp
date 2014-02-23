#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <math.h>
#include <QElapsedTimer>
#include "canvaswidget-opencl.h"


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

double drawBenchmarkCircle(CanvasWidget *canvas, float radius, float centerX, float centerY, int numPoints)
{
    QElapsedTimer timer;
    float currentAngle = 0;

    canvas->setUpdatesEnabled(false);
    timer.start();
    canvas->startStroke(QPointF(cosf(currentAngle) * radius + centerX, sinf(currentAngle) * radius + centerY), 1.0f);
    for (int i = 0; i < numPoints; ++i)
    {
        currentAngle += M_PI * 2.0f / 100.0f;
        canvas->strokeTo(QPointF(cosf(currentAngle) * radius + centerX, sinf(currentAngle) * radius + centerY), 1.0f);
    }
    canvas->endStroke();
    clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);
    double runTime = timer.elapsed();
    canvas->setUpdatesEnabled(true);

    return runTime;
}

void MainWindow::runCircleBenchmark()
{
    float radius = 100;
    float centerX = 150;
    float centerY = 150;

    const int runPoints = 200;

    double totalTime = 0.0;
    double totalPoints = 0.0;
    int numRuns = 0;
    int localRuns = 0;
    double lastAverage = 0.0;

    while (true)
    {
        double runTime = drawBenchmarkCircle(canvas, radius, centerX, centerY, runPoints);
        totalTime += runTime;
        totalPoints += runPoints;
        numRuns += 1;
        localRuns += 1;

        qDebug() << "Run" << runTime << "ms";

        double newAverage = totalTime / numRuns;
        if (localRuns > 4)
        {
            localRuns = 0;
            qDebug() << newAverage << lastAverage;
            if (fabs(newAverage - lastAverage) < 1.0)
                break;
        }

        lastAverage = newAverage;
    }

    qDebug() << "Benchmark" << lastAverage << "ms";
}
