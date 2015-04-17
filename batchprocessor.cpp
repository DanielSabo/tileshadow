#include "batchprocessor.h"
#include "canvaswidget-opencl.h"
#include "canvasstack.h"
#include "imagefiles.h"
#include "toolfactory.h"
#include <QApplication>
#include <QFile>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>

class BatchCommand
{
public:
    virtual ~BatchCommand() {}
    virtual void apply(CanvasStack *) {}
};

class BatchCommandStroke : public BatchCommand
{
public:
    struct StrokePoint {
        float x;
        float y;
        float pressure;
        float dt;
    };

    BatchCommandStroke(QJsonObject const &json);
    void apply(CanvasStack *stack) override;

    QString toolPath;
    QColor color;
    QMap<QString, QVariant> toolSettings;
    QPointF pointsOffset;
    std::vector<StrokePoint> points;
};

class BatchCommandExport : public BatchCommand
{
public:
    BatchCommandExport(QJsonObject const &json);
    void apply(CanvasStack *stack) override;

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
    for (QJsonValue const &pointIter: pointsArray)
    {
        //FIXME: Error handling for malformed points
        QJsonArray pointObj = pointIter.toArray();
        StrokePoint point = {
            (float)pointObj.at(0).toDouble(),
            (float)pointObj.at(1).toDouble(),
            (float)pointObj.at(2).toDouble(),
            (float)pointObj.at(3).toDouble()
        };
        points.push_back(point);
    }
}

void BatchCommandStroke::apply(CanvasStack *stack)
{
    CanvasLayer *layer = stack->layers.at(0);
    BaseTool *tool = NULL;
    if (!toolPath.isEmpty())
        tool = ToolFactory::loadTool(toolPath);
    if (!tool)
        tool = ToolFactory::loadTool(ToolFactory::defaultToolName());

    if (color.isValid())
        tool->setColor(color);

    for (auto iter = toolSettings.begin(); iter != toolSettings.end(); ++iter)
        tool->setToolSetting(iter.key(), iter.value());

    StrokeContextArgs args = {layer};
    std::unique_ptr<StrokeContext> stroke(tool->newStroke(args));

    auto pointsIter = points.begin();

    if (pointsIter != points.end())
    {
        QPointF xy(pointsIter->x, pointsIter->y);
        stroke->startStroke(xy + pointsOffset, pointsIter->pressure);
        ++pointsIter;
    }

    while (pointsIter != points.end())
    {
        QPointF xy(pointsIter->x, pointsIter->y);
        stroke->strokeTo(xy + pointsOffset, pointsIter->pressure, pointsIter->dt);
        ++pointsIter;
    }

    stroke.reset();
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

void BatchCommandExport::apply(CanvasStack *stack)
{
    QImage output = stackToImage(stack);
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
            else if (commandName == QStringLiteral("export"))
            {
                commandList.emplace_back(new BatchCommandExport(commandObject));
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
        CanvasStack layers;
        layers.newLayerAt(0);

        for (auto &commandIter: commandList)
            commandIter->apply(&layers);
    }

    QApplication::quit();
}
