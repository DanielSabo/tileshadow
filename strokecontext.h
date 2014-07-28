#ifndef STROKECONTEXT_H
#define STROKECONTEXT_H

#include <canvaslayer.h>

class StrokeContext
{
public:
    StrokeContext(CanvasLayer *layer) : layer(layer) {}
    virtual ~StrokeContext() {}

    virtual TileSet startStroke(QPointF point, float pressure) = 0;
    virtual TileSet strokeTo(QPointF point, float pressure, float dt) = 0;

    CanvasLayer   *layer;
};

#endif // STROKECONTEXT_H
