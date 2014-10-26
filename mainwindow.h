#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QScopedPointer>
#include "systeminfodialog.h"
#include "benchmarkdialog.h"
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

    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;
    QScopedPointer<SystemInfoDialog> infoWindow;
    QScopedPointer<BenchmarkDialog> benchmarkWindow;
    CanvasWidget *canvas;
    QLabel *statusBarLabel;

    void updateTitle();
    void updateStatus();
    bool doSave(QString filename);
    bool promptSave();

private slots:
    void canvasModified();

public slots:
    void showOpenCLInfo();
    void showDeviceSelect();
    void runCircleBenchmark();
    void actionQuit();
    void actionNewFile();
    void actionOpenFile();
    void actionSave();
    void actionSaveAs();
    void actionExport();
    void actionUndo();
    void actionRedo();
    void actionZoomIn();
    void actionZoomOut();
    void actionToolSizeIncrease();
    void actionToolSizeDecrease();
    void actionResetTool();
    void actionSetBackgroundColor();
    void actionMergeLayerDown();
    void canvasStats();
    void showStatusBar(bool visible);
    void updateTool();
};

#endif // MAINWINDOW_H
