#ifndef BASETOOL_H
#define BASETOOL_H

#include <QColor>
#include <strokecontext.h>

class BaseTool
{
public:
    BaseTool();
    virtual ~BaseTool();
    virtual StrokeContext *newStroke(CanvasLayer *layer) = 0;

    virtual void reset() = 0;
    virtual void setSizeMod(float mult);
    virtual float getPixelRadius() = 0;
    virtual void setColor(QColor const &color);
};

#endif // BASETOOL_H
