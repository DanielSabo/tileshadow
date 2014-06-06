#ifndef CANVASUNDOEVENT_H
#define CANVASUNDOEVENT_H

#include <QSharedPointer>
#include "canvasstack.h"
#include "canvaslayer.h"

class CanvasUndoEvent
{
public:
    CanvasUndoEvent();
    virtual ~CanvasUndoEvent();
    virtual TileSet apply(CanvasStack *stack, int *activeLayer) = 0;
};

class CanvasUndoTiles : public CanvasUndoEvent
{
public:
    CanvasUndoTiles();
    ~CanvasUndoTiles();
    TileSet apply(CanvasStack *stack, int *activeLayer);

    int currentLayer;
    QSharedPointer<TileMap> targetTileMap;
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

#endif // CANVASUNDOEVENT_H
