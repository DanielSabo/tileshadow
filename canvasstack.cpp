#include "canvasstack.h"
#include "canvastile.h"
#include "canvascontext.h"

CanvasStack::CanvasStack()
{
    backgroundTile = new CanvasTile(0, 0);
    backgroundTile->fill(1.0f, 1.0f, 1.0f, 1.0f);
    backgroundTileCL = backgroundTile->copy();
    backgroundTileCL->unmapHost();
}

CanvasStack::~CanvasStack()
{
    delete backgroundTile;
    delete backgroundTileCL;

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
    for (QList<CanvasLayer *>::iterator iter = layers.begin(); iter != layers.end(); ++iter)
        delete *iter;
    layers.clear();

    for (TileMap::iterator iter = tiles.begin(); iter != tiles.end(); ++iter)
        delete iter->second;
    tiles.clear();
}

void cpuBlendInPlace(CanvasTile *inTile, CanvasTile *auxTile)
{
    const int numPixels = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT;

    float *in  = inTile->mapHost();
    float *aux = auxTile->mapHost();
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

void clBlendInPlace(CanvasTile *inTile, CanvasTile *auxTile, BlendMode::Mode mode)
{
    const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
    cl_mem inMem  = inTile->unmapHost();
    cl_mem auxMem = auxTile->unmapHost();

    cl_kernel kernel;

    switch (mode) {
    case BlendMode::Multiply:
        kernel = SharedOpenCL::getSharedOpenCL()->blendKernel_multiply;
        break;
    case BlendMode::ColorDodge:
        kernel = SharedOpenCL::getSharedOpenCL()->blendKernel_colorDodge;
        break;
    case BlendMode::ColorBurn:
        kernel = SharedOpenCL::getSharedOpenCL()->blendKernel_colorBurn;
        break;
    case BlendMode::Screen:
        kernel = SharedOpenCL::getSharedOpenCL()->blendKernel_screen;
        break;
    default:
        kernel = SharedOpenCL::getSharedOpenCL()->blendKernel_over;
        break;
    }

    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&inMem);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&inMem);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&auxMem);
    clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                           kernel,
                           1, NULL, global_work_size, NULL,
                           0, NULL, NULL);
}

CanvasTile *CanvasStack::getTileMaybe(int x, int y, bool cache)
{
    int layerCount = layers.size();

    CanvasTile *result = NULL;

    if (layerCount == 0)
        result = NULL;
    else if (layerCount == 1)
    {
        CanvasTile *auxTile = NULL;
        CanvasLayer *layer = layers.at(0);

        if (layer->visible)
            auxTile = layer->getTileMaybe(x, y);

        if (auxTile)
        {
            result = backgroundTileCL->copy();
            clBlendInPlace(result, auxTile, layer->mode);
        }
        else
            result = NULL;
    }
    else
    {
        CanvasTile *inTile = NULL;

        for (int currentLayer = 0; currentLayer < layerCount; currentLayer++)
        {
            CanvasTile *auxTile = NULL;
            CanvasLayer *layer = layers.at(currentLayer);

            if (layer->visible)
                auxTile = layer->getTileMaybe(x, y);

            if (auxTile)
            {
                if (!inTile)
                    inTile = backgroundTileCL->copy();
                clBlendInPlace(inTile, auxTile, layer->mode);
            }
        }

        result = inTile;
    }

    if (result)
    {
        result->x = x;
        result->y = y;
    }

    if (result && cache)
    {
        TileMap::iterator found = tiles.find(QPoint(x, y));

        if (found != tiles.end())
        {
            delete found->second;
            found->second = result;
        }
        else
        {
            tiles[QPoint(x, y)] = result;
        }
    }

    return result;
}

TileSet CanvasStack::getTileSet() const
{
    QList<CanvasLayer *>::const_iterator layersIter;
    TileSet result;

    for (layersIter = layers.begin(); layersIter != layers.end(); layersIter++)
    {
        TileSet layerSet = (*layersIter)->getTileSet();
        result.insert(layerSet.begin(), layerSet.end());
    }

    return result;
}

float *CanvasStack::openTileAt(int x, int y)
{
    CanvasTile *result = getTileMaybe(x, y);

    if (!result)
        result = backgroundTile;

    return result->mapHost();
}

cl_mem CanvasStack::clOpenTileAt(int x, int y)
{
    CanvasTile *result = getTileMaybe(x, y);

    if (!result)
        result = backgroundTileCL;

    return result->unmapHost();
}
