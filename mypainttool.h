#ifndef MYPAINTTOOL_H
#define MYPAINTTOOL_H

#include "basetool.h"

class MyPaintToolPrivate;

class MyPaintTool : public BaseTool
{
public:
    MyPaintTool(const QString &path);
    ~MyPaintTool();

    void reset();
    void setSizeMod(float mult);
    float getPixelRadius();
    void setColor(const QColor &color);

    StrokeContext *newStroke(CanvasContext *ctx, CanvasLayer *layer);

private:
    MyPaintToolPrivate *priv;
};

#endif // MYPAINTTOOL_H
