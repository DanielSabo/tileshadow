#ifndef LAYERLISTWIDGET_H
#define LAYERLISTWIDGET_H

#include <QWidget>
#include <memory>
#include "layershuffletype.h"

namespace Ui {
class LayerListWidget;
}

class CanvasWidget;
class LayerListWidget : public QWidget
{
    Q_OBJECT
private:
    std::unique_ptr<Ui::LayerListWidget> ui;

public:
    explicit LayerListWidget(CanvasWidget *canvas, QWidget *parent);
    ~LayerListWidget();

protected:
    CanvasWidget *canvas;
    bool freezeLayerList;

public slots:
    void layerListAdd();
    void layerListRemove();
    void layerListDuplicate();
    void layerListMoveUp();
    void layerListMoveDown();
    void updateLayers();

private slots:
    void layerModeActivated(int index);

    void opacitySliderMoved(int value);
    void opacitySliderReleased();

    void layerListShuffle(int srcIndex, int targetIndex, LayerShuffle::Type op);
    void layerListItemEdited(int row, int column, QVariant const &data);
    void layerListSelectionChanged(int row);
};

#endif // LAYERLISTWIDGET_H
