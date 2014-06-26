#ifndef BASETOOL_H
#define BASETOOL_H

#include "canvascontext.h"

class BaseTool
{
public:
    BaseTool();
    virtual ~BaseTool();
    virtual StrokeContext *newStroke(CanvasContext *ctx, CanvasLayer *layer) = 0;

    virtual void reset() = 0;
    virtual void setSizeMod(float mult);
    virtual float getPixelRadius() = 0;
    virtual void setColor(QColor const &color);
};

#endif // BASETOOL_H
