#ifndef MYPAINTTOOL_H
#define MYPAINTTOOL_H

#include "basetool.h"

class MyPaintToolPrivate;

class MyPaintTool : public BaseTool
{
public:
    explicit MyPaintTool(const QString &path);
    ~MyPaintTool() override;
    std::unique_ptr<BaseTool> clone() override;

    float getPixelRadius() override;
    void setColor(const QColor &color) override;

    virtual void setToolSetting(QString const &name, QVariant const &value) override;
    virtual QVariant getToolSetting(QString const &name) override;
    virtual QList<ToolSettingInfo> listToolSettings() override;
    virtual QList<ToolSettingInfo> listAdvancedSettings() override;
    virtual QString saveExtension() override;
    virtual QByteArray serialize() override;

    std::unique_ptr<StrokeContext> newStroke(StrokeContextArgs const &args) override;

private:
    MyPaintTool(const MyPaintTool &tool);
    std::unique_ptr<MyPaintToolPrivate> priv;
};

#endif // MYPAINTTOOL_H
