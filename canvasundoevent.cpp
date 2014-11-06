#include "canvasundoevent.h"
#include "canvastile.h"
#include <algorithm>

CanvasUndoEvent::CanvasUndoEvent()
{
}

CanvasUndoEvent::~CanvasUndoEvent()
{

}

bool CanvasUndoEvent::modifiesBackground()
{
    return false;
}

CanvasUndoTiles::CanvasUndoTiles()
{

}

CanvasUndoTiles::~CanvasUndoTiles()
{
    tiles.clear();
}

TileSet CanvasUndoTiles::apply(CanvasStack *stack, int *activeLayer)
{
    TileSet modifiedTiles;

    for (TileMap::iterator iter = tiles.begin(); iter != tiles.end(); ++iter)
    {
        std::unique_ptr<CanvasTile> &target = (*targetTileMap)[iter->first];

        if (target)
            target->swapHost();

        std::swap(target, iter->second);

        if (!target)
            targetTileMap->erase(iter->first);

        modifiedTiles.insert(iter->first);
    }

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

CanvasUndoBackground::CanvasUndoBackground(CanvasStack *stack)
{
    tile = stack->backgroundTileCL->copy();
}

TileSet CanvasUndoBackground::apply(CanvasStack *stack, int *activeLayer)
{
    std::unique_ptr<CanvasTile> oldBackground = stack->backgroundTileCL->copy();
    stack->setBackground(std::move(tile));
    tile = std::move(oldBackground);

    return TileSet();
}

bool CanvasUndoBackground::modifiesBackground()
{
    return true;
}
