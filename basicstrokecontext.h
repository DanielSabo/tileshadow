#ifndef BASICSTROKECONTEXT_H
#define BASICSTROKECONTEXT_H

#include "canvascontext.h"

class BasicStrokeContext : public StrokeContext
{
public:
    BasicStrokeContext(CanvasContext *ctx) : StrokeContext(ctx), radius(10.0f) {}

    bool startStroke(QPointF point, float pressure);
    bool strokeTo(QPointF point, float pressure);
    void drawDab(QPointF point);

    void multiplySize(float mult);
    float getPixelRadius();

    float radius;

    QPointF start;
    QPointF lastDab;
};

#endif // BASICSTROKECONTEXT_H
