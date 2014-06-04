#include "canvascontext.h"
#include "canvastile.h"
#include "ora.h"

#include <QRect>
#include <QDebug>
#include <QtEndian>
#include <QBuffer>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <qzipwriter.h>
#include <qzipreader.h>

#include "lodepng.h"

void saveStackAs(CanvasStack *stack, QString path)
{
    QList<CanvasLayer *>::iterator layersIter;

    /* FIXME: This should use something like QSaveFile but QZip is not compatible */
    QZipWriter oraZipWriter(path, QIODevice::WriteOnly);
    oraZipWriter.setCompressionPolicy(QZipWriter::NeverCompress);

    QBuffer stackBuffer;
    stackBuffer.open(QIODevice::ReadWrite);

    QXmlStreamWriter stackXML(&stackBuffer);
    stackXML.setAutoFormatting(true);
    stackXML.writeStartDocument();
    stackXML.writeStartElement("image");

    int layerNum = 0;
    int imageWidth;
    int imageHeight;

    {
        QRect tileBounds;
        TileSet imageTiles = stack->getTileSet();
        TileSet::iterator iter;

        if (imageTiles.empty())
            tileBounds = QRect(0, 0, 1, 1);
        else
        {
            tileBounds = QRect(imageTiles.begin()->x(), imageTiles.begin()->y(), 1, 1);
        }

        for(iter = imageTiles.begin(); iter != imageTiles.end(); iter++)
        {
            tileBounds = tileBounds.united(QRect(iter->x(), iter->y(), 1, 1));
        }

        imageWidth = tileBounds.width() * TILE_PIXEL_WIDTH;
        imageHeight = tileBounds.height() * TILE_PIXEL_HEIGHT;
    }

    stackXML.writeAttribute("w", QString().sprintf("%d", imageWidth));
    stackXML.writeAttribute("h", QString().sprintf("%d", imageHeight));

    stackXML.writeStartElement("stack");

    for (layersIter = stack->layers.begin(); layersIter != stack->layers.end(); layersIter++)
    {
        QRect bounds;
        uint16_t *layerData = NULL;

        if ((*layersIter)->tiles->empty())
        {
            bounds = QRect(0, 0, 1, 1);
            layerData = new uint16_t[bounds.width() * bounds.height() * 4];
            memset(layerData, 0, bounds.width() * bounds.height() * 4 * sizeof(uint16_t));
        }
        else
        {
            TileMap::iterator tilesIter;

            tilesIter = (*layersIter)->tiles->begin();

            QRect tileBounds = QRect(tilesIter->first.x(), tilesIter->first.y(), 1, 1);

            tilesIter++;

            for (; tilesIter != (*layersIter)->tiles->end(); tilesIter++)
            {
                tileBounds = tileBounds.united(QRect(tilesIter->first.x(), tilesIter->first.y(), 1, 1));
            }

            bounds = QRect(tileBounds.x() * TILE_PIXEL_WIDTH,
                           tileBounds.y() * TILE_PIXEL_HEIGHT,
                           tileBounds.width() * TILE_PIXEL_WIDTH,
                           tileBounds.height() * TILE_PIXEL_HEIGHT);

            // Linearize image
            layerData = new uint16_t[bounds.width() * bounds.height() * 4];
            size_t rowComps = bounds.width() * 4;

            for (int iy = 0; iy < tileBounds.height(); ++iy)
                for (int ix = 0; ix < tileBounds.width(); ++ix)
                {
                    CanvasTile *tile = (*layersIter)->getTileMaybe(ix + tileBounds.x(), iy + tileBounds.y());
                    uint16_t *rowPtr = layerData + (rowComps * iy * TILE_PIXEL_HEIGHT)
                                                 + (4 * ix * TILE_PIXEL_WIDTH);
                    if (tile)
                    {
                        tile->mapHost();
                        float *tileData = tile->tileData;

                        for (int row = 0; row < TILE_PIXEL_HEIGHT; row++)
                        {
                            for (int col = 0; col < TILE_PIXEL_WIDTH * 4; col++)
                            {
                                rowPtr[col] = qToBigEndian((uint16_t)(tileData[col] * 0xFFFF));
                            }

                            rowPtr += rowComps;
                            tileData += TILE_PIXEL_WIDTH * 4;
                        }
                    }
                    else
                    {
                        for (int row = 0; row < TILE_PIXEL_HEIGHT; row++)
                        {
                            memset(rowPtr, 0, TILE_PIXEL_WIDTH * sizeof(uint16_t) * 4);
                            rowPtr += rowComps;
                        }
                    }
                }
        }

        unsigned char *pngData;
        size_t pngDataSize;

        unsigned int png_error = lodepng_encode_memory(&pngData, &pngDataSize,
                                                       (unsigned char*)layerData,
                                                       bounds.width(), bounds.height(),
                                                       LCT_RGBA, 16);

        if (png_error)
            qDebug() << "lodepng error:" << lodepng_error_text(png_error);

        QString layerFileName = QString().sprintf("layer%03d.png", layerNum++);

        oraZipWriter.addFile(layerFileName, QByteArray::fromRawData((const char *)pngData, pngDataSize));

        free(pngData);
        delete[] layerData;

        stackXML.writeStartElement("layer");
        stackXML.writeAttribute("src", layerFileName);
        stackXML.writeAttribute("name", (*layersIter)->name);
        stackXML.writeAttribute("visibility", "visible");
        stackXML.writeAttribute("composite-op", "svg:src-over");
        stackXML.writeAttribute("opacity", QString().sprintf("%f", 1.0));
        stackXML.writeAttribute("x", QString().sprintf("%d", bounds.x()));
        stackXML.writeAttribute("y", QString().sprintf("%d", bounds.y()));
        stackXML.writeEndElement(); // layer
    }

    stackXML.writeEndElement(); // stack
    stackXML.writeEndElement(); // image
    stackXML.writeEndDocument();

    stackBuffer.close();

    oraZipWriter.addFile("stack.xml", stackBuffer.buffer());
    QByteArray mimetypeData("image/openraster");
    oraZipWriter.addFile("mimetype", mimetypeData);
}

static CanvasLayer *layerFromPNGData(QByteArray data, int xOffset, int yOffset)
{
    return NULL;
}

static CanvasLayer *layerFromLinear(uint16_t *layerData, QRect bounds)
{
    QRect tileBounds;
    const size_t dataCompStride = bounds.width() * 4;

    {
        //FIXME: Bounds are not correct
        int tileX1 = tile_indice(bounds.x(), TILE_PIXEL_WIDTH);
        int tileX2 = tile_indice(bounds.x() + bounds.width(), TILE_PIXEL_WIDTH) + 1;
        int tileY1 = tile_indice(bounds.y(), TILE_PIXEL_HEIGHT);
        int tileY2 = tile_indice(bounds.y() + bounds.height(), TILE_PIXEL_HEIGHT) + 1;

        tileBounds = QRect(tileX1, tileY1, tileX2 - tileX1, tileY2 - tileY1);
    }

    float *newTileData = new float[TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4];
    CanvasLayer *result = new CanvasLayer("");

    for (int iy = 0; iy < tileBounds.height(); ++iy)
        for (int ix = 0; ix < tileBounds.width(); ++ix)
        {
            int realPixels = 0;
            int srcXOffset = (ix + tileBounds.x()) * TILE_PIXEL_WIDTH - bounds.x();
            int srcYOffset = (iy + tileBounds.y()) * TILE_PIXEL_HEIGHT - bounds.y();

            for (int row = 0; row < TILE_PIXEL_HEIGHT; ++row)
                for (int col = 0; col < TILE_PIXEL_WIDTH; ++col)
                {
                    float *outPtr = newTileData + (row * TILE_PIXEL_WIDTH * 4) + (col * 4);
                    int srcX = col + srcXOffset;
                    int srcY = row + srcYOffset;
                    uint16_t *inPtr = layerData + srcY * dataCompStride + srcX * 4;

                    if (bounds.contains(srcX + bounds.x(), srcY + bounds.y()))
                    {
                        // Grab pixel
                        uint16_t inPixel[4];
                        inPixel[0] = qFromBigEndian(inPtr[0]);
                        inPixel[1] = qFromBigEndian(inPtr[1]);
                        inPixel[2] = qFromBigEndian(inPtr[2]);
                        inPixel[3] = qFromBigEndian(inPtr[3]);

                        if (inPixel[0] || inPixel[1] || inPixel[2] || inPixel[3])
                        {
                            realPixels++;
                            outPtr[0] = float(inPixel[0]) / 0xFFFF;
                            outPtr[1] = float(inPixel[1]) / 0xFFFF;
                            outPtr[2] = float(inPixel[2]) / 0xFFFF;
                            outPtr[3] = float(inPixel[3]) / 0xFFFF;
                        }
                        else
                        {
                            outPtr[0] = 0.0f;
                            outPtr[1] = 0.0f;
                            outPtr[2] = 0.0f;
                            outPtr[3] = 0.0f;
                        }
                    }
                    else
                    {
                        // Zero
                        outPtr[0] = 0.0f;
                        outPtr[1] = 0.0f;
                        outPtr[2] = 0.0f;
                        outPtr[3] = 0.0f;
                    }
                }

            if (realPixels)
            {
                memcpy(result->openTileAt(ix + tileBounds.x(), iy + tileBounds.y()),
                       newTileData, TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * sizeof(float) * 4);
            }
        }

    delete[] newTileData;

    return result;
}

void loadStackFromORA(CanvasStack *stack, QString path)
{
    QZipReader oraZipReader(path, QIODevice::ReadOnly);

    if (!oraZipReader.exists())
    {
        qDebug() << "Could not open" << path << "no such file";
        return;
    }

    if (oraZipReader.status() != QZipReader::NoError)
    {
        qDebug() << "Error opening" << path;
        return;
    }

    QByteArray stackData = oraZipReader.fileData("stack.xml");

    if (!stackData.size())
    {
        qDebug() << "Failed to read ORA stack.xml";
        return;
    }

    QXmlStreamReader stackXML(stackData);

    /* Blind parsing of the file, the only thing we care about
     * are layer nodes and we assume they are in a simple stack.
     * An ORA2 file will likely cause explosions.
     */

    QList<CanvasLayer *> resultLayers;

    while(!stackXML.atEnd() && !stackXML.hasError())
    {
        QXmlStreamReader::TokenType token = stackXML.readNext();

        if (token == QXmlStreamReader::StartElement &&
            stackXML.name() == "layer")
        {
            QXmlStreamAttributes attributes = stackXML.attributes();

            int x = 0;
            int y = 0;
            QString name;
            QString src;

            if (attributes.hasAttribute("x"))
                x = attributes.value("x").toDouble();

            if (attributes.hasAttribute("y"))
                y = attributes.value("y").toDouble();

            if (attributes.hasAttribute("name"))
                name = attributes.value("name").toString();

            if (attributes.hasAttribute("src"))
                src = attributes.value("src").toString();

            if (src.isEmpty())
                continue;

//            qDebug() << "Found layer" << x << y << name << src;

            QByteArray layerPNGData = oraZipReader.fileData(src);

            if (!layerPNGData.size())
            {
                qDebug() << "Layer data missing";
                continue;
            }

            uint16_t *layerData;
            unsigned int layerDataWidth;
            unsigned int layerDataHeight;

            lodepng_decode_memory((unsigned char **)&layerData, &layerDataWidth, &layerDataHeight,
                                  (unsigned char *)layerPNGData.constData(), layerPNGData.size(), LCT_RGBA, 16);

            if (!layerData)
            {
                qDebug() << "Failed to decode";
                continue;
            }

            CanvasLayer *maybeLayer = layerFromLinear(layerData, QRect(x, y, layerDataWidth, layerDataHeight));

            if (maybeLayer)
            {
                maybeLayer->name = name;
                resultLayers.prepend(maybeLayer);
            }

            free(layerData);
        }
    }

    if (!resultLayers.empty())
    {
        // FIXME: This should probably just return the list
        while(!stack->layers.empty())
            delete stack->layers.takeAt(0);
        stack->tiles.clear();
        stack->layers = resultLayers;
    }
}
