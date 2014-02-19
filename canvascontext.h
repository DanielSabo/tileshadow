#ifndef CANVASCONTEXT_H
#define CANVASCONTEXT_H

#include "canvaswidget-opencl.h"
#include <QPointF>
#include <QOpenGLFunctions_3_2_Core>
#include <map>

static const int TILE_PIXEL_WIDTH  = 64;
static const int TILE_PIXEL_HEIGHT = 64;
static const int TILE_COMP_TOTAL = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4;

class CanvasContext;

class StrokeContext
{
public:
    StrokeContext(CanvasContext *ctx) : ctx(ctx) {}
    virtual ~StrokeContext() {}

    virtual bool startStroke(QPointF point, float pressure) = 0;
    virtual bool strokeTo(QPointF point, float pressure) = 0;

    CanvasContext *ctx;
};

class CanvasTile
{
public:
    CanvasTile();
    ~CanvasTile();

    bool    isOpen;
    cl_mem  tileMem;
    float  *tileData;
    GLuint  tileBuffer;

    void mapHost(void);
    void unmapHost(void);
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

    std::map<uint64_t, CanvasTile *> tiles;
    std::list<CanvasTile *> openTiles;

    CanvasTile *getTile(int x, int y);
    cl_mem clOpenTile(CanvasTile *tile);
    float *openTile(CanvasTile *tile);
    void closeTile(CanvasTile *tile);
    void closeTiles(void);
};

#endif // CANVASCONTEXT_H
