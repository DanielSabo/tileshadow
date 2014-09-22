#ifndef TILESET_H
#define TILESET_H

#include <QPoint>
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

#endif // TILESET_H
