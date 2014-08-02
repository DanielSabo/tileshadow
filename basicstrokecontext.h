#ifndef BASICSTROKECONTEXT_H
#define BASICSTROKECONTEXT_H

#include "strokecontext.h"

class BasicStrokeContext : public StrokeContext
{
public:
    BasicStrokeContext(CanvasLayer *layer, float radius);

    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure, float dt);
    void drawDab(QPointF point, TileSet &modTiles);

    float radius;

    QPointF start;
    QPointF lastDab;
};

#endif // BASICSTROKECONTEXT_H
