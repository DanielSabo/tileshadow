#ifndef TOOLEXTENDEDSETTINGSWINDOW_H
#define TOOLEXTENDEDSETTINGSWINDOW_H

#include <QWidget>

class CanvasWidget;
class ToolExtendedSettingsWindowPrivate;
class ToolExtendedSettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ToolExtendedSettingsWindow(CanvasWidget *canvas, QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *);
    void keyPressEvent(QKeyEvent *);

private:
    ToolExtendedSettingsWindowPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(ToolExtendedSettingsWindow)

signals:

public slots:
    void updateTool();
};

#endif // TOOLEXTENDEDSETTINGSWINDOW_H
