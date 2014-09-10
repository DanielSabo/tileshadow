#ifndef CANVASSTACK_H
#define CANVASSTACK_H

#include <QList>
#include <memory>
#include "canvaswidget-opencl.h"
#include "tileset.h"

class CanvasTile;
class CanvasLayer;

class CanvasStack
{
public:
    CanvasStack();
    ~CanvasStack();

    void newLayerAt(int index, QString name = "");
    void removeLayerAt(int index);
    void clearLayers();

    QList<CanvasLayer *> layers;

    TileSet getTileSet() const;
    std::unique_ptr<CanvasTile> getTileMaybe(int x, int y) const;

    CanvasTile *backgroundTile;
    CanvasTile *backgroundTileCL;
};

#endif // CANVASSTACK_H
