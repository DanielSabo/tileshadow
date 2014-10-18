#include "tiledebugtool.h"
#include "strokecontext.h"
#include "canvastile.h"
#include "paintutils.h"
#include <qmath.h>
#include <QColor>
#include <QVariant>

class TileDebugStrokeContext : public StrokeContext
{
public:
    TileDebugStrokeContext(CanvasLayer *layer, float radius);

    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure, float dt);
    void drawDab(QPointF point, TileSet &modTiles);

    float radius;

    QPointF start;
    QPointF lastDab;
};

TileDebugStrokeContext::TileDebugStrokeContext(CanvasLayer *layer, float radius)
    : StrokeContext(layer), radius(radius)
{

}

void TileDebugStrokeContext::drawDab(QPointF point, TileSet &modTiles)
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

    err = clSetKernelArg<cl_int>(kernel, 1, stride);
    err = clSetKernelArg<cl_float>(kernel, 4, floatRadius);

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            cl_float4 pixel = {1.0f, 1.0f, 1.0f, 1.0f};

            int index = (ix + iy) % 3;
            if (index < 0)
                index = (3 + index) % 3;
            pixel.s[index] = 0.0f;

            cl_int offsetX = point.x() - (ix * TILE_PIXEL_WIDTH);
            cl_int offsetY = point.y() - (iy * TILE_PIXEL_HEIGHT);
            cl_mem data = layer->clOpenTileAt(ix, iy);

            modTiles.insert(QPoint(ix, iy));

            err = clSetKernelArg<cl_mem>(kernel, 0, data);
            err = clSetKernelArg<cl_int>(kernel, 2, offsetX);
            err = clSetKernelArg<cl_int>(kernel, 3, offsetY);
            err = clSetKernelArg<cl_float4>(kernel, 5, pixel);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel, 2,
                                         nullptr, global_work_size, nullptr,
                                         0, nullptr, nullptr);
        }
    }

    (void)err;

    lastDab = point;
}

TileSet TileDebugStrokeContext::startStroke(QPointF point, float pressure)
{
    TileSet modTiles;

    start = point;
    drawDab(point, modTiles);
    return modTiles;
}

TileSet TileDebugStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    TileSet modTiles;

    std::vector<QPoint> line = interpolateLine(lastDab.toPoint(), point.toPoint());

    for (QPoint const &p: line)
        drawDab(p, modTiles);

    return modTiles;
}

class TileDebugToolPrivate
{
public:

    float radius;
    QColor color;
};

TileDebugTool::TileDebugTool() :
    priv(new TileDebugToolPrivate)
{
    reset();
}

TileDebugTool::~TileDebugTool()
{
    delete priv;
}

BaseTool *TileDebugTool::clone()
{
    TileDebugTool *result = new TileDebugTool();
    *result->priv = *priv;

    return result;
}

void TileDebugTool::reset()
{
    priv->color = QColor::fromRgbF(0.0, 0.0, 0.0);
    priv->radius = 10.0f;
}

void TileDebugTool::setToolSetting(QString const &name, QVariant const &value)
{
    if (name == QStringLiteral("size") && value.canConvert<float>())
    {
        float radius = qBound<float>(1.0f, exp(value.toFloat()), 100.0f);

        priv->radius = radius;
    }
    else
    {
        BaseTool::setToolSetting(name, value);
    }
}

QVariant TileDebugTool::getToolSetting(const QString &name)
{
    if (name == QStringLiteral("size"))
    {
        return QVariant::fromValue<float>(log(priv->radius));
    }
    else
    {
        return BaseTool::getToolSetting(name);
    }
}

QList<ToolSettingInfo> TileDebugTool::listToolSettings()
{
    QList<ToolSettingInfo> result;

    result.append(ToolSettingInfo::exponentSlider("size", "SizeExp", 0.0f, 6.0f));

    return result;
}

float TileDebugTool::getPixelRadius()
{
    return priv->radius;
}

void TileDebugTool::setColor(const QColor &color)
{

}

StrokeContext *TileDebugTool::newStroke(CanvasLayer *layer)
{
    TileDebugStrokeContext *result = new TileDebugStrokeContext(layer, priv->radius);

    return result;
}
