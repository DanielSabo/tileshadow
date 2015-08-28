#include "canvasstack.h"
#include "canvastile.h"
#include "canvascontext.h"

CanvasStack::CanvasStack()
{
    std::unique_ptr<CanvasTile> newBackground(new CanvasTile());
    newBackground->fill(1.0f, 1.0f, 1.0f, 1.0f);

    setBackground(std::move(newBackground));
}

CanvasStack::~CanvasStack()
{
    clearLayers();
}

void CanvasStack::clearLayers()
{
    for (CanvasLayer *layer: layers)
        delete layer;
    layers.clear();
}

namespace {
std::unique_ptr<CanvasTile> renderList(QList<CanvasLayer *> const &children, int x, int y, CanvasTile *background)
{
    std::unique_ptr<CanvasTile> result(nullptr);

    for (CanvasLayer *layer: children)
    {
        std::unique_ptr<CanvasTile> childRender = nullptr;
        CanvasTile *auxTile = nullptr;

        if (layer->visible && layer->opacity > 0.0f)
        {
            if (layer->type == LayerType::Layer)
            {
                auxTile = layer->getTileMaybe(x, y);
            }
            else if (layer->type == LayerType::Group)
            {
                childRender = renderList(layer->children, x, y, nullptr);
                auxTile = childRender.get();
            }
        }

        if (auxTile)
        {
            if (!result)
            {
                if (background)
                {
                    result = background->copy();
                }
                else
                {
                    result.reset(new CanvasTile());
                    result->fill(0, 0, 0, 0);
                }
            }
            auxTile->blendOnto(result.get(), layer->mode, layer->opacity);
        }
    }

    return result;
}
}

std::unique_ptr<CanvasTile> CanvasStack::getTileMaybe(int x, int y) const
{
    return renderList(layers, x, y, backgroundTileCL.get());
}

TileSet CanvasStack::getTileSet() const
{
    TileSet result;

    for (CanvasLayer *layer: layers)
    {
        TileSet layerSet = layer->getTileSet();
        result.insert(layerSet.begin(), layerSet.end());
    }

    return result;
}

void CanvasStack::setBackground(std::unique_ptr<CanvasTile> newBackground)
{
    backgroundTileCL = newBackground->copy();
    backgroundTileCL->unmapHost();
    backgroundTile = std::move(newBackground);
}
