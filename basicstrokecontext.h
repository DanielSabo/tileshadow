#ifndef BASICSTROKECONTEXT_H
#define BASICSTROKECONTEXT_H

#include "canvascontext.h"

class BasicStrokeContext : public StrokeContext
{
public:
    BasicStrokeContext(CanvasContext *ctx, CanvasLayer *layer, float radius);

    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure);
    void drawDab(QPointF point, TileSet &modTiles);

    void multiplySize(float mult);
    float getPixelRadius();

    float radius;

    QPointF start;
    QPointF lastDab;
};

#endif // BASICSTROKECONTEXT_H
