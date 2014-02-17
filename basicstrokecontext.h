#ifndef BASICSTROKECONTEXT_H
#define BASICSTROKECONTEXT_H

#include "canvascontext.h"

class BasicStrokeContext : public StrokeContext
{
public:
    BasicStrokeContext(CanvasContext *ctx) : StrokeContext(ctx) {}

    bool startStroke(QPointF point);
    bool strokeTo(QPointF point);
    void drawDab(QPointF point);

    QPointF start;
    QPointF lastDab;
};

#endif // BASICSTROKECONTEXT_H
