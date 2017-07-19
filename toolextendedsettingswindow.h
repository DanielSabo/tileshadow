#ifndef TOOLEXTENDEDSETTINGSWINDOW_H
#define TOOLEXTENDEDSETTINGSWINDOW_H

#include <QWidget>
#include <QImage>
#include <memory>

namespace Ui {
class ToolExtendedSettingsWindow;
}

class CanvasWidget;
class ToolExtendedSettingsWindowPrivate;
class ToolExtendedSettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ToolExtendedSettingsWindow(CanvasWidget *canvas, QWidget *parent = 0);
    ~ToolExtendedSettingsWindow();

protected:
    void closeEvent(QCloseEvent *);
    void keyPressEvent(QKeyEvent *);
    bool event(QEvent *);

private:
    std::unique_ptr<Ui::ToolExtendedSettingsWindow> ui;
    QScopedPointer<ToolExtendedSettingsWindowPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(ToolExtendedSettingsWindow)

signals:

public slots:
    void updateTool(bool pathChangeOrReset);
};

#endif // TOOLEXTENDEDSETTINGSWINDOW_H
