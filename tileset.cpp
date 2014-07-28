#include "tileset.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include <QDebug>

bool _TilePointCompare::operator ()(const QPoint &a, const QPoint &b) const
{
    if (a.y() < b.y())
        return true;
    else if (a.y() > b.y())
        return false;
    else if (a.x() < b.x())
        return true;
    return false;
}

void _deleteTileMap(TileMap *tiles)
{
    for (TileMap::iterator iter = tiles->begin(); iter != tiles->end(); ++iter)
    {
        delete iter->second;
    }
    tiles->clear();
}
