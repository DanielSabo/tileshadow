#ifndef PATTERNFILLTOOL_H
#define PATTERNFILLTOOL_H


#include "basetool.h"

class PatternFillToolPrivate;
class PatternFillTool : public BaseTool
{
public:
    PatternFillTool(QStringList const &patternPaths);
    PatternFillTool(PatternFillTool const &from);
    ~PatternFillTool() override;
    BaseTool *clone() override;

    void reset() override;
    float getPixelRadius() override;
    void setColor(const QColor &color) override;

    void setToolSetting(const QString &name, const QVariant &value) override;
    virtual QVariant getToolSetting(QString const &name) override;
    virtual QList<ToolSettingInfo> listToolSettings() override;

    StrokeContext *newStroke(StrokeContextArgs const &args) override;

private:
    PatternFillToolPrivate *priv;
};

#endif // PATTERNFILLTOOL_H
