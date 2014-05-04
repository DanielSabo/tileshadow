#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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

    void reloadTools();

private slots:
    void setActiveTool();
    void sizeSliderMoved(int value);

public slots:
    void showOpenCLInfo();
    void runCircleBenchmark();
    void actionQuit();
};

#endif // MAINWINDOW_H
