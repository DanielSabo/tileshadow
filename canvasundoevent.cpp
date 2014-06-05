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

TileSet CanvasUndoTiles::apply(CanvasStack *stack)
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

    // FIXME: This event is now invalid, using it again would be an error
    tiles.clear();

    return modifiedTiles;
}
