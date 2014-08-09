#include "tiledebugtool.h"
#include "basicstrokecontext.h"
#include <qmath.h>
#include <QColor>
#include <QVariant>

class TileDebugToolPrivate
{
public:

    float radius;
    QColor color;
};

TileDebugTool::TileDebugTool() :
    priv(new TileDebugToolPrivate)
{
    reset();
}

TileDebugTool::~TileDebugTool()
{
    delete priv;
}

BaseTool *TileDebugTool::clone()
{
    TileDebugTool *result = new TileDebugTool();
    *result->priv = *priv;

    return result;
}

void TileDebugTool::reset()
{
    priv->color = QColor::fromRgbF(0.0, 0.0, 0.0);
    priv->radius = 10.0f;
}

void TileDebugTool::setToolSetting(QString const &name, QVariant const &value)
{
    if (name == QStringLiteral("size") && value.canConvert<float>())
    {
        float radius = qBound<float>(1.0f, exp(value.toFloat()), 100.0f);

        priv->radius = radius;
    }
    else
    {
        BaseTool::setToolSetting(name, value);
    }
}

QVariant TileDebugTool::getToolSetting(const QString &name)
{
    if (name == QStringLiteral("size"))
    {
        return QVariant::fromValue<float>(log(priv->radius));
    }
    else
    {
        return BaseTool::getToolSetting(name);
    }
}

QList<ToolSettingInfo> TileDebugTool::listToolSettings()
{
    QList<ToolSettingInfo> result;

    result.append(ToolSettingInfo::exponentSlider("size", "SizeExp", 0.0f, 6.0f));

    return result;
}

float TileDebugTool::getPixelRadius()
{
    return priv->radius;
}

void TileDebugTool::setColor(const QColor &color)
{

}

StrokeContext *TileDebugTool::newStroke(CanvasLayer *layer)
{
    BasicStrokeContext *result = new BasicStrokeContext(layer, priv->radius);

    return result;
}
