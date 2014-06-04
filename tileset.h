#ifndef TILESET_H
#define TILESET_H

#include <QPoint>
#include <set>
#include <map>

class CanvasTile;

struct _TilePointCompare
{
    bool operator()(const QPoint &a, const QPoint &b);
};

typedef std::set<QPoint, _TilePointCompare> TileSet;
typedef std::map<QPoint, CanvasTile *, _TilePointCompare> TileMap;

void _deleteTileMap(TileMap *tiles);

#endif // TILESET_H
