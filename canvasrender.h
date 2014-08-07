#ifndef CANVASRENDER_H
#define CANVASRENDER_H

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

class CanvasContext;
class CanvasRender
{
public:
    CanvasRender();
    ~CanvasRender();

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

    typedef std::map<QPoint, GLuint, _tilePointCompare> GLTileMap;

    GLuint backgroundGLTile;
    GLTileMap glTiles;

    GLuint getGLBuf(int x, int y);
    void renderTile(int x, int y, CanvasTile *tile);
    void closeTiles(CanvasContext *ctx);
    void clearTiles();
    void updateBackgroundTile(CanvasContext *ctx);
};

#endif // CANVASRENDER_H
