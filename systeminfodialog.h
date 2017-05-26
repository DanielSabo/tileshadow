#ifndef SYSTEMINFODIALOG_H
#define SYSTEMINFODIALOG_H

#include <QDialog>
#include <memory>

namespace Ui {
class SystemInfoDialog;
}

class SystemInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SystemInfoDialog(QWidget *parent = 0);
    ~SystemInfoDialog();

protected:
    void showEvent(QShowEvent *event);

private:
    std::unique_ptr<Ui::SystemInfoDialog> ui;
};

#endif // SYSTEMINFODIALOG_H
