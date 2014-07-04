#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <math.h>
#include <QElapsedTimer>
#include "canvaswidget-opencl.h"
#include <QDir>
#include <QPushButton>
#include <QCloseEvent>
#include <QStatusBar>
#include <QFileDialog>
#include "hsvcolordial.h"
#include "toollistwidget.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    freezeLayerList(false)
{
    ui->setupUi(this);

    canvas = ui->mainCanvas;
    statusBar = ui->statusBar;
    statusBar->hide();
    layersList = ui->layerList;

    {
        QBoxLayout *sidebarLayout = qobject_cast<QBoxLayout *>(ui->sidebar->layout());
        Q_ASSERT(sidebarLayout);
        ToolListWidget *toolList = new ToolListWidget();
        toolList->setCanvas(canvas);
        toolList->reloadTools();
        sidebarLayout->insertWidget(sidebarLayout->count() - 1, toolList);
    }

    //FIXME: Connect in UI file
    connect(layersList, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(layerListNameEdited(QListWidgetItem *)));
    updateTitle();
    updateStatus();
    updateLayers();

    connect(canvas, SIGNAL(updateStats()), this, SLOT(canvasStats()));
    connect(canvas, SIGNAL(updateLayers()), this, SLOT(updateLayers()));
    connect(canvas, SIGNAL(updateTool()), this, SLOT(updateTool()));

    //FIXME: This belongs in the UI file
    connect(ui->toolColorDial, SIGNAL(updateColor(QColor const &)), this, SLOT(colorDialChanged(QColor const &)));
    this->colorDialChanged(ui->toolColorDial->getColor());

    // Resize here because the big widget is unwieldy in the designer
    resize(700,400);

    canvas->setActiveTool("default.myb");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTitle()
{
    QString title = QApplication::applicationDisplayName();
    float scale = canvas->getScale();

    if (scale >= 1.0)
        title = title + " " + QString::number(scale * 100, 'f', 0) + "%";
    else
        title = title + " " + QString::number(scale * 100, 'f', 2) + "%";

    setWindowTitle(title);
}

void MainWindow::showStatusBar(bool s)
{
    if(s)
        statusBar->show();
    else
        statusBar->hide();
}

void MainWindow::updateStatus()
{
    if (!statusBar->isVisible())
        return;

    statusBar->showMessage(
        QString().sprintf("FPS: %.02f Events/sec: %.02f", canvas->frameRate.getRate(), canvas->mouseEventRate.getRate()));
}

void MainWindow::updateLayers()
{
    freezeLayerList = true;

    QList<QString> canvasLayers = canvas->getLayerList();

    layersList->clear();
    foreach(QString layerName, canvasLayers)
    {
        QListWidgetItem *item = new QListWidgetItem(layerName);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        layersList->addItem(item);
    }
    layersList->setCurrentRow((layersList->count() - 1) - canvas->getActiveLayer());

    freezeLayerList = false;
}

void MainWindow::layerListNameEdited(QListWidgetItem * item)
{
    freezeLayerList = true;
    int layerIdx = (layersList->count() - 1) - layersList->row(item);
    canvas->renameLayer(layerIdx, item->text());
    freezeLayerList = false;
}

void MainWindow::updateTool()
{
    blockSignals(true);
    ui->toolColorDial->setColor(canvas->getToolColor());

    float sizeFactor = canvas->getToolSizeFactor();

    if (sizeFactor > 1.0f)
        ui->toolSizeSlider->setValue((sizeFactor - 1.0f) * 10.0f);
    else if (sizeFactor < 1.0f)
        ui->toolSizeSlider->setValue(-(((1.0f / sizeFactor) - 1.0f) * 10.0f));
    else
        ui->toolSizeSlider->setValue(1.0f);

    blockSignals(false);
}

void MainWindow::layerListSelection(int row)
{
    /* Don't update the selection while the list is changing */
    if (freezeLayerList)
        return;

    int layerIdx = (layersList->count() - 1) - row;
    canvas->setActiveLayer(layerIdx);
}

void MainWindow::layerListAdd()
{
    canvas->addLayerAbove(canvas->getActiveLayer());
}

void MainWindow::layerListRemove()
{
    canvas->removeLayer(canvas->getActiveLayer());
}

void MainWindow::layerListMoveUp()
{
    int activeLayerIdx = canvas->getActiveLayer();
    canvas->moveLayer(activeLayerIdx, activeLayerIdx + 1);
}

void MainWindow::layerListMoveDown()
{
    int activeLayerIdx = canvas->getActiveLayer();
    canvas->moveLayer(activeLayerIdx, activeLayerIdx - 1);
}

void MainWindow::canvasStats()
{
    updateStatus();
}


void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
#ifdef Q_OS_MAC
    /* As of QT 5.2 single key shortcuts are not handled correctly (QTBUG-33015), the following
     * hacks around that by searching the main windows action list for shortcuts coresponding
     * to any unhandled key events.
     */
    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != 0)
        return;

    QKeySequence shortcutKey = QKeySequence(event->key() + event->modifiers());

    QList<QAction *>windowActions = findChildren<QAction *>();
    for (QList<QAction *>::Iterator iter = windowActions.begin(); iter != windowActions.end(); ++iter)
    {
        QAction *action = *iter;
        if (action->shortcut() == shortcutKey && action->isEnabled())
        {
            action->trigger();
            break;
        }
    }
#endif
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!infoWindow.isNull())
        infoWindow->close();
    if (!benchmarkWindow.isNull())
        benchmarkWindow->close();
    event->accept();
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

void MainWindow::actionOpenFile()
{
    QString filename = QFileDialog::getOpenFileName(this, "Open", QDir::homePath(), "OpenRaster (*.ora)");

    if (filename.isEmpty())
        return;

    canvas->openORA(filename);
}

void MainWindow::actionSaveAs()
{
    QString filename = QFileDialog::getSaveFileName(this, "Save As...",
                                                    QDir::homePath() + QDir::toNativeSeparators("/untitled.ora"),
                                                    "OpenRaster (*.ora)");

    if (filename.isEmpty())
        return;

    canvas->saveAsORA(filename);
}

void MainWindow::actionUndo()
{
    canvas->undo();
}

void MainWindow::actionRedo()
{
    canvas->redo();
}

void MainWindow::actionToolSizeIncrease()
{
    float size = canvas->getToolSizeFactor() * 2;
    canvas->setToolSizeFactor(size);
}

void MainWindow::actionToolSizeDecrease()
{
    float size = canvas->getToolSizeFactor() / 2;
    canvas->setToolSizeFactor(size);
}

void MainWindow::colorDialChanged(QColor const &color)
{
    canvas->setToolColor(color);
}

void MainWindow::sizeSliderMoved(int value)
{
    float sizeMultiplyer;

    if (value > 0)
    {
        sizeMultiplyer = value / 10.0f + 1.0f;
    }
    else if (value < 0)
    {
        sizeMultiplyer = 1.0f / (-value / 10.0f + 1.0f);
    }
    else
    {
        sizeMultiplyer = 1.0f;
    }

    blockSignals(true);
    canvas->setToolSizeFactor(sizeMultiplyer);
    blockSignals(false);
}

void MainWindow::zoomIn()
{
    canvas->setScale(canvas->getScale() * 2);
    updateTitle();
}

void MainWindow::zoomOut()
{
    canvas->setScale(canvas->getScale() * 0.5);
    updateTitle();
}

double drawBenchmarkCircle(CanvasWidget *canvas, float radius, float centerX, float centerY, int numPoints)
{
    QElapsedTimer timer;
    float currentAngle = 0;

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

    return runTime;
}

void MainWindow::runCircleBenchmark()
{
    if (benchmarkWindow.isNull())
        {
            benchmarkWindow.reset(new BenchmarkDialog (this));
            benchmarkWindow->setWindowTitle(QApplication::applicationDisplayName() + " - Benchmark");
        }

    if (benchmarkWindow->isVisible())
        benchmarkWindow->raise();
    else
        benchmarkWindow->show();

    QString outputText;

    float radius = 100;
    float centerX = 150;
    float centerY = 150;

    const int runPoints = 200;

    double totalTime = 0.0;
    double totalPoints = 0.0;
    int numRuns = 0;
    int localRuns = 0;
    double lastAverage = 0.0;
    int currentRun = 0;

    setEnabled(false);
    canvas->setUpdatesEnabled(false);
    benchmarkWindow->setOutputText(outputText);
    QCoreApplication::processEvents();

    // Discard first run
    {
        double runTime = drawBenchmarkCircle(canvas, radius, centerX, centerY, runPoints);

        qDebug() << "First run" << runTime << "ms (discarded)";
        outputText += QString().sprintf("First run\t%.4fms (discarded)\n", runTime);
        benchmarkWindow->setOutputText(outputText);
        QCoreApplication::processEvents();
    }

    while (true)
    {
        double runTime = drawBenchmarkCircle(canvas, radius, centerX, centerY, runPoints);
        totalTime += runTime;
        totalPoints += runPoints;
        numRuns += 1;
        localRuns += 1;

        qDebug() << "Run" << runTime << "ms";
        outputText += QString().sprintf("Run %d\t%.4fms\n", currentRun, runTime);

        double newAverage = totalTime / numRuns;
        if (localRuns > 4)
        {
            localRuns = 0;
            //qDebug() << newAverage << lastAverage;
            if (fabs(newAverage - lastAverage) < newAverage * 0.0025)
                break;
        }

        if(totalTime >= 1000.0 * 10)
        {
            qDebug() << "Failure to converge, giving up.";
            outputText += "Failure to converge, giving up.";
            break;
        }

        lastAverage = newAverage;
        currentRun += 1;
        benchmarkWindow->setOutputText(outputText);
        QCoreApplication::processEvents();
    }

    qDebug() << "Benchmark" << lastAverage << "ms";
    outputText += "======\n";
    outputText += QString().sprintf("Average\t%.4fms", lastAverage);

    benchmarkWindow->setOutputText(outputText);
    canvas->setUpdatesEnabled(true);
    setEnabled(true);
}
