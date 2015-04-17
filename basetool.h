#ifndef BASETOOL_H
#define BASETOOL_H

#include <QList>
#include <QColor>
#include "strokecontext.h"
#include "toolsettinginfo.h"

struct StrokeContextArgs
{
    CanvasLayer *layer;
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
};

#endif // BASETOOL_H
