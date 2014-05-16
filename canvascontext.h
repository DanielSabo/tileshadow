#ifndef CANVASCONTEXT_H
#define CANVASCONTEXT_H

#include "canvaswidget-opencl.h"
#include <QPointF>
#include <QSet>
#include <QOpenGLFunctions_3_2_Core>
#include <map>
#include <set>
#include "canvaslayer.h"

static const int TILE_PIXEL_WIDTH  = 128;
static const int TILE_PIXEL_HEIGHT = 128;
static const int TILE_COMP_TOTAL = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4;

/* From GEGL */
static inline int tile_indice (int coordinate, int stride)
{
    if (coordinate >= 0)
        return coordinate / stride;
    else
        return ((coordinate + 1) / stride) - 1;
}

class CanvasContext;

struct _TilePointCompare
{
    bool operator()(const QPoint &a, const QPoint &b);
};

typedef std::set<QPoint, _TilePointCompare> TileSet;

class StrokeContext
{
public:
    StrokeContext(CanvasContext *ctx, CanvasLayer *layer) : ctx(ctx), layer(layer) {}
    virtual ~StrokeContext() {}

    virtual TileSet startStroke(QPointF point, float pressure) = 0;
    virtual TileSet strokeTo(QPointF point, float pressure) = 0;

    virtual void multiplySize(float mult) = 0;
    virtual float getPixelRadius() = 0;

    CanvasContext *ctx;
    CanvasLayer   *layer;
};

class CanvasTile
{
public:
    CanvasTile(int x, int y);
    ~CanvasTile();

    int     x;
    int     y;
    cl_mem  tileMem;
    float  *tileData;

    void mapHost(void);
    void unmapHost(void);

    void fill(float r, float g, float b, float a);
};

class CanvasContext
{
public:
    CanvasContext(QOpenGLFunctions_3_2_Core *glFuncs) :
                      glFuncs(glFuncs),
                      vertexShader(0),
                      fragmentShader(0),
                      program(0),
                      vertexBuffer(0),
                      tileWidth(0),
                      tileHeight(0) {}
    ~CanvasContext();

    QOpenGLFunctions_3_2_Core *glFuncs;

    GLuint vertexShader;
    GLuint fragmentShader;

    GLuint program;
    GLuint locationTileOrigin;
    GLuint locationTileSize;
    GLuint locationTileImage;
    GLuint locationTilePixels;

    GLuint vertexBuffer;
    GLuint vertexArray;

    int tileWidth;
    int tileHeight;

    QScopedPointer<StrokeContext> stroke;

    CanvasLayer layer;

    std::map<uint64_t, GLuint> glTiles;
    TileSet dirtyTiles;

    GLuint getGLBuf(int x, int y);
    void closeTileAt(int x, int y);
    void closeTiles(void);

};

#endif // CANVASCONTEXT_H
