#ifndef TOOLLISTWIDGET_H
#define TOOLLISTWIDGET_H

#include <QWidget>

typedef QList<QPair<QString, QString> > ToolList;

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
    ToolList toolList;
    ToolListPopup *popup;

private:
    ToolListWidgetPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(ToolListWidget)

private slots:
    void updateTool();
    void setActiveTool();
    void showPopup();
    void canvasDestroyed(QObject *obj);
};

#endif // TOOLLISTWIDGET_H
