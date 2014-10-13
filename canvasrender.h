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

struct GLShaderProgram
{
    GLShaderProgram() :
        program(0),
        vertexBuffer(0),
        vertexArray(0) {}

    GLuint program;

    GLuint vertexBuffer;
    GLuint vertexArray;
};

class CanvasContext;
class CanvasRender
{
public:
    CanvasRender();
    ~CanvasRender();

    QOpenGLFunctions_3_2_Core *glFuncs;

    struct : GLShaderProgram {
        GLuint tileOrigin;
        GLuint tileSize;
        GLuint tileImage;
        GLuint tilePixels;
        GLuint binSize;
    } tileShader;

    struct : GLShaderProgram {
        GLuint dimensions;
        GLuint pixelRadius;
    } cursorShader;

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
