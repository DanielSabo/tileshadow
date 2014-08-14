#include "canvaslayer.h"
#include "canvastile.h"
#include "canvascontext.h"
#include <QDebug>

CanvasLayer::CanvasLayer(QString name)
    : name(name),
      visible(true),
      mode(BlendMode::Over),
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
    result->mode = mode;

    for (TileMap::iterator iter = tiles->begin(); iter != tiles->end(); ++iter)
    {
        (*result->tiles)[iter->first] = iter->second->copy();
    }

    return result;
}

void CanvasLayer::prune()
{
    /* Search the layer for empty tiles and delete them */

    for (TileMap::iterator iter = tiles->begin(); iter != tiles->end(); )
    {
        float *data = iter->second->mapHost();
        bool empty = true;

        for (int i = 0; i < TILE_COMP_TOTAL && empty; ++i)
            if (data[i] > 0.000001 || data[i] < -0.000001)
                empty = false;

        if (empty)
        {
            delete iter->second;
            tiles->erase(iter++);
        }
        else
            ++iter;
    }
}

void CanvasLayer::swapOut()
{
    for (auto iter: *tiles)
        iter.second->swapHost();
}

static void subrectCopy(cl_mem src, int srcX, int srcY, cl_mem dst, int dstX, int dstY)
{
    static const size_t PIXEL_SIZE = sizeof(float) * 4;
    size_t width = std::min(TILE_PIXEL_WIDTH - srcX, TILE_PIXEL_WIDTH - dstX);
    size_t height = std::min(TILE_PIXEL_HEIGHT - srcY, TILE_PIXEL_HEIGHT - dstY);
    size_t stride = TILE_PIXEL_WIDTH * PIXEL_SIZE;

    size_t copySrcXYZ[3] = CL_DIM3(srcX * PIXEL_SIZE, srcY, 0);
    size_t copyDstXYZ[3] = CL_DIM3(dstX * PIXEL_SIZE, dstY, 0);
    size_t copyRegion[3] = CL_DIM3(width * PIXEL_SIZE, height, 1);

    clEnqueueCopyBufferRect(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                            src, dst,
                            copySrcXYZ, copyDstXYZ, copyRegion,
                            stride,
                            0,
                            stride,
                            0,
                            0, NULL, NULL);
}

CanvasLayer *CanvasLayer::translated(int x, int y) const
{
    CanvasLayer *result = new CanvasLayer();
    result->name = name;
    result->visible = visible;
    result->mode = mode;

    for (TileMap::iterator iter = tiles->begin(); iter != tiles->end(); ++iter)
    {
        // Find the new origin of this tile
        int dstOriginX = iter->first.x() * TILE_PIXEL_WIDTH + x;
        int dstOriginY = iter->first.y() * TILE_PIXEL_HEIGHT + y;

        // For testing, just round to the nearest tile
        QPoint dstOrigin(tile_indice(dstOriginX, TILE_PIXEL_WIDTH),
                         tile_indice(dstOriginY, TILE_PIXEL_HEIGHT));

        int subShiftX = dstOriginX - dstOrigin.x() * TILE_PIXEL_WIDTH;
        int subShiftY = dstOriginY - dstOrigin.y() * TILE_PIXEL_HEIGHT;

        cl_mem originTileMem = iter->second->unmapHost();
        subrectCopy(originTileMem, 0, 0,
                    result->clOpenTileAt(dstOrigin.x(), dstOrigin.y()),
                    subShiftX, subShiftY);

        if (subShiftX)
        {
            subrectCopy(originTileMem, TILE_PIXEL_WIDTH - subShiftX, 0,
                        result->clOpenTileAt(dstOrigin.x() + 1, dstOrigin.y()),
                        0, subShiftY);
        }
        if (subShiftY)
        {
            subrectCopy(originTileMem, 0, TILE_PIXEL_HEIGHT - subShiftY,
                        result->clOpenTileAt(dstOrigin.x(), dstOrigin.y() + 1),
                        subShiftX, 0);
        }
        if (subShiftX && subShiftY)
        {
            subrectCopy(originTileMem,
                        TILE_PIXEL_WIDTH - subShiftX,
                        TILE_PIXEL_HEIGHT - subShiftY,
                        result->clOpenTileAt(dstOrigin.x() + 1, dstOrigin.y() + 1),
                        0, 0);
        }
    }

    return result;
}

TileSet CanvasLayer::takeTiles(CanvasLayer *source)
{
    TileMap::iterator iter;
    TileSet result;

    for (TileMap::iterator iter = tiles->begin(); iter != tiles->end(); ++iter)
    {
        result.insert(iter->first);
        delete iter->second;
    }
    tiles->clear();

    for (iter = source->tiles->begin(); iter != source->tiles->end(); ++iter)
    {
        result.insert(iter->first);
        (*tiles)[iter->first] = iter->second;
    }
    source->tiles->clear();

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

CanvasTile *CanvasLayer::takeTileMaybe(int x, int y)
{
    TileMap::iterator found = tiles->find(QPoint(x, y));

    if (found != tiles->end())
    {
        CanvasTile *result = found->second;
        tiles->erase(found);
        return result;
    }
    else
        return NULL;
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

    CanvasTile *tile = new CanvasTile();

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
