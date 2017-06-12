#ifndef TILESET_H
#define TILESET_H

#include <QPoint>
#include <QRect>
#include <set>
#include <map>
#include <memory>
#include "canvastile.h"

struct _tilePointCompare
{
    bool operator()(const QPoint &a, const QPoint &b) const;
};

typedef std::set<QPoint, _tilePointCompare> TileSet;
typedef std::map<QPoint, std::unique_ptr<CanvasTile>, _tilePointCompare> TileMap;

QRect tileSetBounds(TileSet const &objTiles);

static inline QRect boundingTiles(QRect const &pixelRect) {
    if (pixelRect.isEmpty())
        return QRect();

    int tileX1 = tile_indice(pixelRect.x(), TILE_PIXEL_WIDTH);
    int tileX2 = tile_indice(pixelRect.x() + pixelRect.width(), TILE_PIXEL_WIDTH) + 1;
    int tileY1 = tile_indice(pixelRect.y(), TILE_PIXEL_HEIGHT);
    int tileY2 = tile_indice(pixelRect.y() + pixelRect.height(), TILE_PIXEL_HEIGHT) + 1;

    return QRect(tileX1, tileY1, tileX2 - tileX1, tileY2 - tileY1);
}

static inline void tileSetInsert(TileSet &dst, TileSet const &src) {
    dst.insert(src.begin(), src.end());
}


#endif // TILESET_H
