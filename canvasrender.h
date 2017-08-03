#ifndef CANVASRENDER_H
#define CANVASRENDER_H

#include "canvaswidget-opencl.h"
#include "tileset.h"
#include <map>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QOpenGLFunctions_3_2_Core>

struct GLShaderProgram
{
    GLShaderProgram() :
        program(0),
        vertexBuffer(0),
        vertexArray(0) {}

    void cleanup(QOpenGLFunctions_3_2_Core *glFuncs);

    GLuint program;

    GLuint vertexBuffer;
    GLuint vertexArray;
};

class QMatrix;
class CanvasContext;
class CanvasRender
{
public:
    CanvasRender();
    ~CanvasRender();

    QOpenGLFunctions_3_2_Core *glFuncs;

    struct : GLShaderProgram {
        GLuint tileOrigin;
        GLuint tileScale;
        GLuint tileImage;
        GLuint tilePixels;
        GLuint binSize;
    } tileShader;

    struct : GLShaderProgram {
        GLuint dimensions;
        GLuint pixelRadius;
        GLuint innerColor;
        GLuint outerColor;
    } cursorShader;

    struct : GLShaderProgram {
        GLuint dimensions;
        GLuint pixelRadius;
        GLuint previewColor;
    } colorDotShader;

    struct : GLShaderProgram {
        GLuint fillColor;
    } canvasFrameShader;

    GLuint backbufferFramebuffer;
    GLuint backbufferRenderbuffer;
    GLuint backbufferStencilbuffer;

    typedef struct {
        GLuint glBuf;
        cl_mem clBuf;
    } RenderTile;

    typedef std::map<QPoint, RenderTile, _tilePointCompare> GLTileMap;

    GLuint backgroundGLTile;
    GLTileMap glTiles;

    TileSet dirtyTiles;
    bool dirtyBackground;

    QPoint viewOrigin;
    QSize viewSize;
    float viewScale;
    int viewPixelRatio;
    QRect viewFrame;

    void resizeFramebuffer(int w, int h);
    void shiftFramebuffer(int xOffset, int yOffset);

    GLuint getGLBuf(int x, int y);
    void clearTiles();
    void updateBackgroundTile(CanvasContext *ctx);

    void ensureTiles(const TileMap &tiles);
    void renderTileMap(TileMap &tiles);

    void renderView(QPoint newOrigin, QSize newSize, float newScale, QRect newFrame, bool fullRedraw);
    void drawToolCursor(QPoint cursorPos, float cusrorRadius, QColor outerColor = Qt::black, QColor innerColor = Qt::white);
    void drawColorDots(QColor dotPreviewColor);
private:
    void drawFrame(QRectF frame, const QMatrix &canvasToViewport);
    void renderTile(int x, int y, CanvasTile *tile);
};

#endif // CANVASRENDER_H
