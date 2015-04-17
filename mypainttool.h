#ifndef MYPAINTTOOL_H
#define MYPAINTTOOL_H

#include "basetool.h"

class MyPaintToolPrivate;

class MyPaintTool : public BaseTool
{
public:
    explicit MyPaintTool(const QString &path);
    ~MyPaintTool();
    BaseTool *clone();

    void reset();
    float getPixelRadius();
    void setColor(const QColor &color);

    virtual void setToolSetting(QString const &name, QVariant const &value);
    virtual QVariant getToolSetting(QString const &name);
    virtual QList<ToolSettingInfo> listToolSettings();

    StrokeContext *newStroke(StrokeContextArgs const &args);

private:
    MyPaintTool(const MyPaintTool &tool);
    MyPaintToolPrivate *priv;
};

#endif // MYPAINTTOOL_H
