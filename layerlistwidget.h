#ifndef LAYERLISTWIDGET_H
#define LAYERLISTWIDGET_H

#include <QWidget>

namespace Ui {
class LayerListWidget;
}

class CanvasWidget;
class QListWidgetItem;
class LayerListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LayerListWidget(QWidget *parent = 0);
    ~LayerListWidget();

    void setCanvas(CanvasWidget *newCanvas);

protected:
    CanvasWidget *canvas;
    bool freezeLayerList;

private:
    Ui::LayerListWidget *ui;

public slots:
    void layerListSelection(int row);
    void layerListAdd();
    void layerListRemove();
    void layerListMoveUp();
    void layerListMoveDown();
    void layerListNameEdited(QListWidgetItem *item);
    void updateLayers();

    void canvasDestroyed(QObject *obj);
};

#endif // LAYERLISTWIDGET_H
