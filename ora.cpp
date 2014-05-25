#include "canvascontext.h"
#include "ora.h"

#include <QRect>
#include <QDebug>
#include <QtEndian>
#include <QBuffer>
#include <QXmlStreamWriter>
#include <qzipwriter.h>

#include "lodepng.h"

void saveStackAs(CanvasStack *stack, QString path)
{
    QList<CanvasLayer *>::iterator layersIter;

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

        if ((*layersIter)->tiles.empty())
        {
            bounds = QRect(0, 0, 1, 1);
            layerData = new uint16_t[bounds.width() * bounds.height() * 4];
            memset(layerData, 0, bounds.width() * bounds.height() * 4 * sizeof(uint16_t));
        }
        else
        {
            TileMap::iterator tilesIter;

            tilesIter = (*layersIter)->tiles.begin();

            QRect tileBounds = QRect(tilesIter->first.x(), tilesIter->first.y(), 1, 1);

            tilesIter++;

            for (; tilesIter != (*layersIter)->tiles.end(); tilesIter++)
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

        lodepng_encode_memory(&pngData, &pngDataSize,
                              (unsigned char*)layerData,
                              bounds.width(), bounds.height(),
                              LCT_RGBA, 16);

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
