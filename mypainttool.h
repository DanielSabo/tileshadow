#ifndef MYPAINTTOOL_H
#define MYPAINTTOOL_H

#include "basetool.h"

class MyPaintToolPrivate;

class MyPaintTool : public BaseTool
{
public:
    MyPaintTool(const QString &path);
    ~MyPaintTool();

    void reset();
    float getPixelRadius();
    void setColor(const QColor &color);

    virtual void setToolSetting(QString const &name, QVariant const &value);
    virtual QVariant getToolSetting(QString const &name);
    virtual QList<ToolSettingInfo> listToolSettings();

    StrokeContext *newStroke(CanvasLayer *layer);

private:
    MyPaintToolPrivate *priv;
};

#endif // MYPAINTTOOL_H
