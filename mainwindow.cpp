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
#include <QMessageBox>
#include <QSettings>
#include "hsvcolordial.h"
#include "toolsettingswidget.h"
#include "toollistwidget.h"
#include "layerlistwidget.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    canvas = ui->mainCanvas;
    ui->statusBar->hide();

    {
        QBoxLayout *sidebarLayout = qobject_cast<QBoxLayout *>(ui->sidebar->layout());
        Q_ASSERT(sidebarLayout);

        ToolSettingsWidget *toolSettings = new ToolSettingsWidget();
        toolSettings->setCanvas(canvas);
        sidebarLayout->insertWidget(sidebarLayout->count() - 1, toolSettings);

        LayerListWidget *layerList = new LayerListWidget();
        layerList->setCanvas(canvas);
        sidebarLayout->insertWidget(sidebarLayout->count() - 1, layerList);

        ToolListWidget *toolList = new ToolListWidget();
        toolList->setCanvas(canvas);
        toolList->reloadTools();
        sidebarLayout->insertWidget(sidebarLayout->count() - 1, toolList);
    }

    updateTitle();
    updateStatus();

    connect(canvas, &CanvasWidget::updateStats, this, &MainWindow::canvasStats);
    connect(canvas, &CanvasWidget::updateTool, this, &MainWindow::updateTool);
    connect(canvas, &CanvasWidget::canvasModified, this, &MainWindow::canvasModified);

    // Resize here because the big widget is unwieldy in the designer

    QSettings appSettings;

    if (appSettings.contains("MainWindow/geometry"))
        restoreGeometry(appSettings.value("MainWindow/geometry").toByteArray());
    else
        resize(700, 400);

    canvas->setActiveTool("default.myb");
    canvas->setToolColor(QColor(255, 0, 0));

    setWindowIcon(QIcon(":icons/image-x-generic.png"));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTitle()
{
    QString title;
    setWindowModified(canvas->getModified());

    if (windowFilePath().isEmpty())
        title = QStringLiteral("Unsaved Drawing[*]");
    else
        title = QFileInfo(windowFilePath()).fileName() + "[*]";

    float scale = canvas->getScale();

    if (scale >= 1.0)
        title += " - " + QString::number(scale * 100, 'f', 0) + "%";
    else
        title += " - " + QString::number(scale * 100, 'f', 2) + "%";

    setWindowTitle(title);
}


void MainWindow::showStatusBar(bool show)
{
    if (show)
        ui->statusBar->show();
    else
        ui->statusBar->hide();
}

void MainWindow::updateStatus()
{
    if (!ui->statusBar->isVisible())
        return;

    ui->statusBar->showMessage(
        QString().sprintf("FPS: %.02f Events/sec: %.02f", canvas->frameRate.getRate(), canvas->mouseEventRate.getRate()));
}

void MainWindow::updateTool()
{
}

void MainWindow::canvasStats()
{
    updateStatus();
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    canvas->keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
#ifdef Q_OS_MAC
    /* As of QT 5.2 single key shortcuts are not handled correctly (QTBUG-33015), the following
     * hacks around that by searching the main windows action list for shortcuts coresponding
     * to any unhandled key events.
     */
    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) == 0)
    {
        QKeySequence shortcutKey = QKeySequence(event->key() + event->modifiers());

        QList<QAction *>windowActions = findChildren<QAction *>();
        for (QList<QAction *>::Iterator iter = windowActions.begin(); iter != windowActions.end(); ++iter)
        {
            QAction *action = *iter;
            if (action->shortcut() == shortcutKey && action->isEnabled())
            {
                action->trigger();
                return;
            }
        }
    }
#endif

    canvas->keyReleaseEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!promptSave())
    {
        event->ignore();
        return;
    }

    QSettings().setValue("MainWindow/geometry", saveGeometry());

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

    if (infoWindow->isVisible())
        infoWindow->raise();
    else
        infoWindow->show();
}

void MainWindow::actionQuit()
{
    close();
}

void MainWindow::actionNewFile()
{
    if (!promptSave())
        return;

    canvas->newDrawing();
    setWindowFilePath("");
    updateTitle();
}

void MainWindow::actionOpenFile()
{
    if (!promptSave())
        return;

    QString filename = QFileDialog::getOpenFileName(this, "Open", QDir::homePath(), "OpenRaster (*.ora)");

    if (filename.isEmpty())
        return;

    canvas->openORA(filename);
    setWindowFilePath(filename);
    updateTitle();
}

void MainWindow::actionSave()
{
    doSave(windowFilePath());
}

bool MainWindow::doSave(QString filename)
{
    if (filename.isEmpty())
    {
        filename = QFileDialog::getSaveFileName(this, "Save As...",
                                                QDir::homePath() + QDir::toNativeSeparators("/untitled.ora"),
                                                "OpenRaster (*.ora)");

        if (filename.isEmpty())
            return false;
    }

    canvas->saveAsORA(filename);
    setWindowFilePath(filename);
    updateTitle();

    return true;
}

/* Prompt the user to save before continuing, return true if
 * the operation should continue (they saved or discarded).
 */
bool MainWindow::promptSave()
{
    if (canvas->getModified())
    {
        QMessageBox savePrompt(this);
        savePrompt.setIcon(QMessageBox::Warning);
        if (windowFilePath().isEmpty())
            savePrompt.setText(tr("There are unsaved changes, are you sure you want to discard this drawing?"));
        else
        {
            QString promptText = tr("There are unsaved changes to \"%1\". Are you sure you want to discard this drawing?")
                                    .arg(QFileInfo(windowFilePath()).fileName());
            savePrompt.setText(promptText);
        }
        savePrompt.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        savePrompt.setDefaultButton(QMessageBox::Save);

        int result = savePrompt.exec();
        if (result == QMessageBox::Save)
        {
            if (!doSave(windowFilePath()))
                return false;
        }
        else if (result != QMessageBox::Discard)
        {
            return false;
        }
    }

    return true;
}

void MainWindow::actionSaveAs()
{
    doSave(QString());
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

void MainWindow::actionZoomIn()
{
    canvas->setScale(canvas->getScale() * 2);
    updateTitle();
}

void MainWindow::actionZoomOut()
{
    canvas->setScale(canvas->getScale() * 0.5);
    updateTitle();
}

void MainWindow::canvasModified()
{
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
        benchmarkWindow.reset(new BenchmarkDialog (this));

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
