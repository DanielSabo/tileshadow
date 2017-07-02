#ifndef TILEDEBUGTOOL_H
#define TILEDEBUGTOOL_H

#include "basetool.h"

class TileDebugToolPrivate;
class TileDebugTool : public BaseTool
{
public:
    TileDebugTool();
    ~TileDebugTool() override;
    BaseTool *clone() override;

    float getPixelRadius() override;
    void setColor(const QColor &color) override;

    void setToolSetting(const QString &name, const QVariant &value) override;
    virtual QVariant getToolSetting(QString const &name) override;
    virtual QList<ToolSettingInfo> listToolSettings() override;

    StrokeContext *newStroke(StrokeContextArgs const &args) override;

private:
    std::unique_ptr<TileDebugToolPrivate> priv;
};

#endif // TILEDEBUGTOOL_H
