#ifndef MYPAINTSTROKECONTEXT_H
#define MYPAINTSTROKECONTEXT_H

#include "canvascontext.h"

class MyPaintStrokeContextPrivate;
class MyPaintStrokeContext : public StrokeContext
{
public:
    MyPaintStrokeContext(CanvasContext *ctx);
    ~MyPaintStrokeContext();

    bool startStroke(QPointF point, float pressure);
    bool strokeTo(QPointF point, float pressure);

    bool fromJsonFile(const QString &path);
    void fromDefaults();

    MyPaintStrokeContextPrivate *priv;
};

#endif // MYPAINTSTROKECONTEXT_H
