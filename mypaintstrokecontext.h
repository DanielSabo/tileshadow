#ifndef MYPAINTSTROKECONTEXT_H
#define MYPAINTSTROKECONTEXT_H

#include "canvascontext.h"

class MyPaintStrokeContextPrivate;
class MyPaintStrokeContext : public StrokeContext
{
public:
    MyPaintStrokeContext(CanvasContext *ctx);
    ~MyPaintStrokeContext();

    bool startStroke(QPointF point);
    bool strokeTo(QPointF point);

    MyPaintStrokeContextPrivate *priv;
};

#endif // MYPAINTSTROKECONTEXT_H
