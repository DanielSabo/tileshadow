#ifndef TILESET_H
#define TILESET_H

#include <QPoint>
#include <set>
#include <map>

class CanvasTile;

struct _tilePointCompare
{
    bool operator()(const QPoint &a, const QPoint &b) const;
};

typedef std::set<QPoint, _tilePointCompare> TileSet;
typedef std::map<QPoint, CanvasTile *, _tilePointCompare> TileMap;

void _deleteTileMap(TileMap *tiles);

#endif // TILESET_H
