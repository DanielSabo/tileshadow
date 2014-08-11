#include "basicstrokecontext.h"
#include <qmath.h>
#include "canvastile.h"
#include "paintutils.h"

BasicStrokeContext::BasicStrokeContext(CanvasLayer *layer, float radius)
    : StrokeContext(layer), radius(radius)
{

}

void BasicStrokeContext::drawDab(QPointF point, TileSet &modTiles)
{
    cl_int intRadius = ceil(radius);
    cl_float floatRadius = radius;
    int ix_start = tile_indice(point.x() - intRadius, TILE_PIXEL_WIDTH);
    int iy_start = tile_indice(point.y() - intRadius, TILE_PIXEL_HEIGHT);

    int ix_end   = tile_indice(point.x() + intRadius, TILE_PIXEL_WIDTH);
    int iy_end   = tile_indice(point.y() + intRadius, TILE_PIXEL_HEIGHT);

    const size_t global_work_size[2] = {TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH};
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->circleKernel;

    cl_int err = CL_SUCCESS;
    cl_int stride = TILE_PIXEL_WIDTH;

    err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&stride);
    err = clSetKernelArg(kernel, 4, sizeof(cl_float), (void *)&floatRadius);

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};

            int index = (ix + iy) % 3;
            if (index < 0)
                index = (3 + index) % 3;
            pixel[index] = 0.0f;

            cl_int offsetX = point.x() - (ix * TILE_PIXEL_WIDTH);
            cl_int offsetY = point.y() - (iy * TILE_PIXEL_HEIGHT);
            cl_mem data = layer->clOpenTileAt(ix, iy);

            modTiles.insert(QPoint(ix, iy));

            err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
            err = clSetKernelArg(kernel, 2, sizeof(cl_int), (void *)&offsetX);
            err = clSetKernelArg(kernel, 3, sizeof(cl_int), (void *)&offsetY);
            err = clSetKernelArg(kernel, 5, sizeof(cl_float4), (void *)&pixel);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel, 2,
                                         nullptr, global_work_size, nullptr,
                                         0, nullptr, nullptr);
        }
    }

    (void)err;

    lastDab = point;
}

TileSet BasicStrokeContext::startStroke(QPointF point, float pressure)
{
    TileSet modTiles;

    start = point;
    drawDab(point, modTiles);
    return modTiles;
}

TileSet BasicStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    TileSet modTiles;

    std::vector<QPoint> line = interpolateLine(lastDab.toPoint(), point.toPoint());

    for (QPoint const &p: line)
        drawDab(p, modTiles);

    return modTiles;
}
