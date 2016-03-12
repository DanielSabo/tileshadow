#ifndef CANVASINDEX_H
#define CANVASINDEX_H

#include <QList>

class CanvasStack;
class CanvasLayer;

struct ParentInfo {
    QList<CanvasLayer *> *container; // The container holding element
    int index; // The relative location of *element* in it's container, container[index] == element.
    CanvasLayer *element; // The requested layer

    // The following will be filled in if *container* is not the root list
    QList<CanvasLayer *> *parentContainer; // The parent container which holds *container*
    int parentIndex; // The relative location of *container* in *parentContainer*, parentContainer[parentIndex] == container.
};

QList<int> pathFromAbsoluteIndex(CanvasStack *stack, int index);
ParentInfo parentFromAbsoluteIndex(CanvasStack *stack, int index);
CanvasLayer *layerFromAbsoluteIndex(CanvasStack *stack, int index);
int absoluteIndexOfLayer(CanvasStack *stack, CanvasLayer const *layer);

#endif // CANVASINDEX_H
