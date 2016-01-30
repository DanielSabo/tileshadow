#include "canvasstrokepoint.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>

std::vector<CanvasStrokePoint> CanvasStrokePoint::pointsFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument const jsonDoc = QJsonDocument::fromJson(file.readAll());
    if (!jsonDoc.isArray())
        return {};

    return pointsFromJSON(jsonDoc.array());
}

std::vector<CanvasStrokePoint> CanvasStrokePoint::pointsFromJSON(const QJsonArray &json)
{
    std::vector<CanvasStrokePoint> result;
    result.reserve(json.size());

    for (auto const &child: json)
    {
        QJsonArray pointArray = child.toArray();
        float x = pointArray.at(0).toDouble(0.0);
        float y = pointArray.at(1).toDouble(0.0);
        float p = pointArray.at(2).toDouble(1.0);
        float dt = pointArray.at(3).toDouble(15);
        result.push_back({x, y, p, dt});
    }

    return result;
}
