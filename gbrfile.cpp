#include "gbrfile.h"
#include <QDataStream>
#include <QString>
#include <QDebug>
#include <QBuffer>

QImage readGBR(QIODevice *file)
{
    if (!file->isOpen())
        file->open(QIODevice::ReadOnly);
    auto startPosition = file->pos();

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

    if ((file->size() - (startPosition + headerLength)) < (format * width * height))
    {
        qWarning() << "Failed to read .gbr file, data too short";
        return {};
    }

    file->seek(startPosition + headerLength);

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

QList<QImage> readGIH(QIODevice *file)
{
    if (!file->isOpen())
        file->open(QIODevice::ReadOnly);

    /* Discard the description */
    file->readLine();
    QByteArray layout = file->readLine();

    /* If there was an error reading numImages will zero */
    int numImages = QString::fromUtf8(layout).section(' ', 0, 0).toInt();

    QList<QImage> result;
    while (numImages--){
        QImage image = readGBR(file);
        if (!image.isNull())
            result.append(image);
    }
    return result;
}

namespace {
void writeGBR_internal(QBuffer &buffer, QImage const image)
{
    QDataStream stream(&buffer);

    uint32_t headerLength = 28+1;
    uint32_t version = 2;
    uint32_t width = image.width();
    uint32_t height = image.height();
    uint32_t format = image.depth() <= 8 ? 1 : 4;
    uint32_t magic = ('G' << 24) + ('I' << 16) + ('M' << 8) + 'P';
    uint32_t spacing = 100;
    uint8_t name = 0;
    stream << headerLength << version << width << height << format << magic << spacing << name;

    if (format == 1)
    {
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
            {
                QRgb p = image.pixel(x, y);
                stream << (uint8_t)qRed(p);
            }
    }
    else
    {
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
            {
                QRgb p = image.pixel(x, y);
                stream << (uint8_t)qRed(p) << (uint8_t)qGreen(p) << (uint8_t)qBlue(p) << (uint8_t)qAlpha(p);
            }
    }
}
}

QByteArray writeGBR(QImage const image)
{
    QByteArray result;
    QBuffer buffer(&result);
    buffer.open(QIODevice::WriteOnly);
    writeGBR_internal(buffer, image);
    buffer.close();

    return result;
}

QByteArray writeGIH(QList<QImage> const images)
{
    if (images.empty())
        return {};

    QByteArray result;
    QBuffer buffer(&result);
    buffer.open(QIODevice::WriteOnly);
    buffer.write("Brush\n", 6);
    // We need to include the number of images but the rest of the parasite string is optional
    buffer.write(QString("%1\n").arg(images.size()).toUtf8());

    for (auto const &image: images)
        writeGBR_internal(buffer, image);
    buffer.close();

    return result;
}
