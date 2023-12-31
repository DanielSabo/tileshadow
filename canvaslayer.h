#ifndef CANVASLAYER_H
#define CANVASLAYER_H

#include <QList>
#include <memory>
#include "canvaswidget-opencl.h"
#include "blendmodes.h"
#include "layertype.h"
#include "tileset.h"

class QMatrix;
class CanvasTile;

class CanvasLayer
{
public:
    CanvasLayer(QString name = "");
    CanvasLayer(CanvasLayer const& from);
    CanvasTile &operator=(const CanvasTile&) = delete;
    ~CanvasLayer();

    QString name;
    bool visible;
    bool editable;
    BlendMode::Mode mode;
    float opacity;
    LayerType::Type type;

    std::shared_ptr<TileMap> tiles;
    QList<CanvasLayer *> children;

    TileSet getTileSet() const;

    float *openTileAt(int x, int y);
    cl_mem clOpenTileAt(int x, int y);

    CanvasTile *getTile(int x, int y);
    CanvasTile *getTileMaybe(int x, int y) const;
    std::unique_ptr<CanvasTile> takeTileMaybe(int x, int y);

    std::unique_ptr<CanvasLayer> deepCopy() const;
    std::unique_ptr<CanvasLayer> translated(int x, int y) const;
    std::unique_ptr<CanvasLayer> applyMatrix(QMatrix const &matrix) const;
    std::unique_ptr<CanvasLayer> mergeDown(CanvasLayer const *target) const;
    std::unique_ptr<CanvasLayer> flattened() const;
    TileSet takeTiles(CanvasLayer *source);
    void prune();
    void swapOut();

private:
    std::unique_ptr<CanvasLayer> shellCopy() const;
};

#endif // CANVASLAYER_H
