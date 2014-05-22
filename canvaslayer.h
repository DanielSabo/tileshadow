#ifndef CANVASLAYER_H
#define CANVASLAYER_H

#include "canvaswidget-opencl.h"
#include <map>

class CanvasTile;

class CanvasLayer
{
public:
    CanvasLayer();
    ~CanvasLayer();

    std::map<uint64_t, CanvasTile *> tiles;

    float *openTileAt(int x, int y);
    cl_mem clOpenTileAt(int x, int y);

    CanvasTile *getTile(int x, int y);
};

#endif // CANVASLAYER_H
