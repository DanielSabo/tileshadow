#ifndef GPLFILE_H
#define GPLFILE_H

#include <QIODevice>
#include <QByteArray>
#include <QString>
#include <QColor>
#include <vector>

struct PaletteEntry {
    QString name;
    QColor color;
};

struct ColorPalette {
    QString name;
    int columns = 0;
    std::vector<PaletteEntry> values;

    QByteArray toGPL();
};

ColorPalette readGPL(QIODevice *file);

#endif // GPLFILE_H
