#include "canvaswidget-opencl.h"
#include "canvastile.h"

static QAtomicInt privAllocatedTileCount;
static QAtomicInt privDeviceTileCount;

int CanvasTile::allocatedTileCount()
{
    return privAllocatedTileCount;
}

int CanvasTile::deviceTileCount()
{
    return privDeviceTileCount;
}

CanvasTile::CanvasTile()
{
    tileMem = 0;
    tileData = NULL;
    tileMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                              TILE_COMP_TOTAL * sizeof(float), tileData, NULL);

    privAllocatedTileCount.ref();
    privDeviceTileCount.ref();
}

CanvasTile::~CanvasTile()
{
    if (tileMem)
    {
        unmapHost();
        clReleaseMemObject(tileMem);
        privDeviceTileCount.deref();
    }
    else if (tileData)
    {
        delete tileData;
    }

    privAllocatedTileCount.deref();
}

float *CanvasTile::mapHost()
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
    if (tileData && tileMem)
    {
        cl_int err = clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, NULL, NULL);
        tileData = NULL;
    }
    else if (tileData)
    {
        tileMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR ,
                                  TILE_COMP_TOTAL * sizeof(float), tileData, NULL);
        delete tileData;
        tileData = NULL;
        privDeviceTileCount.ref();
    }

    return tileMem;
}

void CanvasTile::swapHost()
{
    if (SharedOpenCL::getSharedOpenCL()->deviceType == CL_DEVICE_TYPE_CPU)
        return;

    if (tileMem && tileData)
    {
        cl_int err = clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, NULL, NULL);
        tileData = NULL;
    }

    if (!tileData)
    {
        tileData = new float[TILE_COMP_TOTAL];
        cl_int err = clEnqueueReadBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         tileMem, CL_TRUE,
                                         0, TILE_COMP_TOTAL * sizeof(float), tileData,
                                         0, NULL, NULL);
        err = clReleaseMemObject(tileMem);
        tileMem = 0;
        privDeviceTileCount.deref();
    }
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
    CanvasTile *result = new CanvasTile();

    unmapHost();
    result->unmapHost();

    clEnqueueCopyBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                        tileMem, result->tileMem, 0, 0,
                        TILE_COMP_TOTAL * sizeof(float),
                        0, NULL, NULL);

    return result;
}
