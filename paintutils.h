#ifndef PAINTUTILS_H
#define PAINTUTILS_H

#include <QPoint>
#include <vector>
#include <qmath.h>

template<typename T> static inline float dist(T a, T b)
{
    float dx = a.x() - b.x();
    float dy = a.y() - b.y();

    return sqrt(dx * dx + dy * dy);
}

std::vector<QPoint> interpolateLine(QPoint start, QPoint end);

#endif // PAINTUTILS_H
