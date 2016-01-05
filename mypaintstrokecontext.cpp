#include "mypaintstrokecontext.h"
#include "canvastile.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <qmath.h>
#include <QMatrix>
#include <QFile>
#include <QDebug>
#include <QColor>

extern "C" {
#include "mypaint-brush.h"
}

static void getColorFunction (MyPaintSurface *surface,
                              float x, float y,
                              float radius,
                              float * color_r, float * color_g, float * color_b, float * color_a);

static int drawDabFunction (MyPaintSurface *surface,
                            float x, float y,
                            float radius,
                            float color_r, float color_g, float color_b,
                            float opaque, float hardness,
                            float color_a,
                            float aspect_ratio, float angle,
                            float lock_alpha,
                            float colorize);

struct CanvasMyPaintSurface : public MyPaintSurface
{
    MyPaintStrokeContext *strokeContext;
};

class CLMaskImage
{
public:
    CLMaskImage(cl_mem image, QSize size) : image(image), size(size) {}
    CLMaskImage(CLMaskImage &from) = delete;
    CLMaskImage& operator=(CLMaskImage &from) = delete;
    CLMaskImage(CLMaskImage &&from);
    CLMaskImage& operator=(CLMaskImage &&from);

    ~CLMaskImage();

    int width() const { return size.width(); }
    int height() const { return size.height(); }

    cl_mem image;
    QSize  size;
};

CLMaskImage::CLMaskImage(CLMaskImage &&from)
    : image(from.image), size(from.size)
{
    from.image = 0;
}

CLMaskImage& CLMaskImage::operator=(CLMaskImage &&from)
{
    std::swap(image, from.image);
    std::swap(size, from.size);

    return *this;
}

CLMaskImage::~CLMaskImage()
{
    if (image)
        clReleaseMemObject(image);
}

class MyPaintStrokeContextPrivate
{
public:
    MyPaintBrush            *brush;
    int                      activeMask;
    std::vector<MipSet<CLMaskImage>> masks;
    CanvasMyPaintSurface     surface;
    TileSet                  modTiles;
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

void MyPaintStrokeContext::setMasks(const QList<MaskBuffer> &masks)
{
    priv->masks.clear();
    priv->activeMask = 0;

    for (auto const &baseMask: masks)
    {
        MipSet<CLMaskImage> clMips;
        MipSet<MaskBuffer> maskMips = mipsFromMask(baseMask);

//        qDebug() << "mips:" << maskMips.mips.size();

        for (auto const mask: maskMips)
        {
            cl_int err = CL_SUCCESS;
            cl_image_format fmt = {CL_INTENSITY, CL_UNORM_INT8};

            cl_mem maskImage = clCreateImage2D(SharedOpenCL::getSharedOpenCL()->ctx,
                                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                              &fmt,
                                              mask.width(), mask.height(), 0,
                                              (void *)mask.constData(), &err);
            check_cl_error(err);

            if (err == CL_SUCCESS)
            {
//                qDebug() << "  loaded:" << mask.width() << mask.height();
                clMips.mips.emplace_back(maskImage, QSize(mask.width(), mask.height()));
            }
            else
            {
                break;
            }
        }

        if (!clMips.mips.empty())
        priv->masks.push_back(std::move(clMips));
    }
}

MyPaintStrokeContext::MyPaintStrokeContext(CanvasLayer *layer) : StrokeContext(layer)
{
    priv = new MyPaintStrokeContextPrivate();

    priv->brush = mypaint_brush_new();

    memset(&priv->surface, 0, sizeof(MyPaintSurface));
    priv->surface.strokeContext = this;
    priv->surface.draw_dab = drawDabFunction;
    priv->surface.get_color = getColorFunction;
}

MyPaintStrokeContext::~MyPaintStrokeContext()
{
    mypaint_brush_unref(priv->brush);
    priv->brush = nullptr;
    delete priv;
}

TileSet MyPaintStrokeContext::startStroke(QPointF point, float pressure)
{
    mypaint_brush_reset (priv->brush);
    mypaint_brush_new_stroke(priv->brush);

    priv->modTiles.clear();
    mypaint_brush_stroke_to(priv->brush, &priv->surface,
                            point.x(), point.y(),
                            0.0f /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            1.0f /* deltaTime in seconds */);

    mypaint_brush_stroke_to(priv->brush, &priv->surface,
                            point.x(), point.y(),
                            pressure /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            1.0f / 60.0f /* deltaTime in seconds */);
    return priv->modTiles;
}


TileSet MyPaintStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    priv->modTiles.clear();
    mypaint_brush_stroke_to(priv->brush, &priv->surface,
                            point.x(), point.y(),
                            pressure /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            dt / 1000.0f /* deltaTime in seconds */);
    return priv->modTiles;
}

static void getColorFunction (MyPaintSurface *base_surface,
                              float x, float y,
                              float radius,
                              float * color_r, float * color_g, float * color_b, float * color_a)
{
    CanvasMyPaintSurface *surface = static_cast<CanvasMyPaintSurface *>(base_surface);
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
                                          2 * TILE_PIXEL_HEIGHT * sizeof(cl_float4) * tile_count, nullptr, nullptr);

    cl_int err = CL_SUCCESS;

    /* Part 1 */
    err = clSetKernelArg<cl_mem>(kernel1, 6, colorAccumulatorMem);
    err = clSetKernelArg<cl_float>(kernel1, 7, radius);

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
            cl_int offset = offsetX + offsetY * TILE_PIXEL_WIDTH;

            size_t global_work_size[1] = CL_DIM1(height);
            size_t local_work_size[1] = CL_DIM1(1);

            cl_mem data = layer->clOpenTileAt(ix, iy);

            err = clSetKernelArg<cl_mem>(kernel1, 0, data);
            err = clSetKernelArg<cl_float>(kernel1, 1, tileX);
            err = clSetKernelArg<cl_float>(kernel1, 2, tileY);
            err = clSetKernelArg<cl_int>(kernel1, 3, offset);
            err = clSetKernelArg<cl_int>(kernel1, 4, width);
            err = clSetKernelArg<cl_int>(kernel1, 5, row_count);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel1, 1,
                                         nullptr, global_work_size, local_work_size,
                                         0, nullptr, nullptr);
            check_cl_error(err);

            row_count += height;
        }
    }

    size_t global_work_size[1] = {1};
    err = clSetKernelArg<cl_mem>(kernel2, 0, colorAccumulatorMem);
    err = clSetKernelArg<cl_int>(kernel2, 1, row_count);
    err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                 kernel2, 1,
                                 nullptr, global_work_size, nullptr,
                                 0, nullptr, nullptr);
    check_cl_error(err);


    float totalValues[5] = {0, 0, 0, 0, 0};
    clEnqueueReadBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue, colorAccumulatorMem, CL_TRUE,
                        0, sizeof(float) * 5, totalValues,
                        0, nullptr, nullptr);

    if (totalValues[4] > 0.0f && totalValues[3] > 0.0f)
    {
        totalValues[0] /= totalValues[4];
        totalValues[1] /= totalValues[4];
        totalValues[2] /= totalValues[4];
        totalValues[3] /= totalValues[4];

        *color_r = qBound(0.0f, totalValues[0] / totalValues[3], 1.0f);
        *color_g = qBound(0.0f, totalValues[1] / totalValues[3], 1.0f);
        *color_b = qBound(0.0f, totalValues[2] / totalValues[3], 1.0f);
        *color_a = qBound(0.0f, totalValues[3], 1.0f);
    }
    else
    {
        *color_r = 0.0f;
        *color_g = 0.0f;
        *color_b = 0.0f;
        *color_a = 0.0f;
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
    if (radius < 1.0f)
        return 0;

    if (hardness < 0.0f)
        hardness = 0.0f;
    else if (hardness > 1.0f)
        hardness = 1.0f;

    if (aspect_ratio < 1.0f)
        aspect_ratio = 1.0f;

    if (lock_alpha > 0.0f && lock_alpha < 1.0f)
        qWarning() << "drawDab called with unsupported values lock_alpha =" << lock_alpha;

    if (colorize != 0.0f)
        qWarning() << "drawDab called with unsupported values colorize = " << colorize << endl;

    CanvasMyPaintSurface *surface = static_cast<CanvasMyPaintSurface *>(base_surface);
    CanvasLayer *layer = surface->strokeContext->layer;
    MyPaintStrokeContextPrivate *priv = surface->strokeContext->priv;
    QRectF boundRect;
    cl_kernel kernel;
    cl_int err = CL_SUCCESS;
    (void)err; /* Ignore the fact that err is unused, it's helpful for debugging */

    if (priv->masks.empty())
    {
        QMatrix transform;
        transform.scale(1.0f, aspect_ratio);
        transform.scale(1.0f / radius, 1.0f / radius);
        transform.rotate(-angle);

        boundRect.setRect(-1.0f, -1.0f, 2.0f, 2.0f);
        boundRect = transform.inverted().mapRect(boundRect).adjusted(-2.0, -2.0, 4.0, 4.0);

        if (lock_alpha > 0.0f)
            kernel = SharedOpenCL::getSharedOpenCL()->mypaintDabLockedKernel;
        else
            kernel = SharedOpenCL::getSharedOpenCL()->mypaintDabKernel;

        cl_float4 color = {color_r, color_g, color_b, opaque};
        cl_float4 transformMatrix;
        transformMatrix.s[0] = transform.m11();
        transformMatrix.s[1] = transform.m21();
        transformMatrix.s[2] = transform.m12();
        transformMatrix.s[3] = transform.m22();

        float slope1 = -(1.0f / hardness - 1.0f);
        float slope2 = -(hardness / (1.0f - hardness));

        err = clSetKernelArg<cl_float>(kernel, 4, hardness);
        err = clSetKernelArg<cl_float4>(kernel, 5, transformMatrix);
        err = clSetKernelArg<cl_float>(kernel, 6, slope1);
        err = clSetKernelArg<cl_float>(kernel, 7, slope2);
        err = clSetKernelArg<cl_float>(kernel, 8, color_a);
        err = clSetKernelArg<cl_float4>(kernel, 9, color);
    }
    else
    {
        QMatrix transform;
        CLMaskImage const &maskImage = *(priv->masks[priv->activeMask].bestForSize(radius * 2));
        priv->activeMask = (priv->activeMask + 1) % priv->masks.size();
        float maskWidth = maskImage.size.width();
        float maskHeight = maskImage.size.height();

        // Preserve a non-rectangular mask's aspect ratio
        if (maskWidth > maskHeight)
            transform.scale(1.0f, maskWidth / maskHeight);
        else if (maskHeight > maskWidth)
            transform.scale(maskHeight / maskWidth, 1.0f);

        transform.scale(1.0f, aspect_ratio);
        transform.scale(0.5f / radius, 0.5f / radius);
        transform.rotate(-angle);

        boundRect.setRect(-0.5f, -0.5f, 1.0f, 1.0f);
        boundRect = transform.inverted().mapRect(boundRect).adjusted(-1.0, -1.0, 1.0, 1.0);

        if (lock_alpha > 0.0f)
            kernel = SharedOpenCL::getSharedOpenCL()->mypaintMaskDabLockedKernel;
        else
            kernel = SharedOpenCL::getSharedOpenCL()->mypaintMaskDabKernel;

        cl_float4 color = {color_r, color_g, color_b, opaque};
        cl_float4 transformMatrix;
        transformMatrix.s[0] = transform.m11();
        transformMatrix.s[1] = transform.m21();
        transformMatrix.s[2] = transform.m12();
        transformMatrix.s[3] = transform.m22();

        err = clSetKernelArg<cl_mem>(kernel, 4, maskImage.image);
        err = clSetKernelArg<cl_float4>(kernel, 5, transformMatrix);
        err = clSetKernelArg<cl_float>(kernel, 6, color_a);
        err = clSetKernelArg<cl_float4>(kernel, 7, color);
    }

    int firstPixelX = boundRect.x() + x;
    int firstPixelY = boundRect.y() + y;
    int lastPixelX = firstPixelX + boundRect.width();
    int lastPixelY = firstPixelY + boundRect.height();

    int ix_start = tile_indice(firstPixelX, TILE_PIXEL_WIDTH);
    int iy_start = tile_indice(firstPixelY, TILE_PIXEL_HEIGHT);

    int ix_end   = tile_indice(lastPixelX, TILE_PIXEL_WIDTH);
    int iy_end   = tile_indice(lastPixelY, TILE_PIXEL_HEIGHT);

    cl_command_queue cmdQueue = SharedOpenCL::getSharedOpenCL()->cmdQueue;

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
            cl_int offset = offsetX + offsetY * TILE_PIXEL_WIDTH;

            size_t global_work_size[2] = CL_DIM2(width, height);
            size_t local_work_size[2] = CL_DIM2(width, 1);

            priv->modTiles.insert(QPoint(ix, iy));
            cl_mem data = layer->clOpenTileAt(ix, iy);

            err = clSetKernelArg<cl_mem>(kernel, 0, data);
            err = clSetKernelArg<cl_int>(kernel, 1, offset);
            err = clSetKernelArg<cl_float>(kernel, 2, tileX);
            err = clSetKernelArg<cl_float>(kernel, 3, tileY);
            err = clEnqueueNDRangeKernel(cmdQueue, kernel, 2,
                                         nullptr, global_work_size, local_work_size,
                                         0, nullptr, nullptr);
        }
    }

    return 1;
}
