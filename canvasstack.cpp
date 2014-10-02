#include "canvasstack.h"
#include "canvastile.h"
#include "canvascontext.h"

CanvasStack::CanvasStack()
{
    backgroundTile.reset(new CanvasTile());
    backgroundTile->fill(1.0f, 1.0f, 1.0f, 1.0f);
    backgroundTileCL = backgroundTile->copy();
    backgroundTileCL->unmapHost();
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
    int layerCount = layers.size();

    std::unique_ptr<CanvasTile> result(nullptr);

    if (layerCount == 0)
        result = nullptr;
    else if (layerCount == 1)
    {
        CanvasTile *auxTile = nullptr;
        CanvasLayer *layer = layers.at(0);

        if (layer->visible)
            auxTile = layer->getTileMaybe(x, y);

        if (auxTile)
        {
            result = backgroundTileCL->copy();
            auxTile->blendOnto(result.get(), layer->mode);
        }
        else
            result = nullptr;
    }
    else
    {
        for (CanvasLayer *layer: layers)
        {
            CanvasTile *auxTile = nullptr;

            if (layer->visible)
                auxTile = layer->getTileMaybe(x, y);

            if (auxTile)
            {
                if (!result)
                    result = backgroundTileCL->copy();
                auxTile->blendOnto(result.get(), layer->mode);
            }
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
