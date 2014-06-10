#include "canvaswidget-opencl.h"
#include "canvastile.h"

CanvasTile::CanvasTile(int x, int y) : x(x), y(y)
{
    tileMem = 0;
    tileData = NULL;
    tileMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                              TILE_COMP_TOTAL * sizeof(float), tileData, NULL);
}

CanvasTile::~CanvasTile(void)
{
    unmapHost();
    clReleaseMemObject(tileMem);
}

float *CanvasTile::mapHost(void)
{
    if (!tileData)
    {
        cl_int err = CL_SUCCESS;
        tileData = (float *)clEnqueueMapBuffer (SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem,
                                                      CL_TRUE, CL_MAP_READ | CL_MAP_WRITE,
                                                      0, TILE_COMP_TOTAL * sizeof(float),
                                                      0, NULL, NULL, &err);
    }

    return tileData;
}

cl_mem CanvasTile::unmapHost()
{
    if (tileData)
    {
        cl_int err = clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, NULL, NULL);
        tileData = NULL;
    }

    return tileMem;
}

void CanvasTile::fill(float r, float g, float b, float a)
{
    unmapHost();

    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->fillKernel;
    const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};

    float color[4] = {r, g, b, a};

    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&tileMem);
    clSetKernelArg(kernel, 1, sizeof(cl_float4), (void *)&color);
    clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                           kernel, 1,
                           NULL, global_work_size, NULL,
                           0, NULL, NULL);
}

CanvasTile *CanvasTile::copy()
{
    CanvasTile *result = new CanvasTile(x, y);

    unmapHost();
    result->unmapHost();

    clEnqueueCopyBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                        tileMem, result->tileMem, 0, 0,
                        TILE_COMP_TOTAL * sizeof(float),
                        0, NULL, NULL);

    return result;
}
