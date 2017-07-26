#ifndef TOOLLISTWIDGET_H
#define TOOLLISTWIDGET_H

#include <QWidget>

class CanvasWidget;
class ToolListWidgetPrivate;
class ToolListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ToolListWidget(CanvasWidget *canvas, QWidget *parent);
    ~ToolListWidget();

    void reloadTools();
    void pickTool(QString const &toolPath);

protected:
    CanvasWidget *canvas;

private:
    QScopedPointer<ToolListWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(ToolListWidget)

private slots:
    void updateTool();
    void showPopup();
};

#endif // TOOLLISTWIDGET_H
