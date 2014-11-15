#ifndef IMAGEFILES_H
#define IMAGEFILES_H

#include "canvasstack.h"
#include <QImage>

QImage stackToImage(CanvasStack *stack);
std::unique_ptr<CanvasLayer> layerFromImage(QImage image);

#endif // IMAGEFILES_H
