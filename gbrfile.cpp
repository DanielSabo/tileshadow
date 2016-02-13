#include "gbrfile.h"
#include <QDataStream>
#include <QDebug>

QImage readGBR(QIODevice *file)
{
    if (!file->isOpen())
        file->open(QIODevice::ReadOnly);

    uint32_t headerLength;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    {
        QDataStream stream(file);
        stream >> headerLength >> version >> width >> height >> format;
        if (stream.status() != QDataStream::Ok)
        {
            qWarning() << "Failed to read .gbr file, header too short";
            return {};
        }
    }

    if ((file->size() - headerLength) != (format * width * height))
    {
        qWarning() << "Failed to read .gbr file, data too short";
        return {};
    }

    file->seek(headerLength);

    QImage image;

    if (format == 1)
    {
        image = QImage(width, height, QImage::Format_Indexed8);
        image.setColorCount(0xFF);
        for (int i = 0; i <= 0xFF; i++)
            image.setColor(i, qRgba(i, i, i, 0xFF));
    }
    else if (format == 4)
    {
        image = QImage(width, height, QImage::Format_RGBA8888);
    }
    else
    {
        qWarning() << "Failed to read .gbr file, unknown format";
        return {};
    }

    for (uint32_t i = 0; i < height; ++i)
        file->read((char *)image.scanLine(i), format * width);
    return image;
}
