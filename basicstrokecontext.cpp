#include "basicstrokecontext.h"
#include <algorithm>
#include <cmath>

void BasicStrokeContext::drawDab(QPointF point)
{
    cl_int radius = 10;
    int ix_start = (point.x() - radius) / TILE_PIXEL_WIDTH;
    int iy_start = (point.y() - radius) / TILE_PIXEL_HEIGHT;

    int ix_end   = (point.x() + radius) / TILE_PIXEL_WIDTH;
    int iy_end   = (point.y() + radius) / TILE_PIXEL_HEIGHT;

    const size_t global_work_size[2] = {TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH};
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->circleKernel;

    cl_int err = CL_SUCCESS;
    cl_int stride = TILE_PIXEL_WIDTH;

    err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&stride);
    err = clSetKernelArg(kernel, 4, sizeof(cl_int), (void *)&radius);

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            CanvasTile *tile = ctx->getTile(ix, iy);

            float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            pixel[(ix + iy) % 3] = 0.0f;

            cl_int offsetX = point.x() - (ix * TILE_PIXEL_WIDTH);
            cl_int offsetY = point.y() - (iy * TILE_PIXEL_HEIGHT);
            cl_mem data = ctx->clOpenTile(tile);

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

    lastDab = point;
}

bool BasicStrokeContext::startStroke(QPointF point)
{
    start = point;
    drawDab(point);
    return true;
}

bool BasicStrokeContext::strokeTo(QPointF point)
{
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
                drawDab(QPointF(y, x));
            else
                drawDab(QPointF(x, y));

            while (d >= 0)
            {
                y = y + sy;
                d = d - (2 * dx);
            }

            x = x + sx;
            d = d + (2 * dy);
        }
        drawDab(QPointF(x1, y1));
        return true;
    }
    return false;
}
