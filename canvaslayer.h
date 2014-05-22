#ifndef CANVASLAYER_H
#define CANVASLAYER_H

#include "canvaswidget-opencl.h"
#include "tileset.h"

class CanvasTile;

class CanvasLayer
{
public:
    CanvasLayer();
    ~CanvasLayer();

    TileMap tiles;

    TileSet getTileSet();

    float *openTileAt(int x, int y);
    cl_mem clOpenTileAt(int x, int y);

    CanvasTile *getTile(int x, int y);
    CanvasTile *getTileMaybe(int x, int y);
};

#endif // CANVASLAYER_H
