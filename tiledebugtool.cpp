#include "tiledebugtool.h"
#include "basicstrokecontext.h"
#include <QColor>

class TileDebugToolPrivate
{
public:
    float sizeMod;
    QColor color;
};

TileDebugTool::TileDebugTool() :
    priv(new TileDebugToolPrivate)
{
}

TileDebugTool::~TileDebugTool()
{
    delete priv;
}

void TileDebugTool::reset()
{
    priv->color = QColor::fromRgbF(0.0, 0.0, 0.0);
    priv->sizeMod = 1.0f;
}

void TileDebugTool::setSizeMod(float mult)
{
    priv->sizeMod = mult;
}

float TileDebugTool::getPixelRadius()
{
    return 10.0f * priv->sizeMod;
}

void TileDebugTool::setColor(const QColor &color)
{

}

StrokeContext *TileDebugTool::newStroke(CanvasLayer *layer)
{
    BasicStrokeContext *result = new BasicStrokeContext(layer, priv->sizeMod * 10.0f);
//    result->setColor(priv->color);

    return result;
}
