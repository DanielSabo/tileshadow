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

    QOpenGLFunctions_3_2_Core *glFuncs = nullptr;

    struct : GLShaderProgram {
        GLuint tileOrigin;
        GLuint tileMatrix;
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

    GLuint backbufferFramebuffer = 0;
    GLuint backbufferRenderbuffer = 0;
    GLuint backbufferStencilbuffer = 0;

    typedef struct {
        GLuint glBuf;
        cl_mem clBuf;
    } RenderTile;

    typedef std::map<QPoint, RenderTile, _tilePointCompare> GLTileMap;

    GLuint backgroundGLTile = 0;
    GLTileMap glTiles;

    TileSet dirtyTiles;
    bool dirtyBackground = true;

    QPoint viewOrigin;
    QSize viewSize;
    float viewScale = 1.0f;
    float viewAngle = 0.0f;
    int viewPixelRatio = 1;
    QRect viewFrame;
    bool mirrorHoriz = false;
    bool mirrorVert = false;

    void resizeFramebuffer(int w, int h);
    void shiftFramebuffer(int xOffset, int yOffset);

    GLuint getGLBuf(int x, int y);
    void clearTiles();
    void updateBackgroundTile(CanvasContext *ctx);

    void ensureTiles(const TileMap &tiles);
    void renderTileMap(TileMap &tiles);

    void renderView(QPoint newOrigin, QSize newSize, float newScale, float newAngle, bool newMirrorHoriz, bool newMirrorVert, QRect newFrame, bool fullRedraw);
    void drawToolCursor(QPoint cursorPos, float cusrorRadius, QColor outerColor = Qt::black, QColor innerColor = Qt::white);
    void drawColorDots(QColor dotPreviewColor);
private:
    void drawFrame(QRectF frame, const QMatrix &canvasToViewport);
    void renderTile(int x, int y, CanvasTile *tile);
};

#endif // CANVASRENDER_H
