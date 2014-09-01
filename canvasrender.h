#ifndef CANVASRENDER_H
#define CANVASRENDER_H

#include "canvaswidget-opencl.h"
#include <QPoint>
#include <QRect>
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

    GLuint backbufferFramebuffer;
    GLuint backbufferRenderbuffer;

    typedef std::map<QPoint, GLuint, _tilePointCompare> GLTileMap;

    GLuint backgroundGLTile;
    GLTileMap glTiles;

    QSize viewSize;

    void resizeFramebuffer(int w, int h);

    GLuint getGLBuf(int x, int y);
    void clearTiles();
    void updateBackgroundTile(CanvasContext *ctx);

    void ensureTiles(const TileMap &tiles);
    void renderTileMap(TileMap &tiles);

private:
    void renderTile(int x, int y, CanvasTile *tile);
};

#endif // CANVASRENDER_H
