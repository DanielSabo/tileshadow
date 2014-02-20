#include "mypaintstrokecontext.h"
#include <QElapsedTimer>
#include <cstring>
#include <iostream>
#include <cmath>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

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
    QElapsedTimer         timer;
};

bool MyPaintStrokeContext::fromJsonFile(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "MyPaint brush error, couldn't open " << path;
        return false;
    }

    QByteArray source = file.readAll();
    if (source.isNull())
    {
        qWarning() << "MyPaint brush error, couldn't read " << path;
        return false;
    }

    QJsonDocument brushJsonDoc = QJsonDocument::fromJson(source);

    if (!brushJsonDoc.isObject())
    {
        qWarning() << "JSON parse failed " << __LINE__;
        return false;
    }

    QJsonObject brushObj = brushJsonDoc.object();
    QJsonValue  val = brushObj.value("version");
    if (brushObj.value("version").toDouble(-1) != 3)
    {
        qWarning() << "JSON parse failed " << __LINE__;
        return false;
    }

    val = brushObj.value("settings");
    if (val.isObject())
    {
        QJsonObject settingsObj = val.toObject();
        QJsonObject::iterator iter;

        for (iter = settingsObj.begin(); iter != settingsObj.end(); ++iter)
        {
            QString     settingName = iter.key();
            QJsonObject settingObj  = iter.value().toObject();

            MyPaintBrushSetting settingID = mypaint_brush_setting_from_cname(settingName.toUtf8().constData());
            double baseValue = settingObj.value("base_value").toDouble(0);
            mypaint_brush_set_base_value(priv->brush, settingID, baseValue);

            QJsonObject inputsObj = settingObj.value("inputs").toObject();
            QJsonObject::iterator inputIter;

            for (inputIter = inputsObj.begin(); inputIter != inputsObj.end(); ++inputIter)
            {
                QString    inputName  = inputIter.key();
                QJsonArray inputArray = inputIter.value().toArray();

                MyPaintBrushInput inputID = mypaint_brush_input_from_cname(inputName.toUtf8().constData());
                int number_of_mapping_points = inputArray.size();

                mypaint_brush_set_mapping_n (priv->brush, settingID, inputID, number_of_mapping_points);

                for (int i = 0; i < number_of_mapping_points; ++i)
                {
                    QJsonArray point = inputArray.at(i).toArray();
                    float x = point.at(0).toDouble();
                    float y = point.at(1).toDouble();

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

MyPaintStrokeContext::MyPaintStrokeContext(CanvasContext *ctx) : StrokeContext(ctx)
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

bool MyPaintStrokeContext::startStroke(QPointF point, float pressure)
{
    (void)point; (void)pressure;

    mypaint_brush_reset (priv->brush);
    mypaint_brush_new_stroke(priv->brush);

    /* FIXME: brushlib doesn't seem to want the first point */
    #if 0
    mypaint_brush_stroke_to(priv->brush, (MyPaintSurface *)priv->surface,
                            point.x(), point.y(),
                            pressure /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            0.0 /* deltaTime in ms*/);
    #endif
    priv->timer.start();
    return true;
}


bool MyPaintStrokeContext::strokeTo(QPointF point, float pressure)
{
    double dt = priv->timer.restart() / 1000.0;
    mypaint_brush_stroke_to(priv->brush, (MyPaintSurface *)priv->surface,
                            point.x(), point.y(),
                            pressure /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            dt /* deltaTime in ms*/);
    return true;
}

static void getColorFunction (MyPaintSurface *base_surface,
                              float x, float y,
                              float radius,
                              float * color_r, float * color_g, float * color_b, float * color_a)
{
    CanvasMyPaintSurface *surface = (CanvasMyPaintSurface *)base_surface;
    CanvasContext *ctx = surface->strokeContext->ctx;

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

    colorAccumulatorMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                          2 * TILE_PIXEL_HEIGHT * sizeof(cl_float4), NULL, NULL);

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

    /* Part 2 */
    err = clSetKernelArg(kernel2, 0, sizeof(cl_mem), (void *)&colorAccumulatorMem);

    float totalValues[5] = {0, 0, 0, 0, 0};

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        const int tileOriginY = iy * TILE_PIXEL_HEIGHT;
        const int offsetY = std::max(firstPixelY - tileOriginY, 0);
        const int extraY = std::max(tileOriginY + TILE_PIXEL_HEIGHT - lastPixelY - 1, 0);

        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            CanvasTile *tile = ctx->getTile(ix, iy);

            const int tileOriginX = ix * TILE_PIXEL_WIDTH;
            const int offsetX = std::max(firstPixelX - tileOriginX, 0);
            const int extraX = std::max(tileOriginX + TILE_PIXEL_WIDTH - lastPixelX - 1, 0);

            float tileY = (y + 0.5f) - tileOriginY - offsetY;
            float tileX = (x + 0.5f) - tileOriginX - offsetX;

            cl_int width = TILE_PIXEL_WIDTH - offsetX - extraX;
            cl_int height = TILE_PIXEL_HEIGHT - offsetY - extraY;
            cl_int offset = offsetX + offsetY * stride;

            size_t global_work_size[1] = {height};

            cl_mem data = ctx->clOpenTile(tile);

            err = clSetKernelArg(kernel1, 0, sizeof(cl_mem), (void *)&data);
            check_cl_error(err);
            err = clSetKernelArg(kernel1, 1, sizeof(float), (void *)&tileX);
            check_cl_error(err);
            err = clSetKernelArg(kernel1, 2, sizeof(float), (void *)&tileY);
            check_cl_error(err);
            err = clSetKernelArg(kernel1, 3, sizeof(cl_int), (void *)&offset);
            check_cl_error(err);
            err = clSetKernelArg(kernel1, 4, sizeof(cl_int), (void *)&width);
            check_cl_error(err);
            err = clSetKernelArg(kernel1, 5, sizeof(cl_int), (void *)&height);
            check_cl_error(err);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel1, 1,
                                         NULL, global_work_size, NULL,
                                         0, NULL, NULL);
            check_cl_error(err);

            global_work_size[0] = 1;
            err = clSetKernelArg(kernel2, 1, sizeof(cl_int), (void *)&height);
            check_cl_error(err);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel2, 1,
                                         NULL, global_work_size, NULL,
                                         0, NULL, NULL);
            check_cl_error(err);

            float outputBuffer[5];

            clEnqueueReadBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue, colorAccumulatorMem, CL_TRUE,
                                0, sizeof(float) * 5, outputBuffer,
                                0, NULL, NULL);

            // clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);

            totalValues[0] += outputBuffer[0];
            totalValues[1] += outputBuffer[1];
            totalValues[2] += outputBuffer[2];
            totalValues[3] += outputBuffer[3];
            totalValues[4] += outputBuffer[4];
        }
    }

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

    (void)colorize;

    if (lock_alpha > 0.0f)
        qWarning() << "drawDab called with unsupported values lock_alpha = " << lock_alpha << endl;

    if (color_a != 1.0f)
        qWarning() << "drawDab called with unsupported values color_a = " << color_a << endl;

    if (colorize != 0.0f)
        qWarning() << "drawDab called with unsupported values colorize = " << colorize << endl;

    // cout << "Invoked drawDabFunction at " << x << ", " << y << endl;

    if (radius >= 1.0f)
    {
        CanvasContext *ctx = surface->strokeContext->ctx;

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
        err = clSetKernelArg(kernel, 12, sizeof(cl_float4), (void *)&color);

        for (int iy = iy_start; iy <= iy_end; ++iy)
        {
            const int tileOriginY = iy * TILE_PIXEL_HEIGHT;
            const int offsetY = std::max(firstPixelY - tileOriginY, 0);
            const int extraY = std::max(tileOriginY + TILE_PIXEL_HEIGHT - lastPixelY - 1, 0);

            for (int ix = ix_start; ix <= ix_end; ++ix)
            {
                CanvasTile *tile = ctx->getTile(ix, iy);

                const int tileOriginX = ix * TILE_PIXEL_WIDTH;
                const int offsetX = std::max(firstPixelX - tileOriginX, 0);
                const int extraX = std::max(tileOriginX + TILE_PIXEL_WIDTH - lastPixelX - 1, 0);

                float tileY = (y + 0.5f) - tileOriginY - offsetY;
                float tileX = (x + 0.5f) - tileOriginX - offsetX;

                int width = TILE_PIXEL_WIDTH - offsetX - extraX;
                int height = TILE_PIXEL_HEIGHT - offsetY - extraY;
                cl_int offset = offsetX + offsetY * stride;

                size_t global_work_size[2] = {width, height};

                cl_mem data = ctx->clOpenTile(tile);

                err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
                err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&offset);
                err = clSetKernelArg(kernel, 3, sizeof(float), (void *)&tileX);
                err = clSetKernelArg(kernel, 4, sizeof(float), (void *)&tileY);
                err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                             kernel, 2,
                                             NULL, global_work_size, NULL,
                                             0, NULL, NULL);
            }
        }

        return 1;
    }

    return 0; /* Returns non-zero if the surface was modified */
}
