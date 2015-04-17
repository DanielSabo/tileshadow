#ifndef TOOLLISTWIDGET_H
#define TOOLLISTWIDGET_H

#include <QWidget>

class CanvasWidget;
class ToolListPopup;
class ToolListWidgetPrivate;
class ToolListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ToolListWidget(CanvasWidget *canvas, QWidget *parent);

    void reloadTools();
    void pickTool(QString const &toolPath);

protected:
    CanvasWidget *canvas;
    ToolListPopup *popup;

private:
    ToolListWidgetPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(ToolListWidget)

private slots:
    void updateTool();
    void showPopup();
};

#endif // TOOLLISTWIDGET_H
