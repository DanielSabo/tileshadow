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
#include "strokecontext.h"

class CanvasContext
{
public:
    CanvasContext();
    ~CanvasContext();

    QOpenGLFunctions_3_2_Core *glFuncs;

    GLuint program;
    GLuint locationTileOrigin;
    GLuint locationTileSize;
    GLuint locationTileImage;
    GLuint locationTilePixels;

    GLuint vertexBuffer;
    GLuint vertexArray;

    GLuint cursorProgram;
    GLuint cursorProgramDimensions;
    GLuint cursorProgramPixelRadius;
    GLuint cursorVertexBuffer;
    GLuint cursorVertexArray;

    QScopedPointer<StrokeContext> stroke;
    ulong strokeEventTimestamp; // last stroke event time, in milliseconds

    int currentLayer;
    QScopedPointer<CanvasLayer> currentLayerCopy;
    CanvasStack layers;

    QList<CanvasUndoEvent *> undoHistory;
    QList<CanvasUndoEvent *> redoHistory;

    typedef std::map<QPoint, GLuint, _tilePointCompare> GLTileMap;

    GLuint backgroundGLTile;
    GLTileMap glTiles;
    TileSet dirtyTiles;
    TileSet strokeModifiedTiles;

    GLuint getGLBuf(int x, int y);
    void closeTileAt(int x, int y);
    void closeTiles();
    void clearUndoHistory();
    void clearRedoHistory();
    void clearTiles();
    void updateBackgroundTile();
};

#endif // CANVASCONTEXT_H
