#include "patternfilltool.h"
#include "strokecontext.h"
#include "canvastile.h"
#include "paintutils.h"
#include <qmath.h>
#include <QImage>
#include <QVariant>
#include <QDir>
#include <QDebug>

class PatternFillStrokeContext : public StrokeContext
{
public:
    PatternFillStrokeContext(CanvasLayer *layer, float radius, const QImage &image);
    ~PatternFillStrokeContext() override;

    TileSet startStroke(QPointF point, float pressure) override;
    TileSet strokeTo(QPointF point, float pressure, float dt) override;
    void drawDab(QPointF point, TileSet &modTiles);

    float radius;
    cl_mem pattern;

    QPoint lastDab;
};

PatternFillStrokeContext::PatternFillStrokeContext(CanvasLayer *layer, float radius, QImage const &image)
    : StrokeContext(layer), radius(radius), pattern(0)
{
    {
        cl_int err = CL_SUCCESS;
        cl_image_format fmt = {CL_RGBA, CL_UNORM_INT8};

        pattern = clCreateImage2D(SharedOpenCL::getSharedOpenCL()->ctx,
                                  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                  &fmt,
                                  image.width(), image.height(), image.bytesPerLine(),
                                  (void *)image.bits(), &err);
        check_cl_error(err);

        if (err != CL_SUCCESS)
            pattern = 0;
    }
}

PatternFillStrokeContext::~PatternFillStrokeContext()
{
    if (pattern)
        clReleaseMemObject(pattern);
}

void PatternFillStrokeContext::drawDab(QPointF point, TileSet &modTiles)
{
    if (!pattern)
        return;

    cl_int intRadius = ceil(radius);
    cl_float floatRadius = radius;
    int ix_start = tile_indice(point.x() - intRadius, TILE_PIXEL_WIDTH);
    int iy_start = tile_indice(point.y() - intRadius, TILE_PIXEL_HEIGHT);

    int ix_end   = tile_indice(point.x() + intRadius, TILE_PIXEL_WIDTH);
    int iy_end   = tile_indice(point.y() + intRadius, TILE_PIXEL_HEIGHT);

    const size_t global_work_size[2] = {TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH};
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->patternFill_fillCircle;

    cl_int err = CL_SUCCESS;

    err = clSetKernelArg<cl_float>(kernel, 5, floatRadius);
    err = clSetKernelArg<cl_mem>(kernel, 6, pattern);

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            cl_int offsetX = point.x() - (ix * TILE_PIXEL_WIDTH);
            cl_int offsetY = point.y() - (iy * TILE_PIXEL_HEIGHT);
            cl_mem data = layer->clOpenTileAt(ix, iy);

            modTiles.insert(QPoint(ix, iy));

            err = clSetKernelArg<cl_mem>(kernel, 0, data);
            err = clSetKernelArg<cl_int>(kernel, 1, offsetX);
            err = clSetKernelArg<cl_int>(kernel, 2, offsetY);
            err = clSetKernelArg<cl_int>(kernel, 3, ix * TILE_PIXEL_WIDTH);
            err = clSetKernelArg<cl_int>(kernel, 4, iy * TILE_PIXEL_HEIGHT);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel, 2,
                                         nullptr, global_work_size, nullptr,
                                         0, nullptr, nullptr);
        }
    }

    (void)err;
}

TileSet PatternFillStrokeContext::startStroke(QPointF point, float pressure)
{
    TileSet modTiles;

    drawDab(point, modTiles);
    lastDab = point.toPoint();
    return modTiles;
}

TileSet PatternFillStrokeContext::strokeTo(QPointF point, float pressure, float dt)
{
    TileSet modTiles;

    std::vector<QPoint> line = interpolateLine(lastDab, point.toPoint());

    for (QPoint const &p: line)
    {
        if (dist(lastDab, p) >= radius * 0.1f)
        {
            drawDab(p, modTiles);
            lastDab = point.toPoint();
        }
    }

    return modTiles;
}

class PatternFillToolPrivate
{
public:
    float radiusExp;
    QImage image;
    QStringList patterns;
    QString selectedPattern;

    void loadImage();
};

void PatternFillToolPrivate::loadImage()
{
    image = QImage(1, 1, QImage::Format_RGBA8888);
    image.fill(Qt::blue);

    if (!patterns.empty() && !selectedPattern.isEmpty())
    {
        QImage loadedImage = QImage(selectedPattern);
        if (!loadedImage.isNull())
            image = loadedImage.convertToFormat(QImage::Format_RGBA8888);
    }
}

PatternFillTool::PatternFillTool(QStringList const &patternPaths) :
    priv(new PatternFillToolPrivate)
{
    priv->patterns = QStringList();

    for (QString const &patternPath: patternPaths)
    {
        QFileInfoList infoList = QDir(patternPath).entryInfoList({"*.png"}, QDir::NoDotAndDotDot | QDir::Files);
        for (QFileInfo const &info: infoList)
        {
            priv->patterns << info.filePath();
        }
    }

    reset();
}


PatternFillTool::PatternFillTool(PatternFillTool const &from) :
    priv(new PatternFillToolPrivate)
{
    *priv = *from.priv;
}

PatternFillTool::~PatternFillTool()
{
    delete priv;
}

BaseTool *PatternFillTool::clone()
{
    return new PatternFillTool(*this);
}

void PatternFillTool::reset()
{
    if (priv->patterns.empty())
        priv->selectedPattern = QString();
    else
        priv->selectedPattern = priv->patterns.first();
    priv->radiusExp = 3.0f;
    priv->loadImage();
}

void PatternFillTool::setToolSetting(QString const &name, QVariant const &value)
{
    if (name == QStringLiteral("size") && value.canConvert<float>())
    {
        priv->radiusExp = qBound<float>(1.0f, value.toFloat(), 6.0f);;
    }
    else if (name == QStringLiteral("pattern-path"))
    {
        priv->selectedPattern = value.toString();
        priv->loadImage();
    }
    else
    {
        BaseTool::setToolSetting(name, value);
    }
}

QVariant PatternFillTool::getToolSetting(const QString &name)
{
    if (name == QStringLiteral("size"))
    {
        return QVariant::fromValue<float>(priv->radiusExp);
    }
    else if (name == QStringLiteral("pattern-path"))
    {
        return QVariant::fromValue<QString>(priv->selectedPattern);
    }
    else
    {
        return BaseTool::getToolSetting(name);
    }
}

QList<ToolSettingInfo> PatternFillTool::listToolSettings()
{
    QList<ToolSettingInfo> result;
    decltype(ToolSettingInfo::options) optionList;
    for (QString const& patternPath: priv->patterns)
        optionList.append({patternPath, QFileInfo(patternPath).fileName()});

    result.append(ToolSettingInfo::exponentSlider("size", "Size", 0.0f, 6.0f));
    result.append(ToolSettingInfo::listBox("pattern-path", "Pattern", optionList));

    return result;
}

float PatternFillTool::getPixelRadius()
{
    return exp(priv->radiusExp);
}

void PatternFillTool::setColor(const QColor &color)
{

}

StrokeContext *PatternFillTool::newStroke(const StrokeContextArgs &args)
{
    PatternFillStrokeContext *result = new PatternFillStrokeContext(args.layer, exp(priv->radiusExp), priv->image);

    return result;
}

