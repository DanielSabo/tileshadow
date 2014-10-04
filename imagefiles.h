#ifndef IMAGEEXPORT_H
#define IMAGEEXPORT_H

#include "canvasstack.h"
#include <QImage>

QImage stackToImage(CanvasStack *stack);
std::unique_ptr<CanvasLayer> layerFromImage(QImage image);

#endif // IMAGEEXPORT_H
