#include "mypaintstrokecontext.h"
#include "canvastile.h"
#include <cstring>
#include <iostream>
#include <qmath.h>
#include <QFile>
#include <QDebug>
#include <QColor>

extern "C" {
#include "mypaint-brush.h"
};

using namespace std;

static void getColorFunction (MyPaintSurface *surface,
                              float x, float y,
                              float radius,
                              float * color_r, float * color_g, float * color_b, float * color_a);

static int drawDabFunction (MyPaintSurface *surface,
                            float x, float y,
                            float radius,
                            float color_r, float color_g, float color_b,
                            float opaque, float hardness,
                            float alpha_eraser,
                            float aspect_ratio, float angle,
                            float lock_alpha,
                            float colorize);

typedef struct
{
  MyPaintSurface parent;
  MyPaintStrokeContext *strokeContext;
} CanvasMyPaintSurface;

class MyPaintStrokeContextPrivate
{
public:
    MyPaintBrush         *brush;
    CanvasMyPaintSurface *surface;
    TileSet               modTiles;
};

bool MyPaintStrokeContext::fromSettings(const MyPaintToolSettings &settings)
{
    if (settings.empty())
    {
        mypaint_brush_from_defaults(priv->brush);
        mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_H, 0.0);
        mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_S, 1.0);
        mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_V, 1.0);
        return false;
    }
    else
    {
        MyPaintToolSettings::const_iterator iter;

        for (iter = settings.begin(); iter != settings.end(); ++iter)
        {
            QString settingName = iter.key();

            MyPaintBrushSetting settingID = mypaint_brush_setting_from_cname(settingName.toUtf8().constData());
            float baseValue = iter.value().first;
            mypaint_brush_set_base_value(priv->brush, settingID, baseValue);

            QMap<QString, QList<QPointF> >::const_iterator inputIter;

            for (inputIter = iter.value().second.begin(); inputIter != iter.value().second.end(); ++inputIter)
            {
                QString inputName = inputIter.key();
                QList<QPointF> const &mappingPoints = inputIter.value();

                MyPaintBrushInput inputID = mypaint_brush_input_from_cname(inputName.toUtf8().constData());
                // int number_of_mapping_points = inputIter->second.size();
                int number_of_mapping_points = mappingPoints.size();

                mypaint_brush_set_mapping_n (priv->brush, settingID, inputID, number_of_mapping_points);

                for (int i = 0; i < number_of_mapping_points; ++i)
                {
                    float x = mappingPoints.at(i).x();
                    float y = mappingPoints.at(i).y();

                    mypaint_brush_set_mapping_point(priv->brush, settingID, inputID, i, x, y);
                }
            }
        }
    }

    return true;
}

void MyPaintStrokeContext::fromDefaults()
{
    mypaint_brush_from_defaults(priv->brush);
    mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_H, 0.0);
    mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_S, 1.0);
    mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_V, 1.0);
}

MyPaintStrokeContext::MyPaintStrokeContext(CanvasLayer *layer) : StrokeContext(layer)
{
    priv = new MyPaintStrokeContextPrivate();

    priv->brush = mypaint_brush_new();

    priv->surface = new CanvasMyPaintSurface;
    memset(priv->surface, 0, sizeof(MyPaintSurface));

    priv->surface->strokeContext = this;
    priv->surface->parent.draw_dab = drawDabFunction;
    priv->surface->parent.get_color = getColorFunction;
}

MyPaintStrokeContext::~MyPaintStrokeContext()
{
    mypaint_brush_unref(priv->brush);
    priv->brush = NULL;
    delete priv->surface;
    delete priv;
}

TileSet MyPaintStrokeContext::startStroke(QPointF point, float pressure)
{
    mypaint_brush_reset (priv->brush);
    mypaint_brush_new_stroke(priv->brush);

    mypaint_brush_stroke_to(priv->brush, (MyPaintSurface *)priv->surface,
                            point.x(), point.y(),
                            0.0f /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            1.0f /* deltaTime in seconds */);

    mypaint_brush_stroke_to(priv->brush, (MyPaintSurface *)priv->surface,
                            point.x(), point.y(),
                            pressure /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            1.0f / 60.0f /* deltaTime in seconds */);
    priv->modTiles.clear();
    return TileSet();
}


TileSet MyPaintStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    mypaint_brush_stroke_to(priv->brush, (MyPaintSurface *)priv->surface,
                            point.x(), point.y(),
                            pressure /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            dt / 1000.0f /* deltaTime in seconds */);

    TileSet result = priv->modTiles;
    priv->modTiles.clear();
    return result;
}

static void getColorFunction (MyPaintSurface *base_surface,
                              float x, float y,
                              float radius,
                              float * color_r, float * color_g, float * color_b, float * color_a)
{
    CanvasMyPaintSurface *surface = (CanvasMyPaintSurface *)base_surface;
    CanvasLayer *layer = surface->strokeContext->layer;

    if (radius < 1.0f)
        radius = 1.0f;

    int fringe_radius = radius + 1.5f;

    int firstPixelX = floorf(x - fringe_radius);
    int firstPixelY = floorf(y - fringe_radius);
    int lastPixelX = ceilf(x + fringe_radius);
    int lastPixelY = ceilf(y + fringe_radius);

    int ix_start = tile_indice(firstPixelX, TILE_PIXEL_WIDTH);
    int iy_start = tile_indice(firstPixelY, TILE_PIXEL_HEIGHT);

    int ix_end   = tile_indice(lastPixelX, TILE_PIXEL_WIDTH);
    int iy_end   = tile_indice(lastPixelY, TILE_PIXEL_HEIGHT);

    cl_kernel kernel1 = SharedOpenCL::getSharedOpenCL()->mypaintGetColorKernelPart1;
    cl_kernel kernel2 = SharedOpenCL::getSharedOpenCL()->mypaintGetColorKernelPart2;

    cl_mem colorAccumulatorMem;

    int tile_count = (iy_end - iy_start + 1) * (ix_end - ix_start + 1);

    colorAccumulatorMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE,
                                          2 * TILE_PIXEL_HEIGHT * sizeof(cl_float4) * tile_count, NULL, NULL);

    cl_int err = CL_SUCCESS;
    cl_int stride = TILE_PIXEL_WIDTH;

/*
__kernel void mypaint_color_query_part1(__global float4 *buf,
                                                  float   x,
                                                  float   y,
                                                  int     offset,
                                                  int     width,
                                                  int     height,
                                         __global float4 *accum,
                                                  int     stride,
                                                  float   radius)

__kernel void mypaint_color_query_part2(__global float4 *accum,
                                                 int     count)
{
*/

    /* Part 1 */
    err = clSetKernelArg(kernel1, 6, sizeof(cl_mem), (void *)&colorAccumulatorMem);
    err = clSetKernelArg(kernel1, 7, sizeof(cl_int), (void *)&stride);
    err = clSetKernelArg(kernel1, 8, sizeof(float),  (void *)&radius);

    cl_int row_count = 0;

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        const int tileOriginY = iy * TILE_PIXEL_HEIGHT;
        const int offsetY = std::max(firstPixelY - tileOriginY, 0);
        const int extraY = std::max(tileOriginY + TILE_PIXEL_HEIGHT - lastPixelY - 1, 0);

        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            const int tileOriginX = ix * TILE_PIXEL_WIDTH;
            const int offsetX = std::max(firstPixelX - tileOriginX, 0);
            const int extraX = std::max(tileOriginX + TILE_PIXEL_WIDTH - lastPixelX - 1, 0);

            float tileY = (y + 0.5f) - tileOriginY - offsetY;
            float tileX = (x + 0.5f) - tileOriginX - offsetX;

            cl_int width = TILE_PIXEL_WIDTH - offsetX - extraX;
            cl_int height = TILE_PIXEL_HEIGHT - offsetY - extraY;
            cl_int offset = offsetX + offsetY * stride;

            size_t global_work_size[1] = CL_DIM1(height);
            size_t local_work_size[1] = CL_DIM1(1);

            cl_mem data = layer->clOpenTileAt(ix, iy);

            err = clSetKernelArg(kernel1, 0, sizeof(cl_mem), (void *)&data);
            err = clSetKernelArg(kernel1, 1, sizeof(float), (void *)&tileX);
            err = clSetKernelArg(kernel1, 2, sizeof(float), (void *)&tileY);
            err = clSetKernelArg(kernel1, 3, sizeof(cl_int), (void *)&offset);
            err = clSetKernelArg(kernel1, 4, sizeof(cl_int), (void *)&width);
            err = clSetKernelArg(kernel1, 5, sizeof(cl_int), (void *)&row_count);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel1, 1,
                                         NULL, global_work_size, local_work_size,
                                         0, NULL, NULL);
            check_cl_error(err);

            row_count += height;
        }
    }

    size_t global_work_size[1] = {1};
    err = clSetKernelArg(kernel2, 0, sizeof(cl_mem), (void *)&colorAccumulatorMem);
    err = clSetKernelArg(kernel2, 1, sizeof(cl_int), (void *)&row_count);
    err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                 kernel2, 1,
                                 NULL, global_work_size, NULL,
                                 0, NULL, NULL);
    check_cl_error(err);


    float totalValues[5] = {0, 0, 0, 0, 0};
    clEnqueueReadBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue, colorAccumulatorMem, CL_TRUE,
                        0, sizeof(float) * 5, totalValues,
                        0, NULL, NULL);

    if (totalValues[4] > 0.0f)
    {
        *color_r = totalValues[0] / totalValues[4];
        *color_g = totalValues[1] / totalValues[4];
        *color_b = totalValues[2] / totalValues[4];
        *color_a = totalValues[3] / totalValues[4];
    }

    clReleaseMemObject(colorAccumulatorMem);
}

static int drawDabFunction (MyPaintSurface *base_surface,
                            float x, float y,
                            float radius,
                            float color_r, float color_g, float color_b,
                            float opaque, float hardness,
                            float color_a,
                            float aspect_ratio, float angle,
                            float lock_alpha,
                            float colorize)
{
    CanvasMyPaintSurface *surface = (CanvasMyPaintSurface *)base_surface;

    if (hardness < 0.0f)
        hardness = 0.0f;
    else if (hardness > 1.0f)
        hardness = 1.0f;

    if (aspect_ratio < 1.0f)
        aspect_ratio = 1.0f;

    if (lock_alpha > 0.0f)
        qWarning() << "drawDab called with unsupported values lock_alpha = " << lock_alpha << endl;

    if (colorize != 0.0f)
        qWarning() << "drawDab called with unsupported values colorize = " << colorize << endl;

    // cout << "Invoked drawDabFunction at " << x << ", " << y << endl;

    if (radius >= 1.0f)
    {
        CanvasLayer *layer = surface->strokeContext->layer;

        int fringe_radius = radius + 1.5f;

        int firstPixelX = floorf(x - fringe_radius);
        int firstPixelY = floorf(y - fringe_radius);
        int lastPixelX = ceilf(x + fringe_radius);
        int lastPixelY = ceilf(y + fringe_radius);

        int ix_start = tile_indice(firstPixelX, TILE_PIXEL_WIDTH);
        int iy_start = tile_indice(firstPixelY, TILE_PIXEL_HEIGHT);

        int ix_end   = tile_indice(lastPixelX, TILE_PIXEL_WIDTH);
        int iy_end   = tile_indice(lastPixelY, TILE_PIXEL_HEIGHT);

        cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->mypaintDabKernel;

        cl_int err = CL_SUCCESS;
        cl_int stride = TILE_PIXEL_WIDTH;
        /*
        float segments[4] =
            {
                1.0f, -(1.0f / hardness - 1.0f),
                hardness / (1.0f - hardness), -hardness / (1.0f - hardness),
            };
        */
        // float color[4] = {color_r, color_g, color_b, color_a};
        float color[4] = {color_r, color_g, color_b, opaque};

        float angle_rad = angle / 360 * 2 * M_PI;
        float sn = sinf(angle_rad);
        float cs = cosf(angle_rad);
        float slope1 = -(1.0f / hardness - 1.0f);
        float slope2 = -(hardness / (1.0f - hardness));

        err = clSetKernelArg(kernel, 2, sizeof(cl_int), (void *)&stride);
        err = clSetKernelArg(kernel, 5, sizeof(float), (void *)&radius);
        err = clSetKernelArg(kernel, 6, sizeof(float), (void *)&hardness);
        err = clSetKernelArg(kernel, 7, sizeof(float), (void *)&aspect_ratio);
        err = clSetKernelArg(kernel, 8, sizeof(float), (void *)&sn);
        err = clSetKernelArg(kernel, 9, sizeof(float), (void *)&cs);
        err = clSetKernelArg(kernel, 10, sizeof(float), (void *)&slope1);
        err = clSetKernelArg(kernel, 11, sizeof(float), (void *)&slope2);
        err = clSetKernelArg(kernel, 12, sizeof(float), (void *)&color_a);
        err = clSetKernelArg(kernel, 13, sizeof(cl_float4), (void *)&color);

        for (int iy = iy_start; iy <= iy_end; ++iy)
        {
            const int tileOriginY = iy * TILE_PIXEL_HEIGHT;
            const int offsetY = std::max(firstPixelY - tileOriginY, 0);
            const int extraY = std::max(tileOriginY + TILE_PIXEL_HEIGHT - lastPixelY - 1, 0);

            for (int ix = ix_start; ix <= ix_end; ++ix)
            {
                const int tileOriginX = ix * TILE_PIXEL_WIDTH;
                const int offsetX = std::max(firstPixelX - tileOriginX, 0);
                const int extraX = std::max(tileOriginX + TILE_PIXEL_WIDTH - lastPixelX - 1, 0);

                float tileY = (y + 0.5f) - tileOriginY - offsetY;
                float tileX = (x + 0.5f) - tileOriginX - offsetX;

                int width = TILE_PIXEL_WIDTH - offsetX - extraX;
                int height = TILE_PIXEL_HEIGHT - offsetY - extraY;
                cl_int offset = offsetX + offsetY * stride;

                size_t global_work_size[2] = CL_DIM2(width, height);
                size_t local_work_size[2] = CL_DIM2(width, 1);

                surface->strokeContext->priv->modTiles.insert(QPoint(ix, iy));
                cl_mem data = layer->clOpenTileAt(ix, iy);

                err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
                err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&offset);
                err = clSetKernelArg(kernel, 3, sizeof(float), (void *)&tileX);
                err = clSetKernelArg(kernel, 4, sizeof(float), (void *)&tileY);
                err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                             kernel, 2,
                                             NULL, global_work_size, local_work_size,
                                             0, NULL, NULL);
            }
        }

        return 1;
    }

    return 0; /* Returns non-zero if the surface was modified */
}
