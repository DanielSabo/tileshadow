#ifndef GBRFILE_H
#define GBRFILE_H

#include <QList>
#include <QImage>
#include <QIODevice>

QImage readGBR(QIODevice *file);
QList<QImage> readGIH(QIODevice *file);

#endif // GBRFILE_H
