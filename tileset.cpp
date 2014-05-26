#include "tileset.h"

bool _TilePointCompare::operator ()(const QPoint &a, const QPoint &b)
{
    if (a.y() < b.y())
        return true;
    else if (a.y() > b.y())
        return false;
    else if (a.x() < b.x())
        return true;
    return false;
}
