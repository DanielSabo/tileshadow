#ifndef TILEDEBUGTOOL_H
#define TILEDEBUGTOOL_H

#include "basetool.h"

class TileDebugToolPrivate;
class TileDebugTool : public BaseTool
{
public:
    TileDebugTool();
    ~TileDebugTool();

    void reset();
    void setSizeMod(float mult);
    float getPixelRadius();
    void setColor(const QColor &color);

    StrokeContext *newStroke(CanvasContext *ctx, CanvasLayer *layer);

private:
    TileDebugToolPrivate *priv;
};

#endif // TILEDEBUGTOOL_H
