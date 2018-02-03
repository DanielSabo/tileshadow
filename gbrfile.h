#ifndef GBRFILE_H
#define GBRFILE_H

#include <QList>
#include <QImage>
#include <QIODevice>

QImage readGBR(QIODevice *file);
QList<QImage> readGIH(QIODevice *file);

QByteArray writeGBR(QImage const image);
QByteArray writeGIH(QList<QImage> const images);

#endif // GBRFILE_H
