#ifndef MYPAINTSTROKECONTEXT_H
#define MYPAINTSTROKECONTEXT_H

#include "canvascontext.h"

class MyPaintStrokeContextPrivate;
class MyPaintStrokeContext : public StrokeContext
{
public:
    MyPaintStrokeContext(CanvasContext *ctx, CanvasLayer *layer);
    ~MyPaintStrokeContext();

    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure);

    bool fromJsonFile(const QString &path);
    void fromDefaults();

    void multiplySize(float mult);
    float getPixelRadius();

    MyPaintStrokeContextPrivate *priv;
};

#endif // MYPAINTSTROKECONTEXT_H
