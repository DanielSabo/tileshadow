#ifndef GRADIENTTOOL_H
#define GRADIENTTOOL_H

#include "basetool.h"

class GradientToolPrivate;
class GradientTool : public BaseTool
{
public:
    GradientTool();
    ~GradientTool() override;

    std::unique_ptr<BaseTool> clone() override;
    std::unique_ptr<StrokeContext> newStroke(StrokeContextArgs const &args) override;

    float getPixelRadius() override;
    void setColor(QColor const &color) override;
    bool coalesceMovement() override;

private:
    std::unique_ptr<GradientToolPrivate> priv;
};

#endif // GRADIENTTOOL_H
