#ifndef ROUNDBRUSHTOOL_H
#define ROUNDBRUSHTOOL_H

#include "basetool.h"

class RoundBrushToolPrivate;
class RoundBrushTool : public BaseTool
{
public:
    RoundBrushTool();
    ~RoundBrushTool();
    BaseTool *clone();

    void reset();
    float getPixelRadius();
    void setColor(const QColor &color);

    virtual void setToolSetting(QString const &name, QVariant const &value);
    virtual QVariant getToolSetting(QString const &name);
    virtual QList<ToolSettingInfo> listToolSettings();

    StrokeContext *newStroke(CanvasLayer *layer);

private:
    RoundBrushToolPrivate *priv;
};

#endif // ROUNDBRUSHTOOL_H
