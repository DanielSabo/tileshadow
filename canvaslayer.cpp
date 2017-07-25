#include "canvaslayer.h"
#include "canvasstack.h"
#include "canvastile.h"
#include <QDebug>
#include <utility>

CanvasLayer::CanvasLayer(QString name)
    : name(name),
      visible(true),
      editable(true),
      mode(BlendMode::Over),
      opacity(1.0f),
      type(LayerType::Layer),
      tiles(new TileMap)
{
}

CanvasLayer::CanvasLayer(CanvasLayer const& from)
    : name(from.name),
      visible(from.visible),
      editable(from.editable),
      mode(from.mode),
      opacity(from.opacity),
      type(from.type),
      tiles(from.tiles)
{
    for (CanvasLayer const *child: from.children)
        children.push_back(new CanvasLayer(*child));
}

CanvasLayer::~CanvasLayer()
{
    while (!children.isEmpty())
        delete children.takeLast();
}

std::unique_ptr<CanvasLayer> CanvasLayer::shellCopy() const
{
    std::unique_ptr<CanvasLayer> result(new CanvasLayer());
    result->name = name;
    result->visible = visible;
    result->editable = editable;
    result->mode = mode;
    result->opacity = opacity;
    result->type = type;

    return result;
}

std::unique_ptr<CanvasLayer> CanvasLayer::deepCopy() const
{
    std::unique_ptr<CanvasLayer> result = shellCopy();

    for (TileMap::iterator iter = tiles->begin(); iter != tiles->end(); ++iter)
    {
        (*result->tiles)[iter->first] = iter->second->copy();
    }

    for (CanvasLayer *child: children)
        result->children.append(child->deepCopy().release());

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
            tiles->erase(iter++);
        else
            ++iter;
    }

    for (CanvasLayer *child: children)
        child->prune();
}

void CanvasLayer::swapOut()
{
    for (auto &iter: *tiles)
        iter.second->swapHost();

    for (CanvasLayer *child: children)
        child->swapOut();
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
                            0, nullptr, nullptr);
}

std::unique_ptr<CanvasLayer> CanvasLayer::translated(int x, int y) const
{
    std::unique_ptr<CanvasLayer> result = shellCopy();

    if (type == LayerType::Group)
        for (auto const &child: children)
            result->children.append(child->translated(x, y).release());

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

std::unique_ptr<CanvasLayer> CanvasLayer::mergeDown(const CanvasLayer *target) const
{
    if (type != LayerType::Layer || target->type != LayerType::Layer)
        return std::unique_ptr<CanvasLayer>(new CanvasLayer());

    std::unique_ptr<CanvasLayer> result;
    if (BlendMode::isMasking(mode))
    {
        result = target->shellCopy();

        // For masking modes clip the result tiles to this layer's tiles
        for (auto &iter: *tiles)
        {
            CanvasTile *dst = target->getTileMaybe(iter.first.x(), iter.first.y());
            if (dst)
                result->tiles->emplace(iter.first, dst->copy());
        }
    }
    else
    {
        result = target->deepCopy();
    }

    for (auto &iter: *tiles)
    {
        CanvasTile *src = result->getTile(iter.first.x(), iter.first.y());
        CanvasTile *aux = iter.second.get();
        aux->blendOnto(src, mode, opacity);
    }

    return result;
}

std::unique_ptr<CanvasLayer> CanvasLayer::flattened() const
{
    if (type == LayerType::Group)
    {
        std::unique_ptr<CanvasLayer> result = shellCopy();
        result->type = LayerType::Layer;

        for (auto const &idx: getTileSet())
        {
            auto tile = renderList(children, idx.x(), idx.y());
            if (tile)
                (*result->tiles)[idx] = std::move(tile);
        }

        return result;
    }
    else
    {
        return deepCopy();
    }
}

TileSet CanvasLayer::takeTiles(CanvasLayer *source)
{
    TileSet result;

    tiles->clear();
    std::swap(*tiles, *(source->tiles));

    for (auto const &iter: *tiles)
        result.insert(iter.first);

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

    if (!children.empty())
    {
        for (auto const &child: children)
        {
            TileSet layerSet = child->getTileSet();
            result.insert(layerSet.begin(), layerSet.end());
        }
    }

    return result;
}

std::unique_ptr<CanvasTile> CanvasLayer::takeTileMaybe(int x, int y)
{
    TileMap::iterator found = tiles->find(QPoint(x, y));

    if (found != tiles->end())
    {
        std::unique_ptr<CanvasTile> result = std::move(found->second);
        tiles->erase(found);
        return result;
    }
    else
        return nullptr;
}

CanvasTile *CanvasLayer::getTileMaybe(int x, int y) const
{
    TileMap::iterator found = tiles->find(QPoint(x, y));

    if (found != tiles->end())
        return found->second.get();
    else
        return nullptr;
}

CanvasTile *CanvasLayer::getTile(int x, int y)
{
    std::unique_ptr<CanvasTile> &tile = (*tiles)[QPoint(x, y)];

    if (!tile)
    {
        tile.reset(new CanvasTile());
        tile->fill(0.0f, 0.0f, 0.0f, 0.0f);
    }

    return tile.get();
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
