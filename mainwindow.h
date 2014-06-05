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

private:
    Ui::MainWindow *ui;
    QScopedPointer<SystemInfoDialog> infoWindow;
    QScopedPointer<BenchmarkDialog> benchmarkWindow;
    CanvasWidget *canvas;
    QStatusBar *statusBar;
    QListWidget *layersList;
    bool freezeLayerList;

    void reloadTools();
    void updateTitle();
    void updateStatus();

private slots:
    void setActiveTool();
    void sizeSliderMoved(int value);
    void zoomIn();
    void zoomOut();

public slots:
    void showOpenCLInfo();
    void runCircleBenchmark();
    void actionQuit();
    void actionOpenFile();
    void actionSaveAs();
    void canvasStats();
    void showStatusBar(bool visible);
    void layerListSelection(int row);
    void layerListAdd();
    void layerListRemove();
    void updateLayers();
    void colorDialChanged(QColor const &color);
};

#endif // MAINWINDOW_H
