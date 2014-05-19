#ifndef TILESET_H
#define TILESET_H

#include <QPoint>
#include <set>

struct _TilePointCompare
{
    bool operator()(const QPoint &a, const QPoint &b);
};

typedef std::set<QPoint, _TilePointCompare> TileSet;

#endif // TILESET_H
