#ifndef CANVASLAYER_H
#define CANVASLAYER_H

#include <QSharedPointer>
#include "canvaswidget-opencl.h"
#include "blendmodes.h"
#include "tileset.h"

class CanvasTile;

class CanvasLayer
{
public:
    CanvasLayer(QString name = "");
    ~CanvasLayer();

    QString name;
    bool visible;
    BlendMode::Mode mode;

    QSharedPointer<TileMap> tiles;

    TileSet getTileSet() const;

    float *openTileAt(int x, int y);
    cl_mem clOpenTileAt(int x, int y);

    CanvasTile *getTile(int x, int y);
    CanvasTile *getTileMaybe(int x, int y) const;
    CanvasTile *takeTileMaybe(int x, int y);

    CanvasLayer *deepCopy() const;
    CanvasLayer *translated(int x, int y) const;
    TileSet takeTiles(CanvasLayer *source);
    void prune();
};

#endif // CANVASLAYER_H
