#include "roundbrushtool.h"
#include "canvastile.h"
#include "paintutils.h"
#include <qmath.h>
#include <memory>
#include <QColor>
#include <QVariant>

template<typename T> static inline T pow2(T const &v)
{
    return v * v;
}

static inline float dist(QPoint a, QPoint b)
{
    return sqrt(pow2((a.x() - b.x())) + pow2((a.y() - b.y())));
}

static inline float dist(QPointF a, QPointF b)
{
    return sqrt(pow2((a.x() - b.x())) + pow2((a.y() - b.y())));
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

    for (auto iter: srcTiles)
    {
        if(iter.second)
            delete iter.second;
    }
}

void RoundBrushStrokeContext::drawDab(QPointF point, float pressure, TileSet &modTiles)
{
    float mapped_alpha = pressure * pressure * a;
          mapped_alpha = mapped_alpha * (1.0f - minAlphaCoef) + a * minAlphaCoef;
    float mapped_radius = (1.0f - (pressure - 1.0f) * (pressure - 1.0f)) * radius;
          mapped_radius = mapped_radius * (1.0f - minRadiusCoef) + radius * minRadiusCoef;

    if (firstDab || dist(lastDab, point) > mapped_radius * rateCoefficient || mapped_radius < 5.0f)
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

    clSetKernelArg(circleKernel, 3, sizeof(cl_float), (void *)&mapped_radius);

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

                clSetKernelArg(fillKernel, 0, sizeof(cl_mem), (void *)&drawMem);
                clSetKernelArg(fillKernel, 1, sizeof(float), (void *)&value);
                clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                       fillKernel, 1,
                                       nullptr, &maskComps, nullptr,
                                       0, nullptr, nullptr);
            }

            modTiles.insert(QPoint(ix, iy));

            cl_float floatAlpha = mapped_alpha;

            clSetKernelArg(circleKernel, 0, sizeof(cl_mem), (void *)&drawMem);
            clSetKernelArg(circleKernel, 1, sizeof(cl_int), (void *)&offsetX);
            clSetKernelArg(circleKernel, 2, sizeof(cl_int), (void *)&offsetY);
            clSetKernelArg(circleKernel, 4, sizeof(cl_float), (void *)&floatAlpha);
            clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                   circleKernel, 2,
                                   nullptr, circleWorkSize, nullptr,
                                   0, nullptr, nullptr);
        }
    }
}

void RoundBrushStrokeContext::applyLayer(const TileSet &modTiles)
{
    float pixel[4] = {r, g, b, 1.0f};
    const size_t blendWorkSize[1]  = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
    cl_kernel blendKernel = SharedOpenCL::getSharedOpenCL()->paintKernel_applyMaskTile;

    for (QPoint const &tilePos: modTiles)
    {
        CanvasTile *dstTile = layer->getTile(tilePos.x(), tilePos.y());
        CanvasTile *&srcTile = srcTiles[tilePos];
        cl_mem drawMem = drawTiles[tilePos];

        if (!srcTile)
            srcTile = dstTile->copy();

        cl_mem srcMem = srcTile->unmapHost();
        cl_mem dstMem = dstTile->unmapHost();

        clSetKernelArg(blendKernel, 0, sizeof(cl_mem), (void *)&dstMem);
        clSetKernelArg(blendKernel, 1, sizeof(cl_mem), (void *)&srcMem);
        clSetKernelArg(blendKernel, 2, sizeof(cl_mem), (void *)&drawMem);
        clSetKernelArg(blendKernel, 3, sizeof(cl_float4), (void *)&pixel);
        clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                               blendKernel, 1,
                               nullptr, blendWorkSize, nullptr,
                               0, nullptr, nullptr);
    }
}

TileSet RoundBrushStrokeContext::startStroke(QPointF point, float pressure)
{
    TileSet modTiles;

    lastPoint = point.toPoint();
    lastPressure = pressure;
    drawDab(point, pressure, modTiles);
    applyLayer(modTiles);
    return modTiles;
}

TileSet RoundBrushStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    TileSet modTiles;

    QPoint thisPoint = point.toPoint();

    if (thisPoint == lastPoint)
    {
        drawDab(point, pressure, modTiles);
    }
    else
    {
        std::vector<QPoint> line = interpolateLine(lastPoint, thisPoint);

        float lineLength = dist(lastPoint, thisPoint);
        float deltaPressure = pressure - lastPressure;

        for (QPoint const &p: line)
        {
            float linePressure = lastPressure + deltaPressure * (dist(lastPoint, p) / lineLength);
            drawDab(p, linePressure, modTiles);
        }

        lastPoint = thisPoint;
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
    priv->minRadiusCoef = 0.0f;
    priv->minAlphaCoef = 0.0f;
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
