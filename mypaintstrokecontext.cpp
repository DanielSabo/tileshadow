#include "mypaintstrokecontext.h"
#include <QElapsedTimer>
#include <cstring>
#include <iostream>
#include <cmath>

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

MyPaintStrokeContext::MyPaintStrokeContext(CanvasContext *ctx) : StrokeContext(ctx)
{
    priv = new MyPaintStrokeContextPrivate();

    priv->brush = mypaint_brush_new();
    mypaint_brush_from_defaults(priv->brush);
    mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_H, 0.0);
    mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_S, 1.0);
    mypaint_brush_set_base_value(priv->brush, MYPAINT_BRUSH_SETTING_COLOR_V, 1.0);

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

bool MyPaintStrokeContext::startStroke(QPointF point)
{
    mypaint_brush_new_stroke(priv->brush);
    mypaint_brush_stroke_to(priv->brush, (MyPaintSurface *)priv->surface,
                            point.x(), point.y(),
                            1.0f /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
                            0.0 /* deltaTime in ms*/);
    priv->timer.start();
    return true;
}


bool MyPaintStrokeContext::strokeTo(QPointF point)
{
    double dt = priv->timer.restart() / 1000.0;
    mypaint_brush_stroke_to(priv->brush, (MyPaintSurface *)priv->surface,
                            point.x(), point.y(),
                            1.0f /* pressure */, 0.0f /* xtilt */, 0.0f /* ytilt */,
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

    (void)lock_alpha;
    (void)colorize;

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
        float segments[4] =
            {
                1.0f, -(1.0f / hardness - 1.0f),
                hardness / (1.0f - hardness), -hardness / (1.0f - hardness),
            };
        float color[4] = {color_r, color_g, color_b, color_a};

        float angle_rad = angle / 360 * 2 * M_PI;
        float cs = cosf(angle_rad);
        float sn = sinf(angle_rad);

        err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&stride);
        err = clSetKernelArg(kernel, 4, sizeof(float), (void *)&radius);
        err = clSetKernelArg(kernel, 5, sizeof(float), (void *)&hardness);
        err = clSetKernelArg(kernel, 6, sizeof(float), (void *)&aspect_ratio);
        err = clSetKernelArg(kernel, 7, sizeof(float), (void *)&angle);
        err = clSetKernelArg(kernel, 8, sizeof(float), (void *)&sn);
        err = clSetKernelArg(kernel, 9, sizeof(float), (void *)&cs);
        err = clSetKernelArg(kernel, 10, sizeof(cl_float4), (void *)&segments);
        err = clSetKernelArg(kernel, 11, sizeof(cl_float4), (void *)&color);

        for (int iy = iy_start; iy <= iy_end; ++iy)
        {
            for (int ix = ix_start; ix <= ix_end; ++ix)
            {
                CanvasTile *tile = ctx->getTile(ix, iy);

                float offsetX = x - (ix * TILE_PIXEL_WIDTH);
                float offsetY = y - (iy * TILE_PIXEL_HEIGHT);
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