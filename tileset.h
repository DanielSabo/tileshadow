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

static inline void tileSetInsert(TileSet &dst, TileSet const &src) {
    dst.insert(src.begin(), src.end());
}


#endif // TILESET_H
