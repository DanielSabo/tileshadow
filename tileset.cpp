#include "tileset.h"
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
    TileSet::iterator iter = objTiles.begin();
    if (iter != objTiles.end())
    {
        int x0, x1, y0, y1;
        x0 = x1 = iter->x();
        y0 = y1 = iter->y();

        while (++iter != objTiles.end())
        {
            if (x0 > iter->x()) { x0 = iter->x(); }
            else if (x1 < iter->x()) { x1 = iter->x(); }

            if (y0 > iter->y()) { y0 = iter->y(); }
            else if (y1 < iter->y()) { y1 = iter->y(); }
        }

        return QRect(QPoint(x0, y0), QPoint(x1, y1));
    }
    else
    {
        return QRect(0, 0, 0, 0);
    }
}
