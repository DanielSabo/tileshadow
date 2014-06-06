#ifndef CANVASSTACK_H
#define CANVASSTACK_H

#include <QList>
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

    TileSet getTileSet();
    CanvasTile *getTileAt(int x, int y);
    float *openTileAt(int x, int y);
    cl_mem clOpenTileAt(int x, int y);

    TileMap tiles;
    CanvasTile *backgroundTile;
    CanvasTile *backgroundTileCL;
};

#endif // CANVASSTACK_H
