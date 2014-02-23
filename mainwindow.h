#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QScopedPointer>
#include "systeminfodialog.h"
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

private:
    Ui::MainWindow *ui;
    QScopedPointer<SystemInfoDialog> infoWindow;
    CanvasWidget *canvas;

private slots:
    void showOpenCLInfo();
    void actionQuit();
    void setActiveTool();
    void runCircleBenchmark();
};

#endif // MAINWINDOW_H
