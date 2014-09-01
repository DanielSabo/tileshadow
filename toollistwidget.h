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
    explicit ToolListWidget(QWidget *parent = 0);
    ~ToolListWidget();

    void reloadTools();
    void setCanvas(CanvasWidget *canvas);
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
    void canvasDestroyed(QObject *obj);
};

#endif // TOOLLISTWIDGET_H
