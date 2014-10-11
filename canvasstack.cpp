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

void CanvasStack::newLayerAt(int index, QString name)
{
    layers.insert(index, new CanvasLayer(name));
}

void CanvasStack::removeLayerAt(int index)
{
    if (index < 0 || index > layers.size())
        return;
    if (layers.size() == 1)
        return;
    delete layers.takeAt(index);
}

void CanvasStack::clearLayers()
{
    for (CanvasLayer *layer: layers)
        delete layer;
    layers.clear();
}

std::unique_ptr<CanvasTile> CanvasStack::getTileMaybe(int x, int y) const
{
    std::unique_ptr<CanvasTile> result(nullptr);

    for (CanvasLayer *layer: layers)
    {
        CanvasTile *auxTile = nullptr;

        if (layer->visible && layer->opacity > 0.0f)
            auxTile = layer->getTileMaybe(x, y);

        if (auxTile)
        {
            if (!result)
                result = backgroundTileCL->copy();
            auxTile->blendOnto(result.get(), layer->mode, layer->opacity);
        }
    }

    return result;
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
