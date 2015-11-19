#ifndef ROUNDBRUSHTOOL_H
#define ROUNDBRUSHTOOL_H

#include "basetool.h"

class RoundBrushToolPrivate;
class RoundBrushTool : public BaseTool
{
public:
    RoundBrushTool();
    ~RoundBrushTool() override;
    BaseTool *clone() override;

    void reset() override;
    float getPixelRadius() override;
    void setColor(const QColor &color) override;

    virtual void setToolSetting(QString const &name, QVariant const &value) override;
    virtual QVariant getToolSetting(QString const &name) override;
    virtual QList<ToolSettingInfo> listToolSettings() override;

    StrokeContext *newStroke(StrokeContextArgs const &args) override;

private:
    RoundBrushToolPrivate *priv;
};

#endif // ROUNDBRUSHTOOL_H
