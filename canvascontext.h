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

class CanvasContext;

class StrokeContext
{
public:
    StrokeContext(CanvasContext *ctx, CanvasLayer *layer) : ctx(ctx), layer(layer) {}
    virtual ~StrokeContext() {}

    virtual TileSet startStroke(QPointF point, float pressure) = 0;
    virtual TileSet strokeTo(QPointF point, float pressure) = 0;

    virtual void multiplySize(float mult);
    virtual float getPixelRadius() = 0;
    virtual void setColor(QColor const &color);

    CanvasContext *ctx;
    CanvasLayer   *layer;
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
                      tileHeight(0),
                      currentLayer(0) {}
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

    int currentLayer;
    QScopedPointer<CanvasLayer> currentLayerCopy;
    CanvasStack layers;

    QList<CanvasUndoEvent *> undoHistory;
    QList<CanvasUndoEvent *> redoHistory;

    typedef std::map<QPoint, GLuint, _TilePointCompare> GLTileMap;

    GLTileMap glTiles;
    TileSet dirtyTiles;
    TileSet strokeModifiedTiles;

    GLuint getGLBuf(int x, int y);
    void closeTileAt(int x, int y);
    void closeTiles(void);
    void clearUndoHistory();
    void clearRedoHistory();
};

#endif // CANVASCONTEXT_H
