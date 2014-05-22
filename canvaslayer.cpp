#include "canvaslayer.h"
#include "canvascontext.h"

CanvasLayer::CanvasLayer()
{
}

CanvasLayer::~CanvasLayer()
{
    std::map<uint64_t, CanvasTile *>::iterator iter;

    for (iter = tiles.begin(); iter != tiles.end(); ++iter)
    {
        delete iter->second;
    }
}

CanvasTile *CanvasLayer::getTile(int x, int y)
{
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;

    std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

    if (found != tiles.end())
        return found->second;

    CanvasTile *tile = new CanvasTile(x, y);

    tile->unmapHost();

    cl_int err = CL_SUCCESS;
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->fillKernel;
    const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
    float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&tile->tileMem);
    err = clSetKernelArg(kernel, 1, sizeof(cl_float4), (void *)&pixel);
    err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                 kernel, 1,
                                 NULL, global_work_size, NULL,
                                 0, NULL, NULL);

    tiles[key] = tile;

    return tile;
}

cl_mem CanvasLayer::clOpenTileAt(int x, int y)
{
    CanvasTile *tile = getTile(x, y);

    tile->unmapHost();

    return tile->tileMem;
}

float *CanvasLayer::openTileAt(int x, int y)
{
    CanvasTile *tile = getTile(x, y);

    tile->mapHost();

    return tile->tileData;
}
