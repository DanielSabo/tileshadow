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
    virtual TileSet apply(CanvasStack *stack,
                          int         *activeLayer,
                          CanvasLayer *quickmask,
                          QRect       *activeFrame,
                          QRect       *inactiveFrame) = 0;
};

class CanvasUndoTiles : public CanvasUndoEvent
{
public:
    CanvasUndoTiles();
    ~CanvasUndoTiles();
    TileSet apply(CanvasStack *stack,
                  int         *activeLayer,
                  CanvasLayer *quickmask,
                  QRect       *activeFrame,
                  QRect       *inactiveFrame);

    int currentLayer;
    std::shared_ptr<TileMap> targetTileMap;
    TileMap tiles;
};

class CanvasUndoLayers : public CanvasUndoEvent
{
public:
    CanvasUndoLayers(CanvasStack *stack, int activeLayer);
    ~CanvasUndoLayers();
    TileSet apply(CanvasStack *stack,
                  int         *activeLayer,
                  CanvasLayer *quickmask,
                  QRect       *activeFrame,
                  QRect       *inactiveFrame);

    int currentLayer;
    QList<CanvasLayer *> layers;
};

class CanvasUndoBackground : public CanvasUndoEvent
{
public:
    CanvasUndoBackground(CanvasStack *stack);
    TileSet apply(CanvasStack *stack,
                  int         *activeLayer,
                  CanvasLayer *quickmask,
                  QRect       *activeFrame,
                  QRect       *inactiveFrame);
    bool modifiesBackground();

    std::unique_ptr<CanvasTile> tile;
};

class CanvasUndoQuickMask : public CanvasUndoEvent
{
public:
    CanvasUndoQuickMask(bool visible);
    TileSet apply(CanvasStack *stack,
                  int         *activeLayer,
                  CanvasLayer *quickmask,
                  QRect       *activeFrame,
                  QRect       *inactiveFrame);

    bool visible;
};

class CanvasUndoFrame : public CanvasUndoEvent
{
public:
    CanvasUndoFrame(QRect const &activeFrame, QRect const &inactiveFrame);
    TileSet apply(CanvasStack *stack,
                  int         *activeLayer,
                  CanvasLayer *quickmask,
                  QRect       *activeFrame,
                  QRect       *inactiveFrame);

    QRect active;
    QRect inactive;
};

#endif // CANVASUNDOEVENT_H
