#include "canvaslayer.h"
#include "canvascontext.h"

CanvasLayer::CanvasLayer()
{
}

CanvasLayer::~CanvasLayer()
{
    std::map<uint64_t, CanvasTile *>::iterator iter;

    for (iter = tiles.begin(); iter != tiles.end(); ++iter)
    {
        delete iter->second;
    }
}

CanvasTile *CanvasLayer::getTileMaybe(int x, int y)
{
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;

    std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

    if (found != tiles.end())
        return found->second;
    else
        return NULL;
}

CanvasTile *CanvasLayer::getTile(int x, int y)
{
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;

    std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

    if (found != tiles.end())
        return found->second;

    CanvasTile *tile = new CanvasTile(x, y);

    tile->fill(1.0f, 1.0f, 1.0f, 1.0f);

    tiles[key] = tile;

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
