#ifndef BENCHMARKDIALOG_H
#define BENCHMARKDIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QLabel>

namespace Ui {
class BenchmarkDialog;
}

class BenchmarkDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BenchmarkDialog(QWidget *parent = 0);
    void setOutputText(const QString &txt);

private:
    Ui::BenchmarkDialog *ui;

signals:

public slots:
    void runButtonClicked();

};

#endif // BENCHMARKDIALOG_H
