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

void cpuBlendInPlace(CanvasTile *inTile, CanvasTile *auxTile)
{
    inTile->mapHost();
    auxTile->mapHost();

    const int numPixels = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT;

    float *in  = inTile->tileData;
    float *aux = auxTile->tileData;
    float *out = in;

    for (int i = 0; i < numPixels; ++i)
    {
      float alpha = aux[3];
      float dst_alpha = in[3];

      float a = alpha + dst_alpha * (1.0f - alpha);
      float a_term = dst_alpha * (1.0f - alpha);
      float r = aux[0] * alpha + in[0] * a_term;
      float g = aux[1] * alpha + in[1] * a_term;
      float b = aux[2] * alpha + in[2] * a_term;

      out[0] = r / a;
      out[1] = g / a;
      out[2] = b / a;
      out[3] = a;

      in  += 4;
      aux += 4;
      out += 4;
    }
}

float *CanvasStack::openTileAt(int x, int y)
{
    int layerCount = layers.size();

    CanvasTile *result = NULL;

    if (layerCount == 0)
        result = NULL;
    else if (layerCount == 1)
    {
        CanvasTile *auxTile = layers.at(0)->getTileMaybe(x, y);

        if (auxTile)
        {
            result = backgroundTile->copy();
            cpuBlendInPlace(result, auxTile);
        }
        else
            result = NULL;
    }
    else
    {
        CanvasTile *inTile = NULL;

        for (int currentLayer = 0; currentLayer < layerCount; currentLayer++)
        {
            CanvasTile *auxTile = layers.at(currentLayer)->getTileMaybe(x, y);
            if (auxTile)
            {
                if (!inTile)
                    inTile = backgroundTile->copy();
                cpuBlendInPlace(inTile, auxTile);
            }
        }

        result = inTile;
    }

    if (!result)
        result = backgroundTile;
    else
    {
        uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;
        std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

        if (found != tiles.end())
        {
            delete found->second;
            found->second = result;
        }
        else
        {
            tiles[key] = result;
        }
    }

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
