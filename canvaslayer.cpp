#include "canvaslayer.h"
#include "canvastile.h"
#include "canvascontext.h"

CanvasLayer::CanvasLayer(QString name)
    : name(name),
      visible(true),
      tiles(new TileMap, _deleteTileMap)
{
}

CanvasLayer::~CanvasLayer()
{
}

CanvasLayer *CanvasLayer::deepCopy() const
{
    CanvasLayer *result = new CanvasLayer();
    result->name = name;
    result->visible = visible;

    for (TileMap::iterator iter = tiles->begin(); iter != tiles->end(); ++iter)
    {
        (*result->tiles)[iter->first] = iter->second->copy();
    }

    return result;
}

TileSet CanvasLayer::getTileSet() const
{
    TileMap::iterator iter;
    TileSet result;

    for (iter = tiles->begin(); iter != tiles->end(); ++iter)
    {
        result.insert(iter->first);
    }

    return result;
}

CanvasTile *CanvasLayer::getTileMaybe(int x, int y) const
{
    TileMap::iterator found = tiles->find(QPoint(x, y));

    if (found != tiles->end())
        return found->second;
    else
        return NULL;
}

CanvasTile *CanvasLayer::getTile(int x, int y)
{
    TileMap::iterator found = tiles->find(QPoint(x, y));

    if (found != tiles->end())
        return found->second;

    CanvasTile *tile = new CanvasTile(x, y);

    tile->fill(0.0f, 0.0f, 0.0f, 0.0f);

    (*tiles)[QPoint(x, y)] = tile;

    return tile;
}

cl_mem CanvasLayer::clOpenTileAt(int x, int y)
{
    CanvasTile *tile = getTile(x, y);

    return tile->unmapHost();
}

float *CanvasLayer::openTileAt(int x, int y)
{
    CanvasTile *tile = getTile(x, y);

    return tile->mapHost();
}
