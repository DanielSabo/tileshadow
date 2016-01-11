#ifndef BASETOOL_H
#define BASETOOL_H

#include <QList>
#include <QColor>
#include <QByteArray>
#include "strokecontext.h"
#include "toolsettinginfo.h"

struct StrokeContextArgs
{
    StrokeContextArgs(CanvasLayer *layer, CanvasLayer const *unmodifiedLayer)
        : layer(layer), unmodifiedLayer(unmodifiedLayer) {}

    CanvasLayer *layer;
    CanvasLayer const *unmodifiedLayer;
};

class BaseTool
{
public:
    BaseTool();
    virtual ~BaseTool();
    virtual BaseTool *clone() = 0;
    virtual StrokeContext *newStroke(StrokeContextArgs const &args) = 0;

    virtual void reset() = 0;
    virtual float getPixelRadius() = 0;
    virtual void setColor(QColor const &color);
    virtual bool coalesceMovement();

    virtual void setToolSetting(QString const &name, QVariant const &value);
    virtual QVariant getToolSetting(QString const &name);
    virtual QList<ToolSettingInfo> listToolSettings();
    virtual QList<ToolSettingInfo> listAdvancedSettings();
    virtual QString saveExtension();
    virtual bool saveTo(QByteArray &output);
};

#endif // BASETOOL_H
