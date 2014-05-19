#ifndef CANVASSTACK_H
#define CANVASSTACK_H

#include <QList>
#include "canvaswidget-opencl.h"
#include <map>

class CanvasTile;
class CanvasLayer;

class CanvasStack
{
public:
    CanvasStack();

    void newLayerAt(int index);
    void removeLayerAt(int index);

    QList<CanvasLayer *> layers;

    CanvasTile *getTileAt(int x, int y);
    float *openTileAt(int x, int y);
    cl_mem clOpenTileAt(int x, int y);

    std::map<uint64_t, CanvasTile *> tiles;
    CanvasTile *backgroundTile;
    CanvasTile *backgroundTileCL;
};

#endif // CANVASSTACK_H
