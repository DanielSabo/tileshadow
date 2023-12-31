#ifndef ROUNDBRUSHTOOL_H
#define ROUNDBRUSHTOOL_H

#include "basetool.h"

class RoundBrushToolPrivate;
class RoundBrushTool : public BaseTool
{
public:
    RoundBrushTool();
    ~RoundBrushTool() override;
    std::unique_ptr<BaseTool> clone() override;

    float getPixelRadius() override;
    void setColor(const QColor &color) override;

    virtual void setToolSetting(QString const &name, QVariant const &value) override;
    virtual QVariant getToolSetting(QString const &name) override;
    virtual QList<ToolSettingInfo> listToolSettings() override;

    std::unique_ptr<StrokeContext> newStroke(StrokeContextArgs const &args) override;

private:
    std::unique_ptr<RoundBrushToolPrivate> priv;
};

#endif // ROUNDBRUSHTOOL_H
