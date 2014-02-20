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

    (void)surface; (void)x; (void)y; (void)radius;

    *color_r = 1.0f;
    *color_g = 1.0f;
    *color_b = 1.0f;
    *color_a = 1.0f;

    cout << "Invoked getColorFunction at " << x << ", " << y << endl;
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

        /* FIXME: Use mypaint fringe calculation instead of raw radius */
        int ix_start = (x - radius) / TILE_PIXEL_WIDTH;
        int iy_start = (y - radius) / TILE_PIXEL_HEIGHT;

        int ix_end   = (x + radius) / TILE_PIXEL_WIDTH;
        int iy_end   = (y + radius) / TILE_PIXEL_HEIGHT;

        const size_t global_work_size[2] = {TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH};
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

        err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&stride);
        err = clSetKernelArg(kernel, 4, sizeof(float), (void *)&radius);
        err = clSetKernelArg(kernel, 5, sizeof(float), (void *)&hardness);
        err = clSetKernelArg(kernel, 6, sizeof(float), (void *)&aspect_ratio);
        err = clSetKernelArg(kernel, 7, sizeof(float), (void *)&sn);
        err = clSetKernelArg(kernel, 8, sizeof(float), (void *)&cs);
        err = clSetKernelArg(kernel, 9, sizeof(float), (void *)&slope1);
        err = clSetKernelArg(kernel, 10, sizeof(float), (void *)&slope2);
        err = clSetKernelArg(kernel, 11, sizeof(cl_float4), (void *)&color);

        for (int iy = iy_start; iy <= iy_end; ++iy)
        {
            for (int ix = ix_start; ix <= ix_end; ++ix)
            {
                CanvasTile *tile = ctx->getTile(ix, iy);

                float offsetX = (ix * TILE_PIXEL_WIDTH) + 0.5f - x;
                float offsetY = (iy * TILE_PIXEL_HEIGHT) + 0.5f - y;
                cl_mem data = ctx->clOpenTile(tile);

                err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
                err = clSetKernelArg(kernel, 2, sizeof(float), (void *)&offsetX);
                err = clSetKernelArg(kernel, 3, sizeof(float), (void *)&offsetY);
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
