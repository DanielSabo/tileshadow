#include "canvascontext.h"
#include <QDebug>

CanvasContext::CanvasContext()
    : currentLayer(0)
{
}

CanvasContext::~CanvasContext()
{
    clearUndoHistory();
    clearRedoHistory();
}

void CanvasContext::clearUndoHistory()
{
    while (!undoHistory.empty())
    {
        CanvasUndoEvent *event = undoHistory.first();
        delete event;
        undoHistory.removeFirst();
    }
}

void CanvasContext::clearRedoHistory()
{
    while (!redoHistory.empty())
    {
        CanvasUndoEvent *event = redoHistory.first();
        delete event;
        redoHistory.removeFirst();
    }
}
