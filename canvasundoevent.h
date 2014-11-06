#ifndef CANVASUNDOEVENT_H
#define CANVASUNDOEVENT_H

#include "canvasstack.h"
#include "canvaslayer.h"
#include "canvastile.h"

class CanvasUndoEvent
{
public:
    CanvasUndoEvent();
    virtual ~CanvasUndoEvent();
    virtual bool modifiesBackground();
    virtual TileSet apply(CanvasStack *stack, int *activeLayer) = 0;
};

class CanvasUndoTiles : public CanvasUndoEvent
{
public:
    CanvasUndoTiles();
    ~CanvasUndoTiles();
    TileSet apply(CanvasStack *stack, int *activeLayer);

    int currentLayer;
    std::shared_ptr<TileMap> targetTileMap;
    TileMap tiles;
};

class CanvasUndoLayers : public CanvasUndoEvent
{
public:
    CanvasUndoLayers();
    CanvasUndoLayers(CanvasStack *stack, int activeLayer);
    ~CanvasUndoLayers();
    TileSet apply(CanvasStack *stack, int *activeLayer);

    int currentLayer;
    QList<CanvasLayer *> layers;
};

class CanvasUndoBackground : public CanvasUndoEvent
{
public:
    CanvasUndoBackground(CanvasStack *stack);
    TileSet apply(CanvasStack *stack, int *activeLayer);
    bool modifiesBackground();

    std::unique_ptr<CanvasTile> tile;
};

#endif // CANVASUNDOEVENT_H
