#include "canvaslayer.h"
#include "canvascontext.h"

CanvasLayer::CanvasLayer()
{
}

CanvasLayer::~CanvasLayer()
{
    TileMap::iterator iter;

    for (iter = tiles.begin(); iter != tiles.end(); ++iter)
    {
        delete iter->second;
    }
}

TileSet CanvasLayer::getTileSet()
{
    TileMap::iterator iter;
    TileSet result;

    for (iter = tiles.begin(); iter != tiles.end(); ++iter)
    {
        result.insert(iter->first);
    }

    return result;
}

CanvasTile *CanvasLayer::getTileMaybe(int x, int y)
{
    TileMap::iterator found = tiles.find(QPoint(x, y));

    if (found != tiles.end())
        return found->second;
    else
        return NULL;
}

CanvasTile *CanvasLayer::getTile(int x, int y)
{
    TileMap::iterator found = tiles.find(QPoint(x, y));

    if (found != tiles.end())
        return found->second;

    CanvasTile *tile = new CanvasTile(x, y);

    tile->fill(0.0f, 0.0f, 0.0f, 0.0f);

    tiles[QPoint(x, y)] = tile;

    return tile;
}

cl_mem CanvasLayer::clOpenTileAt(int x, int y)
{
    CanvasTile *tile = getTile(x, y);

    tile->unmapHost();

    return tile->tileMem;
}

float *CanvasLayer::openTileAt(int x, int y)
{
    CanvasTile *tile = getTile(x, y);

    tile->mapHost();

    return tile->tileData;
}
