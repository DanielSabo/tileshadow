#include "roundbrushtool.h"
#include "canvastile.h"
#include "paintutils.h"
#include <qmath.h>
#include <memory>
#include <QColor>
#include <QVariant>

static const float SUBPIXEL_FACTOR = 8.0f;

template<typename T> static inline float dist(T a, T b)
{
    float dx = a.x() - b.x();
    float dy = a.y() - b.y();

    return sqrt(dx * dx + dy * dy);
}

class RoundBrushStrokeContext : public StrokeContext
{
public:
    RoundBrushStrokeContext(CanvasLayer *layer);
    ~RoundBrushStrokeContext() override;
    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure, float dt);
    void drawDab(QPointF point, float pressure, TileSet &modTiles);
    void applyLayer(TileSet const &modTiles);

    TileMap srcTiles;
    std::map<QPoint, cl_mem, _tilePointCompare> drawTiles;

    float radius;

    float lastPressure;
    QPoint lastPoint;
    bool firstDab;
    QPointF lastDab;
    float rateCoefficient;

    float r;
    float g;
    float b;
    float a;

    float minRadiusCoef;
    float minAlphaCoef;
};

RoundBrushStrokeContext::RoundBrushStrokeContext(CanvasLayer *layer)
    : StrokeContext(layer)
{
    firstDab = true;
    rateCoefficient = 0.1f;
}

RoundBrushStrokeContext::~RoundBrushStrokeContext()
{
    for (auto iter: drawTiles)
    {
        if(iter.second)
            clReleaseMemObject(iter.second);
    }
}

void RoundBrushStrokeContext::drawDab(QPointF point, float pressure, TileSet &modTiles)
{
    float mapped_alpha = pressure * pressure * a;
          mapped_alpha = mapped_alpha * (1.0f - minAlphaCoef) + a * minAlphaCoef;
    float mapped_radius = (1.0f - (pressure - 1.0f) * (pressure - 1.0f)) * radius;
          mapped_radius = mapped_radius * (1.0f - minRadiusCoef) + radius * minRadiusCoef;

    if (firstDab || dist(lastDab, point) > mapped_radius * rateCoefficient || mapped_radius < 2.0f)
        firstDab = false;
    else
        return;

    lastDab = point;

    cl_int intRadius = ceil(mapped_radius);
    int ix_start = tile_indice(point.x() - intRadius, TILE_PIXEL_WIDTH);
    int iy_start = tile_indice(point.y() - intRadius, TILE_PIXEL_HEIGHT);

    int ix_end   = tile_indice(point.x() + intRadius, TILE_PIXEL_WIDTH);
    int iy_end   = tile_indice(point.y() + intRadius, TILE_PIXEL_HEIGHT);

    const size_t circleWorkSize[2] = {TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH};
    cl_kernel circleKernel = SharedOpenCL::getSharedOpenCL()->paintKernel_maskCircle;
    cl_kernel fillKernel = SharedOpenCL::getSharedOpenCL()->paintKernel_fillFloats;

    clSetKernelArg<cl_float>(circleKernel, 3, mapped_radius);

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            cl_int offsetX = point.x() - (ix * TILE_PIXEL_WIDTH);
            cl_int offsetY = point.y() - (iy * TILE_PIXEL_HEIGHT);

            cl_mem &drawMem = drawTiles[QPoint(ix, iy)];

            if (!drawMem)
            {
                const size_t maskComps = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT;

                drawMem = clCreateBuffer(SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE,
                                         maskComps * sizeof(float), nullptr, nullptr);
                float value = 0;

                clSetKernelArg<cl_mem>(fillKernel, 0, drawMem);
                clSetKernelArg<float>(fillKernel, 1, value);
                clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                       fillKernel, 1,
                                       nullptr, &maskComps, nullptr,
                                       0, nullptr, nullptr);
            }

            modTiles.insert(QPoint(ix, iy));

            clSetKernelArg<cl_mem>(circleKernel, 0, drawMem);
            clSetKernelArg<cl_int>(circleKernel, 1, offsetX);
            clSetKernelArg<cl_int>(circleKernel, 2, offsetY);
            clSetKernelArg<cl_float>(circleKernel, 4, mapped_alpha);
            clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                   circleKernel, 2,
                                   nullptr, circleWorkSize, nullptr,
                                   0, nullptr, nullptr);
        }
    }
}

void RoundBrushStrokeContext::applyLayer(const TileSet &modTiles)
{
    cl_float4 pixel = {r, g, b, 1.0f};
    const size_t blendWorkSize[1]  = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
    cl_kernel blendKernel = SharedOpenCL::getSharedOpenCL()->paintKernel_applyMaskTile;

    for (QPoint const &tilePos: modTiles)
    {
        CanvasTile *dstTile = layer->getTile(tilePos.x(), tilePos.y());
        std::unique_ptr<CanvasTile> &srcTile = srcTiles[tilePos];
        cl_mem drawMem = drawTiles[tilePos];

        if (!srcTile)
            srcTile = dstTile->copy();

        cl_mem srcMem = srcTile->unmapHost();
        cl_mem dstMem = dstTile->unmapHost();

        clSetKernelArg<cl_mem>(blendKernel, 0, dstMem);
        clSetKernelArg<cl_mem>(blendKernel, 1, srcMem);
        clSetKernelArg<cl_mem>(blendKernel, 2, drawMem);
        clSetKernelArg<cl_float4>(blendKernel, 3, pixel);
        clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                               blendKernel, 1,
                               nullptr, blendWorkSize, nullptr,
                               0, nullptr, nullptr);
    }
}

TileSet RoundBrushStrokeContext::startStroke(QPointF point, float pressure)
{
    TileSet modTiles;

    lastPoint = QPoint(round(point.x() * SUBPIXEL_FACTOR), round(point.y() * SUBPIXEL_FACTOR));
    lastPressure = pressure;
    drawDab(QPointF(lastPoint) / SUBPIXEL_FACTOR, pressure, modTiles);
    applyLayer(modTiles);
    return modTiles;
}

TileSet RoundBrushStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    TileSet modTiles;

    QPoint endPoint(round(point.x() * SUBPIXEL_FACTOR), round(point.y() * SUBPIXEL_FACTOR));

    if (endPoint == lastPoint)
    {
        drawDab(point, pressure, modTiles);
    }
    else
    {
        std::vector<QPoint> line = interpolateLine(lastPoint, endPoint);

        float lineLength = dist(lastPoint, endPoint);
        float deltaPressure = pressure - lastPressure;

        for (QPoint const &p: line)
        {
            float linePressure = lastPressure + deltaPressure * (dist(lastPoint, p) / lineLength);
            drawDab(QPointF(p) / SUBPIXEL_FACTOR, linePressure, modTiles);
        }

        lastPoint = endPoint;
    }

    lastPressure = pressure;

    applyLayer(modTiles);

    return modTiles;
}

class RoundBrushToolPrivate
{
public:
    float radius;
    float r;
    float g;
    float b;
    float a;

    float minRadiusCoef;
    float minAlphaCoef;
};

RoundBrushTool::RoundBrushTool() : priv(new RoundBrushToolPrivate())
{
    reset();
}

RoundBrushTool::~RoundBrushTool()
{
    delete priv;
}

BaseTool *RoundBrushTool::clone()
{
    RoundBrushTool *result = new RoundBrushTool();
    *result->priv = *priv;

    return result;
}


void RoundBrushTool::reset()
{
    priv->radius = 10.0f;
    priv->r = 0.0f;
    priv->g = 0.0f;
    priv->b = 0.0f;
    priv->a = 1.0f;
    priv->minRadiusCoef = 0.4f;
    priv->minAlphaCoef = 1.0f;
}

float RoundBrushTool::getPixelRadius()
{
    return priv->radius;
}

void RoundBrushTool::setColor(const QColor &color)
{
    priv->r = color.redF();
    priv->g = color.greenF();
    priv->b = color.blueF();
}

void RoundBrushTool::setToolSetting(const QString &name, const QVariant &value)
{
    if (name == QStringLiteral("size") && value.canConvert<float>())
    {
        priv->radius = qBound<float>(1.0f, exp(value.toFloat()), 500.0f);
    }
    else if (name == QStringLiteral("opacity") && value.canConvert<float>())
    {
        priv->a = qBound<float>(0.0f, value.toFloat(), 1.0f);
    }
    else if (name == QStringLiteral("min-size") && value.canConvert<float>())
    {
        priv->minRadiusCoef = qBound<float>(0.0f, value.toFloat(), 1.0f);
    }
    else if (name == QStringLiteral("min-opacity") && value.canConvert<float>())
    {
        priv->minAlphaCoef = qBound<float>(0.0f, value.toFloat(), 1.0f);
    }
    else
    {
        BaseTool::setToolSetting(name, value);
    }
}

QVariant RoundBrushTool::getToolSetting(const QString &name)
{
    if (name == QStringLiteral("size"))
    {
        return QVariant::fromValue<float>(log(priv->radius));
    }
    else if (name == QStringLiteral("opacity"))
    {
        return QVariant::fromValue<float>(priv->a);
    }
    else if (name == QStringLiteral("min-size"))
    {
        return QVariant::fromValue<float>(priv->minRadiusCoef);
    }
    else if (name == QStringLiteral("min-opacity"))
    {
        return QVariant::fromValue<float>(priv->minAlphaCoef);
    }
    else
    {
        return BaseTool::getToolSetting(name);
    }
}

QList<ToolSettingInfo> RoundBrushTool::listToolSettings()
{
    QList<ToolSettingInfo> result;

    result.append(ToolSettingInfo::exponentSlider("size", "SizeExp", 0.0f, 6.0f));
    result.append(ToolSettingInfo::exponentSlider("min-size", "Min Size (%)", 0.0f, 1.0f));
    result.append(ToolSettingInfo::linearSlider("opacity", "Opacity", 0.0f, 1.0f));
    result.append(ToolSettingInfo::exponentSlider("min-opacity", "Min Opacity (%)", 0.0f, 1.0f));

    return result;
}

StrokeContext *RoundBrushTool::newStroke(CanvasLayer *layer)
{
    RoundBrushStrokeContext *result = new RoundBrushStrokeContext(layer);
    result->radius = priv->radius;
    result->r = priv->r;
    result->g = priv->g;
    result->b = priv->b;
    result->a = priv->a;

    result->minRadiusCoef = priv->minRadiusCoef;
    result->minAlphaCoef  = priv->minAlphaCoef;

    return result;
}
