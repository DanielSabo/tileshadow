#include "canvascontext.h"
#include <QDebug>

CanvasContext::CanvasContext()
    : currentLayer(0),
      inTransientOpacity(false)
{
}

CanvasContext::~CanvasContext()
{
    clearUndoHistory();
    clearRedoHistory();
}

void CanvasContext::addUndoEvent(CanvasUndoEvent *undoEvent)
{
    clearRedoHistory();
    inTransientOpacity = false;
    undoHistory.prepend(undoEvent);
}

void CanvasContext::clearUndoHistory()
{
    inTransientOpacity = false;
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
