#include "canvascontext.h"
#include <QDebug>

CanvasContext::CanvasContext()
    : currentLayer(0),
      inTransientOpacity(false)
{
    resetQuickmask();
}

CanvasContext::~CanvasContext()
{
    clearUndoHistory();
    clearRedoHistory();
}

void CanvasContext::renderDirty(TileMap *into)
{
    const bool quickmaskState = quickmask->visible;

    for (auto iter: dirtyTiles)
    {
        std::unique_ptr<CanvasTile> renderedTile = layers.getTileMaybe(iter.x(), iter.y());

        if (quickmaskState)
        {
            if (CanvasTile *qmTile = quickmask->getTileMaybe(iter.x(), iter.y()))
            {
                if (!renderedTile)
                    renderedTile = layers.backgroundTileCL->copy();

                static const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
                cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->colorMask;
                cl_mem inMem = renderedTile->unmapHost();
                cl_mem auxMem = qmTile->unmapHost();

                cl_float4 color = {1.0f, 0.0f, 0.0f, 0.6f};

                clSetKernelArg<cl_mem>(kernel, 0, inMem);
                clSetKernelArg<cl_mem>(kernel, 1, inMem);
                clSetKernelArg<cl_mem>(kernel, 2, auxMem);
                clSetKernelArg<cl_float4>(kernel, 3, color);
                clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                       kernel,
                                       1, nullptr, global_work_size, nullptr,
                                       0, nullptr, nullptr);
            }
        }

        (*into)[iter] = std::move(renderedTile);
    }

    dirtyTiles.clear();
}

void CanvasContext::resetQuickmask()
{
    quickmask.reset(new CanvasLayer);
    quickmask->visible = false;
    quickmaskCopy.reset(new CanvasLayer);
    quickmaskCopy->visible = false;
}

void CanvasContext::updateQuickmaskCopy()
{
    if (quickmask->visible)
        quickmaskCopy = quickmask->deepCopy();
    else
        quickmaskCopy->tiles->clear();
}

void CanvasContext::addUndoEvent(CanvasUndoEvent *undoEvent)
{
    clearRedoHistory();
    inTransientOpacity = false;
    undoHistory.push_front(std::unique_ptr<CanvasUndoEvent>(undoEvent));
}

void CanvasContext::clearUndoHistory()
{
    inTransientOpacity = false;
    undoHistory.clear();
}

void CanvasContext::clearRedoHistory()
{
    redoHistory.clear();
}
