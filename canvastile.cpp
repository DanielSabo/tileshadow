#include "canvaswidget-opencl.h"
#include "canvastile.h"

static QAtomicInt privAllocatedTileCount;
static QAtomicInt privDeviceTileCount;

int CanvasTile::allocatedTileCount()
{
    return privAllocatedTileCount.load();
}

int CanvasTile::deviceTileCount()
{
    return privDeviceTileCount.load();
}

CanvasTile::CanvasTile()
{
    tileMem = 0;
    tileData = nullptr;
    tileMem = clCreateBuffer(SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                             TILE_COMP_TOTAL * sizeof(float), tileData, nullptr);

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
        tileData = (float *)clEnqueueMapBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem,
                                               CL_TRUE, CL_MAP_READ | CL_MAP_WRITE,
                                               0, TILE_COMP_TOTAL * sizeof(float),
                                               0, nullptr, nullptr, &err);
    }

    return tileData;
}

cl_mem CanvasTile::unmapHost()
{
    if (tileData && tileMem)
    {
        cl_int err = clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, nullptr, nullptr);
        tileData = nullptr;
    }
    else if (tileData)
    {
        tileMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR ,
                                  TILE_COMP_TOTAL * sizeof(float), tileData, nullptr);
        delete tileData;
        tileData = nullptr;
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
        cl_int err = clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, nullptr, nullptr);
        tileData = nullptr;
    }

    if (!tileData)
    {
        tileData = new float[TILE_COMP_TOTAL];
        cl_int err = clEnqueueReadBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         tileMem, CL_TRUE,
                                         0, TILE_COMP_TOTAL * sizeof(float), tileData,
                                         0, nullptr, nullptr);
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
                           nullptr, global_work_size, nullptr,
                           0, nullptr, nullptr);
}

void CanvasTile::blendOnto(CanvasTile *target, BlendMode::Mode mode, float opacity)
{
    const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
    cl_mem inMem  = target->unmapHost();
    cl_mem auxMem = unmapHost();

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
    clSetKernelArg(kernel, 3, sizeof(cl_float), (void *)&opacity);
    clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                           kernel,
                           1, nullptr, global_work_size, nullptr,
                           0, nullptr, nullptr);
}

std::unique_ptr<CanvasTile> CanvasTile::copy()
{
    CanvasTile *result = new CanvasTile();

    unmapHost();
    result->unmapHost();

    clEnqueueCopyBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                        tileMem, result->tileMem, 0, 0,
                        TILE_COMP_TOTAL * sizeof(float),
                        0, nullptr, nullptr);

    return std::unique_ptr<CanvasTile>(result);
}
