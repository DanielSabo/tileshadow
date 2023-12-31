#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QScopedPointer>
#include <QPointer>
#include "systeminfodialog.h"
#include "benchmarkdialog.h"
#include "toolextendedsettingswindow.h"
#include "canvaswidget.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool eventFilter(QObject *obj, QEvent *event);

    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent * event);

private:
    std::unique_ptr<Ui::MainWindow> ui;
    QScopedPointer<SystemInfoDialog> infoWindow;
    QScopedPointer<BenchmarkDialog> benchmarkWindow;
    QPointer<ToolExtendedSettingsWindow> toolSettingsWindow;
    CanvasWidget *canvas;
    QLabel *statusBarLabel;

    void connectActions();

    void updateTitle();
    void updateStatus();
    bool doSave(QString filename);
    bool promptSave();
    void openFile(QString const &filename);

private slots:
    void canvasModified();

public slots:
    void showOpenCLInfo();
    void showDeviceSelect();
    void showResourcePaths();
    void runCircleBenchmark();
    void actionCopyStrokeData();
    void actionQuit();
    void actionNewFile();
    void actionOpenFile();
    void actionOpenAsLayer();
    void actionSave();
    void actionSaveAs();
    void actionExport();
    void actionUndo();
    void actionRedo();
    void actionZoomIn();
    void actionZoomOut();
    void actionResetView();
    void actionViewMirrorVertical();
    void actionViewMirrorHorizontal();
    void actionViewRotateCounterclockwise();
    void actionViewRotateClockwise();
    void actionToolSizeIncrease();
    void actionToolSizeDecrease();
    void actionResetTool();
    void actionAdvancedToolSettings();
    void actionSetBackgroundColor();
    void actionMergeLayerDown();
    void actionDuplicateLayer();
    void actionNewLayer();
    void actionNewGroup();
    void actionDrawLine();
    void actionToggleEditFrame();
    void actionToggleFrame();
    void actionToggleQuickmask();
    void actionClearQuickmask();
    void actionQuickmaskCut();
    void actionQuickmaskCopy();
    void actionQuickmaskErase();
    void actionRotateLayer();
    void actionScaleLayer();
    void canvasStats();
    void showStatusBar(bool visible);
    void updateTool();

    void openFileRequest(QString const &filename);
};

#endif // MAINWINDOW_H
