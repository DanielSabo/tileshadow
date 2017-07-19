#ifndef TOOLSETTINGSWIDGET_H
#define TOOLSETTINGSWIDGET_H

#include <QWidget>

class CanvasWidget;
class ToolSettingsWidgetPrivate;
class ToolSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToolSettingsWidget(CanvasWidget *canvas, QWidget *parent);
    ~ToolSettingsWidget();

private:
    QScopedPointer<ToolSettingsWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(ToolSettingsWidget)

private slots:
    void updateTool(bool pathChangeOrReset);
};

#endif // TOOLSETTINGSWIDGET_H
