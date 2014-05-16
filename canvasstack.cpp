#include "canvasstack.h"
#include "canvascontext.h"

CanvasStack::CanvasStack()
{
    backgroundTile = new CanvasTile(0, 0);
    backgroundTile->fill(1.0f, 1.0f, 1.0f, 1.0f);
    backgroundTileCL = backgroundTile->copy();
    backgroundTileCL->unmapHost();
}

void CanvasStack::newLayerAt(int index)
{
    layers.insert(index, new CanvasLayer());
}

float *CanvasStack::openTileAt(int x, int y)
{
    int layerCount = layers.size();

    CanvasTile *result = NULL;

    if (layerCount == 1)
        result = layers.at(0)->getTileMaybe(x, y);

    if (!result)
        result = backgroundTile;

    result->mapHost();
    return result->tileData;
}

cl_mem CanvasStack::clOpenTileAt(int x, int y)
{
    int layerCount = layers.size();

    CanvasTile *result = NULL;

    if (layerCount == 1)
        result = layers.at(0)->getTileMaybe(x, y);

    if (!result)
        result = backgroundTileCL;

    result->unmapHost();
    return result->tileMem;
}

/* Render tile */
/* For each layer, from the bottom (0) */
/* if layers[0] != NULL out = layers[0].dup() */
/* If aux == NULL, out = in */
/* If aux != NULL and in == NULL, in = backgroundTile.dup(), out = blend(in, aux) */
/* If aux != NULL and in != NULL, out = blend(in, aux) */
