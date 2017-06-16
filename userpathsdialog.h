#ifndef USERPATHSDIALOG_H
#define USERPATHSDIALOG_H

#include <QDialog>
#include <memory>

namespace Ui {
class UserPathsDialog;
}

class UserPathsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserPathsDialog(QWidget *parent = 0);
    ~UserPathsDialog();

    void savePaths();

protected:
    void showEvent(QShowEvent *event);

private:
    std::unique_ptr<Ui::UserPathsDialog> ui;
};

#endif // USERPATHSDIALOG_H
