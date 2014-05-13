#include "basicstrokecontext.h"
#include <algorithm>
#include <cmath>
#include <QSet>
#include <QList>

void BasicStrokeContext::drawDab(QPointF point, TileSet &modTiles)
{
    cl_int intRadius = radius;
    int ix_start = tile_indice(point.x() - intRadius, TILE_PIXEL_WIDTH);
    int iy_start = tile_indice(point.y() - intRadius, TILE_PIXEL_HEIGHT);

    int ix_end   = tile_indice(point.x() + intRadius, TILE_PIXEL_WIDTH);
    int iy_end   = tile_indice(point.y() + intRadius, TILE_PIXEL_HEIGHT);

    const size_t global_work_size[2] = {TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH};
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->circleKernel;

    cl_int err = CL_SUCCESS;
    cl_int stride = TILE_PIXEL_WIDTH;

    err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&stride);
    err = clSetKernelArg(kernel, 4, sizeof(cl_int), (void *)&intRadius);

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            pixel[(ix + iy) % 3] = 0.0f;

            cl_int offsetX = point.x() - (ix * TILE_PIXEL_WIDTH);
            cl_int offsetY = point.y() - (iy * TILE_PIXEL_HEIGHT);
            cl_mem data = ctx->clOpenTileAt(ix, iy);

            modTiles.insert(QPoint(ix, iy));

            err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
            err = clSetKernelArg(kernel, 2, sizeof(cl_int), (void *)&offsetX);
            err = clSetKernelArg(kernel, 3, sizeof(cl_int), (void *)&offsetY);
            err = clSetKernelArg(kernel, 5, sizeof(cl_float4), (void *)&pixel);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel, 2,
                                         NULL, global_work_size, NULL,
                                         0, NULL, NULL);
        }
    }

    (void)err;

    lastDab = point;
}

TileSet BasicStrokeContext::startStroke(QPointF point, float pressure)
{
    TileSet modTiles;
    (void)pressure;

    start = point;
    drawDab(point, modTiles);
    return modTiles;
}

TileSet BasicStrokeContext::strokeTo(QPointF point, float pressure)
{
    TileSet modTiles;
    (void)pressure;

    float dist = sqrtf(powf(lastDab.x() - point.x(), 2.0f) + powf(lastDab.y() - point.y(), 2.0f));

    if (dist >= 1.0f)
    {
        int x0 = lastDab.x();
        int y0 = lastDab.y();
        int x1 = point.x();
        int y1 = point.y();
        int x = x0;
        int y = y0;

        bool steep = false;
        int sx, sy;
        int dx = abs(x1 - x0);
        if ((x1 - x0) > 0)
          sx = 1;
        else
          sx = -1;

        int dy = abs(y1 - y0);
        if ((y1 - y0) > 0)
          sy = 1;
        else
          sy = -1;

        if (dy > dx)
        {
          steep = true;
          std::swap(x, y);
          std::swap(dy, dx);
          std::swap(sy, sx);
        }

        int d = (2 * dy) - dx;

        for (int i = 0; i < dx; ++i)
        {
            if (steep)
                drawDab(QPointF(y, x), modTiles);
            else
                drawDab(QPointF(x, y), modTiles);

            while (d >= 0)
            {
                y = y + sy;
                d = d - (2 * dx);
            }

            x = x + sx;
            d = d + (2 * dy);
        }
        drawDab(QPointF(x1, y1), modTiles);
        return modTiles;
    }
    return modTiles;
}


void BasicStrokeContext::multiplySize(float mult)
{
    radius *= mult;
}

float BasicStrokeContext::getPixelRadius()
{
    return radius;
}
