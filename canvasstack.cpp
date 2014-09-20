#include "canvasstack.h"
#include "canvastile.h"
#include "canvascontext.h"

CanvasStack::CanvasStack()
{
    backgroundTile.reset(new CanvasTile());
    backgroundTile->fill(1.0f, 1.0f, 1.0f, 1.0f);
    backgroundTileCL.reset(backgroundTile->copy());
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
                           1, nullptr, global_work_size, nullptr,
                           0, nullptr, nullptr);
}

std::unique_ptr<CanvasTile> CanvasStack::getTileMaybe(int x, int y) const
{
    int layerCount = layers.size();

    CanvasTile *result = nullptr;

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
            clBlendInPlace(result, auxTile, layer->mode);
        }
        else
            result = nullptr;
    }
    else
    {
        CanvasTile *inTile = nullptr;

        for (CanvasLayer *layer: layers)
        {
            CanvasTile *auxTile = nullptr;

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

    return std::unique_ptr<CanvasTile>(result);
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
