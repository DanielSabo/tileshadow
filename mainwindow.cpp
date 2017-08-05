#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <qmath.h>
#include <QElapsedTimer>
#include "canvaswidget-opencl.h"
#include <QDir>
#include <QPushButton>
#include <QCloseEvent>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QImageWriter>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QClipboard>
#include <QMimeData>
#include "canvastile.h"
#include "hsvcolordial.h"
#include "toolfactory.h"
#include "toolsettingswidget.h"
#include "toollistwidget.h"
#include "layerlistwidget.h"
#include "deviceselectdialog.h"
#include "userpathsdialog.h"
#include "filedialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connectActions();
    statusBarLabel = new QLabel();
    ui->statusBar->addWidget(statusBarLabel);

    canvas = ui->mainCanvas;

    auto addSidebarWidget = [&](QWidget *widget, QSizePolicy::Policy vp) {
        ui->sidebarLayout->insertWidget(ui->sidebarLayout->count(), widget);
        QSizePolicy sizePolicy = widget->sizePolicy();
        sizePolicy.setVerticalPolicy(vp);
        widget->setSizePolicy(sizePolicy);

    };

    addSidebarWidget(new ToolSettingsWidget(canvas, this), QSizePolicy::Preferred);
    addSidebarWidget(new ToolListWidget(canvas, this), QSizePolicy::Preferred);
    addSidebarWidget(new LayerListWidget(canvas, this), QSizePolicy::Expanding);

    updateTitle();
    updateStatus();

    connect(canvas, &CanvasWidget::updateStats, this, &MainWindow::canvasStats);
    connect(canvas, &CanvasWidget::updateTool, this, &MainWindow::updateTool);
    connect(canvas, &CanvasWidget::canvasModified, this, &MainWindow::canvasModified);

    setAcceptDrops(true);

    // Resize here because the big widget is unwieldy in the designer

    QSettings appSettings;

    if (appSettings.contains("MainWindow/geometry"))
        restoreGeometry(appSettings.value("MainWindow/geometry").toByteArray());
    else
        resize(700, 400);

    showStatusBar(appSettings.value("MainWindow/statusBar", false).toBool());

#ifdef Q_OS_MAC
    // On OSX the window icon represents the current file
    setWindowIcon(QIcon(":icons/image-x-generic.png"));
#else
    // On other platforms it represents the application
    QIcon windowIcon;
    windowIcon.addFile(":icons/app-tileshadow-16.png");
    windowIcon.addFile(":icons/app-tileshadow-32.png");
    windowIcon.addFile(":icons/app-tileshadow-128.png");
    setWindowIcon(windowIcon);
#endif

#ifdef Q_OS_MAC
    QApplication::instance()->installEventFilter(this);
    ui->splitter->installEventFilter(this);
    ui->splitter->setStyleSheet(QStringLiteral(
        "QSplitterHandle:hover {}"
        "QSplitter::Handle {"
        "background-image: none;"
        "}"
        "QSplitter::Handle:hover {"
        "background-color: palette(highlight);"
        "}"));
#endif

    setFocus();
}

MainWindow::~MainWindow()
{
}

void MainWindow::connectActions()
{
    // File
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::actionNewFile);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::actionOpenFile);
    connect(ui->actionOpen_As_Layer, &QAction::triggered, this, &MainWindow::actionOpenAsLayer);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::actionSave);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindow::actionSaveAs);
    connect(ui->actionExport, &QAction::triggered, this, &MainWindow::actionExport);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::actionQuit);

    // Edit
    connect(ui->actionUndo, &QAction::triggered, this, &MainWindow::actionUndo);
    connect(ui->actionRedo, &QAction::triggered, this, &MainWindow::actionRedo);
    connect(ui->actionNew_Layer, &QAction::triggered, this, &MainWindow::actionNewLayer);
    connect(ui->actionNew_Group, &QAction::triggered, this, &MainWindow::actionNewGroup);
    connect(ui->actionMerge_Layer, &QAction::triggered, this, &MainWindow::actionMergeLayerDown);
    connect(ui->actionDuplicate_Layer, &QAction::triggered, this, &MainWindow::actionDuplicateLayer);
    connect(ui->actionRotate_Layer, &QAction::triggered, this, &MainWindow::actionRotateLayer);
    connect(ui->actionScale_Layer, &QAction::triggered, this, &MainWindow::actionScaleLayer);
    connect(ui->actionSet_Background_Color, &QAction::triggered, this, &MainWindow::actionSetBackgroundColor);
    connect(ui->actionToggle_Quickmask, &QAction::triggered, this, &MainWindow::actionToggleQuickmask);
    connect(ui->actionQuickmask_Cut, &QAction::triggered, this, &MainWindow::actionQuickmaskCut);
    connect(ui->actionQuickmask_Copy, &QAction::triggered, this, &MainWindow::actionQuickmaskCopy);
    connect(ui->actionQuickmask_Erase, &QAction::triggered, this, &MainWindow::actionQuickmaskErase);
    connect(ui->actionClear_Quickmask, &QAction::triggered, this, &MainWindow::actionClearQuickmask);
    connect(ui->actionEdit_Frame, &QAction::triggered, this, &MainWindow::actionToggleEditFrame);
    connect(ui->actionToggle_Frame, &QAction::triggered, this, &MainWindow::actionToggleFrame);

    // View
    connect(ui->actionZoom_In, &QAction::triggered, this, &MainWindow::actionZoomIn);
    connect(ui->actionZoom_Out, &QAction::triggered, this, &MainWindow::actionZoomOut);
    connect(ui->actionReset_View, &QAction::triggered, this, &MainWindow::actionResetView);
    connect(ui->actionMirror_Vertical, &QAction::triggered, this, &MainWindow::actionViewMirrorVertical);
    connect(ui->actionMirror_Horizontal, &QAction::triggered, this, &MainWindow::actionViewMirrorHorizontal);
    connect(ui->actionRotate_View_Counterclockwise, &QAction::triggered, this, &MainWindow::actionViewRotateCounterclockwise);
    connect(ui->actionRotate_View_Clockwise, &QAction::triggered, this, &MainWindow::actionViewRotateClockwise);

    // Tool
    connect(ui->actionIncrease_Tool_Size, &QAction::triggered, this, &MainWindow::actionToolSizeIncrease);
    connect(ui->actionDecrease_Tool_Size, &QAction::triggered, this, &MainWindow::actionToolSizeDecrease);
    connect(ui->actionAdvanced_Settings, &QAction::triggered, this, &MainWindow::actionAdvancedToolSettings);
    connect(ui->actionResetTool, &QAction::triggered, this, &MainWindow::actionResetTool);
    connect(ui->actionDraw_Line, &QAction::triggered, this, &MainWindow::actionDrawLine);

    // System
    connect(ui->actionCircle_Benchmark, &QAction::triggered, this, &MainWindow::runCircleBenchmark);
    connect(ui->actionCopy_Stroke_Data, &QAction::triggered, this, &MainWindow::actionCopyStrokeData);
    connect(ui->actionSelect_OpenCL_Device, &QAction::triggered, this, &MainWindow::showDeviceSelect);
    connect(ui->actionShow_OpenCL_Information, &QAction::triggered, this, &MainWindow::showOpenCLInfo);
    connect(ui->actionStatus_Bar, &QAction::toggled, this, &MainWindow::showStatusBar);
    connect(ui->actionResource_Paths, &QAction::triggered, this, &MainWindow::showResourcePaths);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        openFileRequest(static_cast<QFileOpenEvent *>(event)->file());
        return true;
    }
#ifdef Q_OS_MAC
    if (QSplitterHandle *handle = qobject_cast<QSplitterHandle *>(obj))
    {
        /* Hack around QTBUG-33479 */
        if (event->type() == QEvent::Leave)
        {
            unsetCursor();
        }
        else if (event->type() == QEvent::Enter)
        {
            if (handle->orientation() == Qt::Horizontal)
                setCursor(Qt::SplitHCursor);
            else
                setCursor(Qt::SplitVCursor);
        }
        else if (event->type() == QEvent::Polish)
        {
            /* Enable the :hover selector in custom stylesheets */
            handle->setAttribute(Qt::WA_Hover);
        }

        return false;
    }
    else if (event->type() == QEvent::Show)
    {
        if (qobject_cast<QFileDialog *>(obj))
        {
            ui->actionCut->setVisible(true);
            ui->actionCopy->setVisible(true);
            ui->actionPaste->setVisible(true);
        }
    }
    else if (event->type() == QEvent::Hide)
    {
        if (qobject_cast<QFileDialog *>(obj))
        {
            ui->actionCut->setVisible(false);
            ui->actionCopy->setVisible(false);
            ui->actionPaste->setVisible(false);
        }
    }
#endif

    return false;
}

void MainWindow::updateTitle()
{
    QString title;
    setWindowModified(canvas->getModified());

    if (windowFilePath().isEmpty())
        title = QStringLiteral("Unsaved Drawing[*]");
    else
        title = QFileInfo(windowFilePath()).fileName() + "[*]";

    auto vt = canvas->getViewTransform();

    if (vt.scale >= 1.0)
        title += " - " + QString::number(vt.scale * 100, 'f', 0) + "%";
    else
        title += " - " + QString::number(vt.scale * 100, 'f', 2) + "%";

    if ((vt.mirrorHorizontal || vt.mirrorVertical) && (vt.angle != 0.0f))
        title += tr(" (Flipped, Rotated)");
    else if (vt.mirrorHorizontal || vt.mirrorVertical)
        title += tr(" (Flipped)");
    else if (vt.angle != 0.0f)
        title += tr(" (Rotated)");

    setWindowTitle(title);
}

void MainWindow::showStatusBar(bool show)
{
    ui->actionStatus_Bar->setChecked(show);

    if (show)
        ui->statusBar->show();
    else
        ui->statusBar->hide();
    statusBarLabel->setText("");
}

void MainWindow::updateStatus()
{
    if (!ui->statusBar->isVisible())
        return;

    QString message;

    message.sprintf("FPS: %.02f Events/sec: %.02f", canvas->frameRate.getRate(), canvas->mouseEventRate.getRate());

    int deviceAllocated = CanvasTile::deviceTileCount() * TILE_COMP_TOTAL * sizeof(float);
    deviceAllocated /= 1024 * 1024;
    int allocated = CanvasTile::allocatedTileCount() * TILE_COMP_TOTAL * sizeof(float);
    allocated /= 1024 * 1024;
    allocated -= deviceAllocated;

    message += " Tiles: " + QString::number(deviceAllocated) + "MB + " + QString::number(allocated) + "MB";

    statusBarLabel->setText(message);
}

void MainWindow::updateTool()
{
    if (canvas->getAdvancedToolSettings().size())
        ui->actionAdvanced_Settings->setEnabled(true);
    else
        ui->actionAdvanced_Settings->setEnabled(false);
}

void MainWindow::canvasStats()
{
    updateStatus();
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{

#if (QT_VERSION < QT_VERSION_CHECK(5, 5, 0))
#ifdef Q_OS_MAC
    /* As of QT 5.2 single key shortcuts are not handled correctly (QTBUG-33015), the following
     * hacks around that by searching the main windows action list for shortcuts coresponding
     * to any unhandled key events.
     * As of 5.4 lowercase shortcuts work (and therefor the hack would cause double events), but
     * shift still requires the hack.
     */
#if (QT_VERSION < QT_VERSION_CHECK(5, 4, 0))
    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) == 0)
#else
    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier)) == Qt::ShiftModifier)
#endif
    {
        if(!hasFocus())
        {
            QKeyEvent overrideEvent(QEvent::ShortcutOverride,
                                    event->key(),
                                    event->modifiers(),
                                    event->text(),
                                    event->isAutoRepeat(),
                                    event->count());
            overrideEvent.ignore();

            QApplication::instance()->sendEvent(focusWidget(), &overrideEvent);

            if (overrideEvent.isAccepted())
                return;
        }

        QKeySequence shortcutKey = QKeySequence(event->key() + event->modifiers());

        QList<QAction *>windowActions = findChildren<QAction *>();
        for (QAction *action: windowActions)
        {
            if (action->shortcut() == shortcutKey && action->isEnabled())
            {
                action->trigger();
                return;
            }
        }
    }
#endif
#endif

}

namespace {
    bool checkDragUrls(QList<QUrl> const &urls)
    {
        if (!urls.isEmpty() && urls.at(0).isLocalFile())
        {
            QString path = urls.at(0).toLocalFile();
            if (path.endsWith(".ora"))
                return true;
            for (auto const &readerFormat: QImageReader::supportedImageFormats())
                if (path.endsWith(QStringLiteral(".") + readerFormat))
                    return true;
        }
        return false;
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (checkDragUrls(urls))
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (checkDragUrls(urls))
    {
        openFileRequest(urls.at(0).toLocalFile());
        event->acceptProposedAction();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!promptSave())
    {
        event->ignore();
        return;
    }

    QSettings().setValue("MainWindow/geometry", saveGeometry());
    QSettings().setValue("MainWindow/statusBar", ui->statusBar->isVisible());

    if (!infoWindow.isNull())
        infoWindow->close();
    if (!benchmarkWindow.isNull())
        benchmarkWindow->close();
    if (!toolSettingsWindow.isNull())
        toolSettingsWindow->close();
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

void MainWindow::showDeviceSelect()
{
    DeviceSelectDialog().exec();
}

void MainWindow::showResourcePaths()
{
    UserPathsDialog().exec();
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

void MainWindow::openFileRequest(QString const &filename)
{
    if (!promptSave())
        return;

    openFile(filename);
}

void MainWindow::actionOpenFile()
{
    if (!promptSave())
        return;

    QStringList importWildcards;
    importWildcards.append("*.ora");

    for (auto const &readerFormat: QImageReader::supportedImageFormats())
        importWildcards.append(QStringLiteral("*.") + readerFormat);

    QString formats = QStringLiteral("Images (") + importWildcards.join(" ") + ")";

    QString filename = FileDialog::getOpenFileName(this, "Open", QDir::homePath(), formats);

    openFile(filename);
}

void MainWindow::openFile(QString const &filename)
{
    if (filename.isEmpty())
        return;

    if (filename.endsWith(".ora"))
    {
        canvas->openORA(filename);
        setWindowFilePath(filename);
        updateTitle();
    }
    else
    {
        QImage image(filename);
        if (!image.isNull())
        {
            canvas->openImage(image);
            setWindowFilePath("");
            updateTitle();
        }
    }
}

void MainWindow::actionOpenAsLayer()
{
    QStringList importWildcards;

    for (auto const &readerFormat: QImageReader::supportedImageFormats())
        importWildcards.append(QStringLiteral("*.") + readerFormat);

    QString formats = QStringLiteral("Images (") + importWildcards.join(" ") + ")";

    QString filename = FileDialog::getOpenFileName(this, "Open", QDir::homePath(), formats);

    if (!filename.isEmpty())
    {
        QImage image(filename);
        if (!image.isNull())
        {
            canvas->addLayerAbove(canvas->getActiveLayer(), image, QFileInfo(filename).fileName());
        }
    }
}

void MainWindow::actionSave()
{
    doSave(windowFilePath());
}

bool MainWindow::doSave(QString filename)
{
    if (filename.isEmpty())
    {
        QString saveDirectory = QDir::homePath();
        if (!windowFilePath().isEmpty())
            saveDirectory = QFileInfo(windowFilePath()).dir().path();
        filename = FileDialog::getSaveFileName(this, "Save As...",
                                               saveDirectory + QDir::toNativeSeparators("/untitled.ora"),
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

void MainWindow::actionExport()
{
    QList<QByteArray> writerKnownFormats = QImageWriter::supportedImageFormats();
    QList<QByteArray> exportFormats;
    exportFormats.append("png");
    exportFormats.append("bmp");
    exportFormats.append("jpg");
    exportFormats.append("jpeg");

    QStringList exportWildcards;

    for(int i = 0; i < exportFormats.size(); ++i)
        if (writerKnownFormats.contains(exportFormats[i]))
            exportWildcards.append(QStringLiteral("*.") + exportFormats[i]);

    QString formats;
    if (!exportWildcards.isEmpty())
        formats = QStringLiteral("Export formats ") + "(" + exportWildcards.join(" ") + ")";
    else
        return;

    QString filename = FileDialog::getSaveFileName(this, tr("Export..."),
                                                   QDir::homePath(),
                                                   formats);

    if (filename.isEmpty())
        return;

    QImage image = canvas->asImage();

    if (image.isNull())
    {
        QMessageBox::warning(this, tr("Could not save image"),
                             tr("Could not save because the document is empty."));
    }
    else
    {
        QImageWriter writer(filename);
        if (filename.endsWith(".jpg", Qt::CaseInsensitive) || filename.endsWith(".jpeg", Qt::CaseInsensitive))
            writer.setQuality(90);
        else if (filename.endsWith(".png", Qt::CaseInsensitive))
            writer.setQuality(9);

        if (!writer.write(image))
        {

            QMessageBox::warning(this, tr("Could not save image"),
                                 tr("Save failed: ") + writer.errorString());
        }
    }
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
    bool hasSize = false;
    double size = canvas->getToolSetting(QStringLiteral("size")).toDouble(&hasSize);

    // FIXME: This is specific to the mypaint log-size property
    if (hasSize)
        canvas->setToolSetting("size", QVariant(size + 0.3));
}

void MainWindow::actionToolSizeDecrease()
{
    bool hasSize = false;
    double size = canvas->getToolSetting(QStringLiteral("size")).toDouble(&hasSize);

    // FIXME: This is specific to the mypaint log-size property
    if (hasSize)
        canvas->setToolSetting("size", QVariant(size - 0.3));
}

void MainWindow::actionResetTool()
{
    canvas->resetToolSettings();
}

void MainWindow::actionAdvancedToolSettings()
{
    if (toolSettingsWindow)
        toolSettingsWindow->raise();
    else
    {
        toolSettingsWindow = new ToolExtendedSettingsWindow(canvas);
        toolSettingsWindow->setAttribute(Qt::WA_DeleteOnClose, true);
        toolSettingsWindow->show();
    }
}

void MainWindow::actionSetBackgroundColor()
{
    QColor color = canvas->getToolColor();

    if (color.isValid())
        canvas->setBackgroundColor(color);
}

void MainWindow::actionMergeLayerDown()
{
    canvas->mergeLayerDown(canvas->getActiveLayer());
}

void MainWindow::actionDuplicateLayer()
{
    canvas->duplicateLayer(canvas->getActiveLayer());
}

void MainWindow::actionNewLayer()
{
    canvas->addLayerAbove(canvas->getActiveLayer());
}

void MainWindow::actionNewGroup()
{
    canvas->addGroupAbove(canvas->getActiveLayer());
}

void MainWindow::actionDrawLine()
{
    canvas->lineDrawMode();
}

void MainWindow::actionToggleEditFrame()
{
    canvas->toggleEditFrame();
}

void MainWindow::actionToggleFrame()
{
    canvas->toggleFrame();
}

void MainWindow::actionToggleQuickmask()
{
    canvas->toggleQuickmask();
}

void MainWindow::actionClearQuickmask()
{
    canvas->clearQuickmask();
}

void MainWindow::actionQuickmaskCut()
{
    canvas->quickmaskCut();
}

void MainWindow::actionQuickmaskCopy()
{
    canvas->quickmaskCopy();
}

void MainWindow::actionQuickmaskErase()
{
    canvas->quickmaskErase();
}

void MainWindow::actionRotateLayer()
{
    canvas->rotateMode();
}

void MainWindow::actionScaleLayer()
{
    canvas->scaleMode();
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

void MainWindow::actionResetView()
{
    auto vt = canvas->getViewTransform();
    vt.angle = 0.0f;
    vt.mirrorHorizontal = false;
    vt.mirrorVertical = false;
    canvas->setViewTransform(vt);
    updateTitle();
}

void MainWindow::actionViewMirrorVertical()
{
    auto vt = canvas->getViewTransform();
    vt.mirrorVertical = !vt.mirrorVertical;
    canvas->setViewTransform(vt);
    updateTitle();
}

void MainWindow::actionViewMirrorHorizontal()
{
    auto vt = canvas->getViewTransform();
    vt.mirrorHorizontal = !vt.mirrorHorizontal;
    canvas->setViewTransform(vt);
    updateTitle();
}

void MainWindow::actionViewRotateCounterclockwise()
{
    auto vt = canvas->getViewTransform();
    if (vt.mirrorHorizontal != vt.mirrorVertical)
        vt.angle += 90.0f;
    else
        vt.angle -= 90.0f;
    canvas->setViewTransform(vt);
    updateTitle();
}

void MainWindow::actionViewRotateClockwise()
{
    auto vt = canvas->getViewTransform();
    if (vt.mirrorHorizontal != vt.mirrorVertical)
        vt.angle -= 90.0f;
    else
        vt.angle += 90.0f;
    canvas->setViewTransform(vt);
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
    bool savedSync = canvas->getSynchronous();
    canvas->setSynchronous(true);

    timer.start();
    canvas->startStroke(QPointF(cosf(currentAngle) * radius + centerX, sinf(currentAngle) * radius + centerY), 1.0f);
    for (int i = 0; i < numPoints; ++i)
    {
        currentAngle += M_PI * 2.0f / 100.0f;
        canvas->strokeTo(QPointF(cosf(currentAngle) * radius + centerX, sinf(currentAngle) * radius + centerY), 1.0f, 1000.0f / 60.0f);
    }
    canvas->endStroke();
    clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);
    double runTime = timer.elapsed();
    canvas->setSynchronous(savedSync);

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
            if (fabs(newAverage - lastAverage) <= newAverage * 0.0025)
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

void MainWindow::actionCopyStrokeData()
{
    QJsonArray strokeArray;
    for (auto const &point : canvas->getLastStrokeData())
        strokeArray.push_back(QJsonArray{point.x, point.y, point.p, point.dt});
    QJsonDocument result(strokeArray);

    QString formattedResult = QString::fromUtf8(result.toJson());
    QApplication::clipboard()->setText(formattedResult);
}
