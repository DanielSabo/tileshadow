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
