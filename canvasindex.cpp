#include "canvasindex.h"
#include "canvasstack.h"
#include "canvaslayer.h"

namespace {
QList<int> pathFromAbsoluteIndex(QList<CanvasLayer *> *layers, const int index, int &currentIndex)
{
    for (int i = 0; i < layers->size(); ++i)
    {
        CanvasLayer *layer = layers->at(i);
        if (index == currentIndex)
            return {i};
        currentIndex++;
        if (!layer->children.empty())
        {
            QList<int> result = pathFromAbsoluteIndex(&layer->children, index, currentIndex);
            if (!result.empty())
            {
                result.prepend(i);
                return result;
            }
        }
    }

    return QList<int>{};
}

ParentInfo findAbsoluteParent(QList<CanvasLayer *> *layers, const int index, int &currentIndex)
{
    for (int i = 0; i < layers->size(); ++i)
    {
        CanvasLayer *layer = layers->at(i);
        if (index == currentIndex)
            return {layers, i, layer, nullptr, 0};
        currentIndex++;
        if (!layer->children.empty())
        {
            ParentInfo result = findAbsoluteParent(&layer->children, index, currentIndex);
            if (result.element)
            {
                if (!result.parentContainer)
                {
                    result.parentContainer = layers;
                    result.parentIndex = i;
                }
                return result;
            }
        }
    }

    return {nullptr, 0, nullptr};
}

int findAbsoluteIndex(QList<CanvasLayer *> *layers, CanvasLayer const *targetLayer, int &currentIndex)
{
    for (int i = 0; i < layers->size(); ++i)
    {
        CanvasLayer *layer = layers->at(i);
        if (layer == targetLayer)
            return currentIndex;
        currentIndex++;
        if (!layer->children.empty())
        {
            int result = findAbsoluteIndex(&layer->children, targetLayer, currentIndex);
            if (result != -1)
                return result;
        }
    }

    return -1;
}
}

/*
 * Return the list of indexes that leads to *index* in *stack*, such that
 * stack->layers[p[0]][p[1]]...[p[n]] references *index*. Returns an empty
 * list if index is out of range.
 */
QList<int> pathFromAbsoluteIndex(CanvasStack *stack, int index)
{
    if (index < 0)
        return QList<int>{};

    int currentIndex = 0;
    return pathFromAbsoluteIndex(&stack->layers, index, currentIndex);
}


/* Returns ParentInfo for the layer at the specified absolute index
 * of the stack, if the index is out of range ParentInfo.element will
 * be NULL.
 */
ParentInfo parentFromAbsoluteIndex(CanvasStack *stack, int index)
{
    if (index < 0)
        return {nullptr, 0, nullptr};

    int currentIndex = 0;
    return findAbsoluteParent(&stack->layers, index, currentIndex);
}

/* Returns the layer at the specified absolute index of the stack,
 * returns NULL if the index is out of range.
 */
CanvasLayer *layerFromAbsoluteIndex(CanvasStack *stack, int index)
{
    ParentInfo info = parentFromAbsoluteIndex(stack, index);
    return info.element;
}

/* Search stack for layer and return it's absolute index,
 * returns -1 if the stack doesn't contain the layer.
 */
int absoluteIndexOfLayer(CanvasStack *stack, CanvasLayer const *layer) {
    int currentIndex = 0;
    return findAbsoluteIndex(&stack->layers, layer, currentIndex);
}
