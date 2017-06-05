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

TileSet CanvasUndoTiles::apply(CanvasStack *stack,
                               int         *activeLayer,
                               CanvasLayer *quickmask,
                               QRect       *activeFrame,
                               QRect       *inactiveFrame)
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
    for (auto const &layer: stack->layers)
        layers.push_back(new CanvasLayer(*layer));
    currentLayer = activeLayer;
}

CanvasUndoLayers::~CanvasUndoLayers()
{
    for (auto const &layer: layers)
        delete layer;
    layers.clear();
}

TileSet CanvasUndoLayers::apply(CanvasStack *stack,
                                int         *activeLayer,
                                CanvasLayer *quickmask,
                                QRect       *activeFrame,
                                QRect       *inactiveFrame)
{
    TileSet oldStackTiles = stack->getTileSet();

    std::swap(layers, stack->layers);
    std::swap(*activeLayer, currentLayer);

    TileSet modifiedTiles = stack->getTileSet();
    modifiedTiles.insert(oldStackTiles.begin(), oldStackTiles.end());

    return modifiedTiles;
}

CanvasUndoBackground::CanvasUndoBackground(CanvasStack *stack)
{
    tile = stack->backgroundTileCL->copy();
}

TileSet CanvasUndoBackground::apply(CanvasStack *stack,
                                    int         *activeLayer,
                                    CanvasLayer *quickmask,
                                    QRect       *activeFrame,
                                    QRect       *inactiveFrame)
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

CanvasUndoQuickMask::CanvasUndoQuickMask(bool visible)
    : visible(visible)
{
}

TileSet CanvasUndoQuickMask::apply(CanvasStack *stack,
                                   int         *activeLayer,
                                   CanvasLayer *quickmask,
                                   QRect       *activeFrame,
                                   QRect       *inactiveFrame)
{
    quickmask->visible = visible;
    visible = !visible;

    return quickmask->getTileSet();
}

CanvasUndoFrame::CanvasUndoFrame(const QRect &activeFrame, const QRect &inactiveFrame)
    : active(activeFrame),
      inactive(inactiveFrame)
{
}

TileSet CanvasUndoFrame::apply(CanvasStack *stack,
                               int         *activeLayer,
                               CanvasLayer *quickmask,
                               QRect       *activeFrame,
                               QRect       *inactiveFrame)
{
    std::swap(active, *activeFrame);
    std::swap(inactive, *inactiveFrame);

    return TileSet();
}
