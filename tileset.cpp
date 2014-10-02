#include "tileset.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include <QDebug>

bool _tilePointCompare::operator ()(const QPoint &a, const QPoint &b) const
{
    if (a.y() < b.y())
        return true;
    else if (a.y() > b.y())
        return false;
    else if (a.x() < b.x())
        return true;
    return false;
}

QRect tileSetBounds(TileSet const &objTiles)
{
    QRect tileBounds;
    TileSet::iterator iter;

    if (objTiles.empty())
        tileBounds = QRect(0, 0, 0, 0);
    else
    {
        tileBounds = QRect(objTiles.begin()->x(), objTiles.begin()->y(), 1, 1);
    }

    for(iter = objTiles.begin(); iter != objTiles.end(); iter++)
    {
        tileBounds = tileBounds.united(QRect(iter->x(), iter->y(), 1, 1));
    }

    return tileBounds;
}
