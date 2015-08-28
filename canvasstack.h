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

    void clearLayers();

    int size() const { return layers.size(); }
    bool empty() const { return layers.empty(); }

    QList<CanvasLayer *> layers;

    TileSet getTileSet() const;
    std::unique_ptr<CanvasTile> getTileMaybe(int x, int y) const;

    std::unique_ptr<CanvasTile> backgroundTile;
    std::unique_ptr<CanvasTile> backgroundTileCL;

    void setBackground(std::unique_ptr<CanvasTile> newBackground);
};

#endif // CANVASSTACK_H
