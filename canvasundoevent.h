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
    virtual TileSet apply(CanvasStack *stack) = 0;
};

class CanvasUndoTiles : public CanvasUndoEvent
{
public:
    CanvasUndoTiles();
    ~CanvasUndoTiles();
    TileSet apply(CanvasStack *stack);

    QSharedPointer<TileMap> targetTileMap;
    TileMap tiles;
};

#endif // CANVASUNDOEVENT_H
