#ifndef CANVASCONTEXT_H
#define CANVASCONTEXT_H

#include <memory>
#include <list>
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

    std::unique_ptr<BaseTool> strokeTool;
    std::unique_ptr<StrokeContext> stroke;

    int currentLayer;
    std::unique_ptr<CanvasLayer> currentLayerCopy;
    CanvasStack layers;
    std::unique_ptr<CanvasStack> flashStack;
    std::unique_ptr<CanvasLayer> quickmask;
    std::unique_ptr<CanvasLayer> quickmaskCopy;

    std::list<std::unique_ptr<CanvasUndoEvent>> undoHistory;
    std::list<std::unique_ptr<CanvasUndoEvent>> redoHistory;

    bool inTransientOpacity;
    TileSet dirtyTiles;
    TileSet strokeModifiedTiles;

    void renderDirty(TileMap *into);

    void resetQuickmask();
    void updateQuickmaskCopy();

    void addUndoEvent(CanvasUndoEvent *undoEvent);
    void clearUndoHistory();
    void clearRedoHistory();
};

#endif // CANVASCONTEXT_H
