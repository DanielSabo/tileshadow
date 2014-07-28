#ifndef MYPAINTSTROKECONTEXT_H
#define MYPAINTSTROKECONTEXT_H

#include "canvascontext.h"

typedef QMap<QString, QPair<float, QMap<QString, QList<QPointF> > > > MyPaintToolSettings;

class MyPaintStrokeContextPrivate;
class MyPaintStrokeContext : public StrokeContext
{
public:
    MyPaintStrokeContext(CanvasLayer *layer);
    ~MyPaintStrokeContext();

    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure, float dt);

    bool fromSettings(MyPaintToolSettings const &settings);
    void fromDefaults();

    void multiplySize(float mult);
    float getPixelRadius();
    void setColor(QColor const &color);

    MyPaintStrokeContextPrivate *priv;
};

#endif // MYPAINTSTROKECONTEXT_H
