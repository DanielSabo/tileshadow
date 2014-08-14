#include "canvasundoevent.h"
#include "canvastile.h"
#include <algorithm>

CanvasUndoEvent::CanvasUndoEvent()
{
}

CanvasUndoEvent::~CanvasUndoEvent()
{

}

CanvasUndoTiles::CanvasUndoTiles()
{

}

CanvasUndoTiles::~CanvasUndoTiles()
{
    for (TileMap::iterator iter = tiles.begin(); iter != tiles.end(); ++iter)
        delete iter->second;
    tiles.clear();
}

TileSet CanvasUndoTiles::apply(CanvasStack *stack, int *activeLayer)
{
    TileSet modifiedTiles;
    TileMap redoTiles;

    for (TileMap::iterator iter = tiles.begin(); iter != tiles.end(); ++iter)
    {
        CanvasTile *redoTile = (*targetTileMap)[iter->first];

        if (redoTile)
            redoTile->swapHost();

        redoTiles[iter->first] = redoTile;

        if (iter->second)
            (*targetTileMap)[iter->first] = iter->second;
        else
            targetTileMap->erase(iter->first);

        modifiedTiles.insert(iter->first);
    }

    tiles = redoTiles;

    std::swap(*activeLayer, currentLayer);
    return modifiedTiles;
}

CanvasUndoLayers::CanvasUndoLayers(CanvasStack *stack, int activeLayer)
{
    for (QList<CanvasLayer *>::iterator iter = stack->layers.begin(); iter != stack->layers.end(); ++iter)
        layers.push_back(new CanvasLayer(**iter));
    currentLayer = activeLayer;
}

CanvasUndoLayers::~CanvasUndoLayers()
{
    for (QList<CanvasLayer *>::iterator iter = layers.begin(); iter != layers.end(); ++iter)
        delete *iter;
    layers.clear();
}

TileSet CanvasUndoLayers::apply(CanvasStack *stack, int *activeLayer)
{
    QList<CanvasLayer *> redoLayers;
    TileSet oldStackTiles = stack->getTileSet();

    for (QList<CanvasLayer *>::iterator iter = stack->layers.begin(); iter != stack->layers.end(); ++iter)
        redoLayers.push_back(new CanvasLayer(**iter));

    stack->clearLayers();

    for (QList<CanvasLayer *>::iterator iter = layers.begin(); iter != layers.end(); ++iter)
        stack->layers.push_back(*iter);

    TileSet modifiedTiles = stack->getTileSet();
    modifiedTiles.insert(oldStackTiles.begin(), oldStackTiles.end());

    layers = redoLayers;

    std::swap(*activeLayer, currentLayer);
    return modifiedTiles;
}
