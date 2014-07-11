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
    void keyReleaseEvent(QKeyEvent *);

private:
    Ui::MainWindow *ui;
    QScopedPointer<SystemInfoDialog> infoWindow;
    QScopedPointer<BenchmarkDialog> benchmarkWindow;
    CanvasWidget *canvas;

    void updateTitle();
    void updateStatus();
    bool doSave(QString filename);
    bool promptSave();

private slots:
    void zoomIn();
    void zoomOut();
    void canvasModified();

public slots:
    void showOpenCLInfo();
    void runCircleBenchmark();
    void actionQuit();
    void actionNewFile();
    void actionOpenFile();
    void actionSave();
    void actionSaveAs();
    void actionUndo();
    void actionRedo();
    void actionToolSizeIncrease();
    void actionToolSizeDecrease();
    void canvasStats();
    void showStatusBar(bool visible);
    void updateTool();
};

#endif // MAINWINDOW_H
