#ifndef GRADIENTTOOL_H
#define GRADIENTTOOL_H



#include "basetool.h"

class GradientToolPrivate;
class GradientTool : public BaseTool
{
public:
    GradientTool();
    ~GradientTool() override;

    BaseTool *clone();
    StrokeContext *newStroke(StrokeContextArgs const &args);

    void reset();
    float getPixelRadius();
    void setColor(QColor const &color);
    bool coalesceMovement();

private:
    GradientToolPrivate *priv;
};

#endif // GRADIENTTOOL_H
