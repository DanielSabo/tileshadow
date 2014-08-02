#include "benchmarkdialog.h"
#include "ui_benchmarkdialog.h"
#include "mainwindow.h"

BenchmarkDialog::BenchmarkDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BenchmarkDialog)
{
  ui->setupUi(this);
}

void BenchmarkDialog::setOutputText(const QString &txt)
{
    ui->outputLabel->setText(txt);
}

void BenchmarkDialog::runButtonClicked()
{
    MainWindow *mainWindow = qobject_cast<MainWindow *>(parent());
    if (mainWindow)
        mainWindow->runCircleBenchmark();
}
