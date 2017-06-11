#include "batchprocessor.h"
#include "canvaswidget-opencl.h"
#include "canvasstack.h"
#include "canvasstrokepoint.h"
#include "imagefiles.h"
#include "ora.h"
#include "toolfactory.h"
#include <QApplication>
#include <QFile>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>

struct BatchProcessorContext {
    CanvasStack layers;
    std::unique_ptr<CanvasLayer> currentLayerCopy;
};

class BatchCommand
{
public:
    virtual ~BatchCommand() {}
    virtual void apply(BatchProcessorContext *) {}
};

class BatchCommandStroke : public BatchCommand
{
public:
    BatchCommandStroke(QJsonObject const &json);
    void apply(BatchProcessorContext *ctx) override;

    QString toolPath;
    QColor color;
    QMap<QString, QVariant> toolSettings;
    QPointF pointsOffset;
    std::vector<CanvasStrokePoint> points;
};

class BatchCommandRotate : public BatchCommand
{
public:
    BatchCommandRotate(QJsonObject const &json);
    void apply(BatchProcessorContext *ctx) override;

    QPoint origin;
    double angle = 0.0;
    QPointF offset;
};

class BatchCommandScale : public BatchCommand
{
public:
    BatchCommandScale(QJsonObject const &json);
    void apply(BatchProcessorContext *ctx) override;

    QPoint origin;
    QPointF scale;
    QPointF offset;
};

class BatchCommandExport : public BatchCommand
{
public:
    BatchCommandExport(QJsonObject const &json);
    void apply(BatchProcessorContext *ctx) override;

    QString path;
};

class BatchCommandOpen : public BatchCommand
{
public:
    BatchCommandOpen(QJsonObject const &json);
    void apply(BatchProcessorContext *ctx) override;

    QString path;
};

BatchCommandStroke::BatchCommandStroke(const QJsonObject &json)
{
    if (!json.contains("points") || !json["points"].isArray())
        throw QString("Stroke command without points");

    toolPath = json["path"].toString();

    QJsonArray const &offsetObj = json["offset"].toArray();
    if (offsetObj.size() == 2)
    {
        pointsOffset.rx() = offsetObj.at(0).toDouble();
        pointsOffset.ry() = offsetObj.at(1).toDouble();
    }

    QJsonArray const &colorObj = json["color"].toArray();
    if (colorObj.size() == 3)
    {
        float r = colorObj.at(0).toDouble();
        float g = colorObj.at(1).toDouble();
        float b = colorObj.at(2).toDouble();
        color = QColor::fromRgbF(r, g, b);
    }

    QJsonObject const &settingsObj = json["settings"].toObject();
    for (auto iter = settingsObj.begin(); iter != settingsObj.end(); ++iter)
    {
        toolSettings[iter.key()] = iter.value().toVariant();
    }

    QJsonArray const &pointsArray = json["points"].toArray();
    points = CanvasStrokePoint::pointsFromJSON(pointsArray);
}

void BatchCommandStroke::apply(BatchProcessorContext *ctx)
{
    CanvasLayer *layer = ctx->layers.layers.at(0);
    std::unique_ptr<BaseTool> tool;
    if (!toolPath.isEmpty())
        tool = ToolFactory::loadTool(toolPath);
    if (!tool)
        tool = ToolFactory::loadTool(ToolFactory::defaultToolName());

    if (color.isValid())
        tool->setColor(color);

    for (auto iter = toolSettings.begin(); iter != toolSettings.end(); ++iter)
        tool->setToolSetting(iter.key(), iter.value());

    StrokeContextArgs args = {layer, ctx->currentLayerCopy.get()};
    std::unique_ptr<StrokeContext> stroke(tool->newStroke(args));

    auto pointsIter = points.begin();

    if (pointsIter != points.end())
    {
        QPointF xy(pointsIter->x, pointsIter->y);
        stroke->startStroke(xy + pointsOffset, pointsIter->p);
        ++pointsIter;
    }

    while (pointsIter != points.end())
    {
        QPointF xy(pointsIter->x, pointsIter->y);
        stroke->strokeTo(xy + pointsOffset, pointsIter->p, pointsIter->dt);
        ++pointsIter;
    }

    stroke.reset();

    //FIXME: Copy only modified tiles
    ctx->currentLayerCopy = ctx->layers.layers.at(0)->deepCopy();
}

BatchCommandRotate::BatchCommandRotate(const QJsonObject &json)
{
    angle = json["angle"].toDouble(0.0);

    QJsonArray jsonOrigin = json["origin"].toArray();
    if (jsonOrigin.size() == 2)
    {
        origin.rx() = jsonOrigin.at(0).toInt();
        origin.ry() = jsonOrigin.at(1).toInt();
    }
    else
    {
        throw QString("Invalid origin for rotatation");
    }

    QJsonArray jsonOffset = json["offset"].toArray();
    if (jsonOffset.size() == 2)
    {
        offset.rx() = jsonOffset.at(0).toDouble();
        offset.ry() = jsonOffset.at(1).toDouble();
    }
    else if (jsonOffset.size() != 0)
    {
        throw QString("Invalid offset for rotatation");
    }
}

void BatchCommandRotate::apply(BatchProcessorContext *ctx)
{
    QMatrix transform;
    transform.translate(offset.x(), offset.y());
    transform.translate(origin.x(), origin.y());
    transform.rotate(angle);
    transform.translate(-origin.x(), -origin.y());

    CanvasLayer *layer = ctx->layers.layers.at(0);
    std::unique_ptr<CanvasLayer> rotated = layer->applyMatrix(transform);
    layer->takeTiles(rotated.get());
    layer->prune();
    ctx->currentLayerCopy = ctx->layers.layers.at(0)->deepCopy();
}

BatchCommandScale::BatchCommandScale(const QJsonObject &json)
{
    QJsonArray jsonOrigin = json["origin"].toArray();
    if (jsonOrigin.size() == 2)
    {
        origin.rx() = jsonOrigin.at(0).toInt();
        origin.ry() = jsonOrigin.at(1).toInt();
    }
    else
    {
        throw QString("Invalid origin for scale");
    }

    QJsonArray jsonScale = json["scale"].toArray();
    if (jsonScale.size() == 2)
    {
        scale.rx() = jsonScale.at(0).toDouble();
        scale.ry() = jsonScale.at(1).toDouble();
    }
    else
    {
        throw QString("Invalid ratio for scale");
    }

    QJsonArray jsonOffset = json["offset"].toArray();
    if (jsonOffset.size() == 2)
    {
        offset.rx() = jsonOffset.at(0).toDouble();
        offset.ry() = jsonOffset.at(1).toDouble();
    }
    else if (jsonOffset.size() != 0)
    {
        throw QString("Invalid offset for scale");
    }
}

void BatchCommandScale::apply(BatchProcessorContext *ctx)
{
    QMatrix transform;
    transform.translate(offset.x(), offset.y());
    transform.translate(origin.x(), origin.y());
    transform.scale(scale.x(), scale.y());
    transform.translate(-origin.x(), -origin.y());

    CanvasLayer *layer = ctx->layers.layers.at(0);
    std::unique_ptr<CanvasLayer> scaled = layer->applyMatrix(transform);
    layer->takeTiles(scaled.get());
    layer->prune();
    ctx->currentLayerCopy = ctx->layers.layers.at(0)->deepCopy();
}

BatchCommandExport::BatchCommandExport(const QJsonObject &json)
{
    if (!json.contains("path"))
        throw QString("Export command without path");

    QString pathStr = json["path"].toString();

    if (pathStr.isEmpty())
        throw QString("Export command with invalid path");

    path = pathStr;
}

void BatchCommandExport::apply(BatchProcessorContext *ctx)
{
    if (path.endsWith(".ora"))
    {
        saveStackAs(&ctx->layers, QRect(), path);
    }
    else
    {
        QImage output = stackToImage(&ctx->layers);
        if (output.isNull())
        {
            qWarning() << "Failed to export, image is empty";
            return;
        }

        QImageWriter writer(path);
        if (path.endsWith(".jpg", Qt::CaseInsensitive) || path.endsWith(".jpeg", Qt::CaseInsensitive))
            writer.setQuality(90);
        else if (path.endsWith(".png", Qt::CaseInsensitive))
            writer.setQuality(9);

        if (!writer.write(output))
        {
            qWarning() << "Export failed" << writer.errorString();
        }
    }
}

BatchCommandOpen::BatchCommandOpen(const QJsonObject &json)
{
    if (!json.contains("path"))
        throw QString("Open command without path");

    QString pathStr = json["path"].toString();

    if (pathStr.isEmpty() || !QFile::exists(pathStr))
        throw QString("Open command with invalid path");

    path = pathStr;
}

void BatchCommandOpen::apply(BatchProcessorContext *ctx)
{
    if (path.endsWith(".ora"))
    {
        loadStackFromORA(&ctx->layers, nullptr, path);
    }
    else
    {
        QImage image(path);
        if (!image.isNull())
        {
            ctx->layers.clearLayers();
            std::unique_ptr<CanvasLayer> imageLayer = layerFromImage(image);
            imageLayer->name = path;
            ctx->layers.layers.append(imageLayer.release());
        }
        else
        {
            qWarning() << "Failed to load" << path << ": invalid file format";
        }
    }
}

BatchProcessor::BatchProcessor(QObject *parent) :
    QObject(parent)
{
}

void BatchProcessor::execute(QString path)
{
    QFile file(path);
    std::vector<std::unique_ptr<BatchCommand>> commandList;

    try
    {
        if (!file.open(QIODevice::ReadOnly))
            throw QString("Failed to open file: ") + file.errorString();

        QByteArray source = file.readAll();
        if (source.isNull())
            throw QString("Failed to read file");

        QJsonDocument const batchJsonDoc = QJsonDocument::fromJson(source);
        if (!batchJsonDoc.isArray())
            throw QString("JSON parse failed");

        for (QJsonValue const &iterValue: batchJsonDoc.array())
        {
            QJsonObject const &commandObject = iterValue.toObject();
            QString commandName = commandObject["command"].toString();

            if (commandName.isEmpty())
            {
                throw QString("Missing command");
            }
            else if (commandName == QStringLiteral("stroke"))
            {
                commandList.emplace_back(new BatchCommandStroke(commandObject));
            }
            else if (commandName == QStringLiteral("rotate"))
            {
                commandList.emplace_back(new BatchCommandRotate(commandObject));
            }
            else if (commandName == QStringLiteral("scale"))
            {
                commandList.emplace_back(new BatchCommandScale(commandObject));
            }
            else if (commandName == QStringLiteral("export"))
            {
                commandList.emplace_back(new BatchCommandExport(commandObject));
            }
            else if (commandName == QStringLiteral("open"))
            {
                commandList.emplace_back(new BatchCommandOpen(commandObject));
            }
            else
            {
                throw QString("Invalid command: ") + commandName;
            }
        }
    }
    catch (QString err)
    {
        qWarning() << path << err;
        QApplication::quit();
        return;
    }

    qDebug() << "Parsed" << commandList.size() << "batch commands.";

    if (!commandList.empty())
    {
        SharedOpenCL::getSharedOpenCL();
        BatchProcessorContext ctx;
        ctx.layers.layers.append(new CanvasLayer());
        ctx.currentLayerCopy = ctx.layers.layers.at(0)->deepCopy();

        for (auto &commandIter: commandList)
            commandIter->apply(&ctx);
    }

    QApplication::quit();
}
