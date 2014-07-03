#ifndef TOOLLISTWIDGET_H
#define TOOLLISTWIDGET_H

#include <QWidget>

class CanvasWidget;
class ToolListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ToolListWidget(QWidget *parent = 0);
    ~ToolListWidget();

    void reloadTools();
    void setCanvas(CanvasWidget *canvas);

protected:
  CanvasWidget *canvas;

private slots:
    void updateTool();
    void setActiveTool();
    void canvasDestroyed(QObject *obj);
};

#endif // TOOLLISTWIDGET_H
