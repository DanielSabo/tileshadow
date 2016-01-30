#ifndef CANVASSTROKEPOINT_H
#define CANVASSTROKEPOINT_H

#include <vector>

class QString;
class QJsonArray;

struct CanvasStrokePoint
{
    float x;
    float y;
    float p;
    float dt;

    static std::vector<CanvasStrokePoint> pointsFromFile(QString const &filename);
    static std::vector<CanvasStrokePoint> pointsFromJSON(QJsonArray const &json);
};

#endif // CANVASSTROKEPOINT_H
