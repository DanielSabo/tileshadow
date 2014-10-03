#ifndef CANVASCONTEXT_H
#define CANVASCONTEXT_H

#include "canvaswidget-opencl.h"
#include <QPointF>
#include <QSet>
#include <QOpenGLFunctions_3_2_Core>
#include <map>
#include "tileset.h"
#include "canvaslayer.h"
#include "canvasstack.h"
#include "canvasundoevent.h"
#include "basetool.h"
#include "strokecontext.h"

class CanvasContext
{
public:
    CanvasContext();
    ~CanvasContext();

    QScopedPointer<BaseTool> strokeTool;
    QScopedPointer<StrokeContext> stroke;

    int currentLayer;
    QScopedPointer<CanvasLayer> currentLayerCopy;
    CanvasStack layers;

    QList<CanvasUndoEvent *> undoHistory;
    QList<CanvasUndoEvent *> redoHistory;

    bool inTransientOpacity;
    TileSet dirtyTiles;
    TileSet strokeModifiedTiles;

    void addUndoEvent(CanvasUndoEvent *undoEvent);
    void clearUndoHistory();
    void clearRedoHistory();
    void updateBackgroundTile();
};

#endif // CANVASCONTEXT_H
