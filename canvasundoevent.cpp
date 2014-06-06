#include "canvasundoevent.h"
#include "canvastile.h"

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

    for (TileMap::iterator iter = tiles.begin(); iter != tiles.end(); ++iter)
    {
        delete (*targetTileMap)[iter->first];
        if (iter->second)
        {
            (*targetTileMap)[iter->first] = iter->second;
        }
        else
        {
            targetTileMap->erase(iter->first);
        }

        modifiedTiles.insert(iter->first);
    }

    *activeLayer = currentLayer;

    // FIXME: This event is now invalid, using it again would be an error
    tiles.clear();

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
    TileSet oldStackTiles = stack->getTileSet();
    stack->clearLayers();

    for (QList<CanvasLayer *>::iterator iter = layers.begin(); iter != layers.end(); ++iter)
        stack->layers.push_back(*iter);

    TileSet modifiedTiles = stack->getTileSet();
    modifiedTiles.insert(oldStackTiles.begin(), oldStackTiles.end());

    // FIXME: This event is now invalid, using it again would be an error
    layers.clear();

    *activeLayer = currentLayer;
    return modifiedTiles;
}
