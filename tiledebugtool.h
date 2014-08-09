#ifndef TILEDEBUGTOOL_H
#define TILEDEBUGTOOL_H

#include "basetool.h"

class TileDebugToolPrivate;
class TileDebugTool : public BaseTool
{
public:
    TileDebugTool();
    ~TileDebugTool();
    BaseTool *clone();

    void reset();
    float getPixelRadius();
    void setColor(const QColor &color);

    void setToolSetting(const QString &name, const QVariant &value);
    virtual QVariant getToolSetting(QString const &name);
    virtual QList<ToolSettingInfo> listToolSettings();

    StrokeContext *newStroke(CanvasLayer *layer);

private:
    TileDebugToolPrivate *priv;
};

#endif // TILEDEBUGTOOL_H
