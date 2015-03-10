#include "gradienttool.h"
#include <cmath>

class GradientToolPrivate
{
public:
    QColor color;
};

class GradientStrokeContext : public StrokeContext
{
public:
    GradientStrokeContext(CanvasLayer *layer, QColor const &color);
    ~GradientStrokeContext() override;

    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure, float dt);

    std::unique_ptr<CanvasLayer> srcLayer;

    QColor color;
    QPoint start;
    QPoint end;
};

GradientTool::GradientTool()
    : priv(new GradientToolPrivate())
{
    reset();
}

GradientTool::~GradientTool()
{
    delete priv;
}

BaseTool *GradientTool::clone()
{
    GradientTool *clone = new GradientTool();
    *clone->priv = *priv;
    return clone;
}

StrokeContext *GradientTool::newStroke(CanvasLayer *layer)
{
    return new GradientStrokeContext(layer, priv->color);
}

void GradientTool::reset()
{
    priv->color = QColor("red");
}

float GradientTool::getPixelRadius()
{
    return 10.0f;
}

void GradientTool::setColor(const QColor &color)
{
    priv->color = color;
}

bool GradientTool::coalesceMovement()
{
    return true;
}

GradientStrokeContext::GradientStrokeContext(CanvasLayer *layer, const QColor &color)
    : StrokeContext(layer), srcLayer(layer->deepCopy()), color(color)
{
}

GradientStrokeContext::~GradientStrokeContext()
{
}

TileSet GradientStrokeContext::startStroke(QPointF point, float pressure)
{
    start = point.toPoint();
    end = start;

    return TileSet();
}

TileSet GradientStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    end = point.toPoint();
    auto opencl = SharedOpenCL::getSharedOpenCL();
    cl_kernel kernel = opencl->gradientApply;
    const size_t workSize[2] = CL_DIM2(TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH);

    float dx = end.x() - start.x();
    float dy = end.y() - start.y();
    float denom = dx * dx + dy * dy;
    clSetKernelArg<cl_float2>(kernel, 3, {dx / denom, dy / denom});
    clSetKernelArg<cl_float4>(kernel, 4, {(float)color.redF(), (float)color.greenF(), (float)color.blueF(), 1.0f});

    for (auto &iter: *(srcLayer->tiles))
    {
        QPoint const &tileIdx = iter.first;
        CanvasTile *srcTile = iter.second.get();
        CanvasTile *dstTile = layer->getTile(tileIdx.x(), tileIdx.y());

        cl_int originX = tileIdx.x() * TILE_PIXEL_WIDTH;
        cl_int originY = tileIdx.y() * TILE_PIXEL_HEIGHT;

        clSetKernelArg<cl_mem>(kernel, 0, srcTile->unmapHost());
        clSetKernelArg<cl_mem>(kernel, 1, dstTile->unmapHost());
        clSetKernelArg<cl_int2>(kernel, 2, {originX - start.x(), originY - start.y()});
        clEnqueueNDRangeKernel(opencl->cmdQueue,
                               kernel, 2,
                               nullptr, workSize, nullptr,
                               0, nullptr, nullptr);
    }

    return layer->getTileSet();
}
