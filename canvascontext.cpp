#include "canvascontext.h"

CanvasTile::CanvasTile()
{
    isOpen = false;
    tileMem = 0;
    tileBuffer = 0;
    tileData = NULL;
    tileMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                              TILE_COMP_TOTAL * sizeof(float), tileData, NULL);
}

CanvasTile::~CanvasTile(void)
{
    unmapHost();
    clReleaseMemObject(tileMem);
}

void CanvasTile::mapHost(void)
{
    if (!tileData)
    {
        cl_int err = CL_SUCCESS;
        tileData = (float *)clEnqueueMapBuffer (SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem,
                                                      CL_TRUE, CL_MAP_READ | CL_MAP_WRITE,
                                                      0, TILE_COMP_TOTAL * sizeof(float),
                                                      0, NULL, NULL, &err);
    }
}

void CanvasTile::unmapHost()
{
    if (tileData)
    {
        clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, NULL, NULL);
        tileData = NULL;
    }
}

CanvasTile *CanvasContext::getTile(int x, int y)
{
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;

    std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

    if (found != tiles.end())
        return found->second;

    CanvasTile *tile = new CanvasTile();

    cl_int err = CL_SUCCESS;
    cl_mem data = clOpenTile(tile);
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->fillKernel;
    const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
    float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
    err = clSetKernelArg(kernel, 1, sizeof(cl_float4), (void *)&pixel);
    err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                 kernel, 1,
                                 NULL, global_work_size, NULL,
                                 0, NULL, NULL);


    glFuncs->glGenBuffers(1, &tile->tileBuffer);
    glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, tile->tileBuffer);
    glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                          sizeof(float) * TILE_COMP_TOTAL,
                          NULL,
                          GL_DYNAMIC_DRAW);

    clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);

    closeTile(tile);

    // cout << "Added tile at " << x << "," << y << endl;
    tiles[key] = tile;

    return tile;
}

cl_mem CanvasContext::clOpenTile(CanvasTile *tile)
{
    if (!tile->isOpen)
    {
        openTiles.push_front(tile);
    }

    tile->unmapHost();

    tile->isOpen = true;

    return tile->tileMem;
}

float *CanvasContext::openTile(CanvasTile *tile)
{
    if (!tile->isOpen)
    {
        openTiles.push_front(tile);
    }

    tile->mapHost();

    tile->isOpen = true;

    return tile->tileData;
}

void CanvasContext::closeTile(CanvasTile *tile)
{
    if (!tile->isOpen)
        return;

    tile->mapHost();

    /* Push the new data to OpenGL */
    glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, tile->tileBuffer);
    glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                          sizeof(float) * TILE_COMP_TOTAL,
                          tile->tileData,
                          GL_STREAM_DRAW);

    tile->isOpen = false;
}

void CanvasContext::closeTiles(void)
{
    while(!openTiles.empty())
    {
        CanvasTile *tile = openTiles.front();

        closeTile(tile);

        openTiles.pop_front();
    }
}