#include "paintutils.h"
#include <stdlib.h>
#include <algorithm>

std::vector<QPoint> interpolateLine(QPoint start, QPoint end)
{
    std::vector<QPoint> result;

    int x0 = start.x();
    int y0 = start.y();
    int x1 = end.x();
    int y1 = end.y();
    int x = x0;
    int y = y0;

    if (x0 == x1 && y0 == y1)
    {
        result.push_back(start);
        return result;
    }

    bool steep = false;
    int sx, sy;
    int dx = ::abs(x1 - x0);
    if ((x1 - x0) > 0)
      sx = 1;
    else
      sx = -1;

    int dy = ::abs(y1 - y0);
    if ((y1 - y0) > 0)
      sy = 1;
    else
      sy = -1;

    if (dy > dx)
    {
      steep = true;
      std::swap(x, y);
      std::swap(dy, dx);
      std::swap(sy, sx);
    }

    int d = (2 * dy) - dx;

    for (int i = 0; i < dx; ++i)
    {
        if (steep)
            result.push_back(QPoint(y, x));
        else
            result.push_back(QPoint(x, y));

        while (d >= 0)
        {
            y = y + sy;
            d = d - (2 * dx);
        }

        x = x + sx;
        d = d + (2 * dy);
    }
    result.push_back(QPoint(x1, y1));
    return result;
}
