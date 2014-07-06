#ifndef TOOLSETTINGSWIDGET_H
#define TOOLSETTINGSWIDGET_H

#include <QWidget>

class CanvasWidget;
class ToolSettingsWidgetPrivate;
class ToolSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToolSettingsWidget(QWidget *parent = 0);

    void setCanvas(CanvasWidget *canvas);

private:
    ToolSettingsWidgetPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(ToolSettingsWidget)

private slots:
    void updateTool();
    void canvasDestroyed(QObject *obj);
};

#endif // TOOLSETTINGSWIDGET_H
