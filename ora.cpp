#include "canvascontext.h"
#include "ora.h"

#include <QRect>
#include <QDebug>
#include <QtEndian>

#include "lodepng.h"

void saveStackAs(CanvasStack *stack, QString path)
{
    QList<CanvasLayer *>::iterator layersIter;

    for (layersIter = stack->layers.begin(); layersIter != stack->layers.end(); layersIter++)
    {
        if ((*layersIter)->tiles.empty())
            continue;

        TileMap::iterator tilesIter;

        tilesIter = (*layersIter)->tiles.begin();
        int x = tilesIter->first.x();
        int y = tilesIter->first.y();

        QRect tileBounds = QRect(x, y, 1, 1);

        x *= TILE_PIXEL_WIDTH;
        y *= TILE_PIXEL_HEIGHT;

        QRect bounds = QRect(x, y, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
        tilesIter++;

        for (; tilesIter != (*layersIter)->tiles.end(); tilesIter++)
        {
            x = tilesIter->first.x();
            y = tilesIter->first.y();

            tileBounds = tileBounds.united(QRect(x, y, 1, 1));

            x *= TILE_PIXEL_WIDTH;
            y *= TILE_PIXEL_HEIGHT;

            bounds = bounds.united(QRect(x, y, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT));
        }

        qDebug() << "Layer bounds" << bounds;
        qDebug() << "Tile bounds" << tileBounds;

        // Linearize image
        uint16_t *layerData = new uint16_t[bounds.width() * bounds.height() * 4];
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
                        for (int col = 0; col < TILE_PIXEL_WIDTH; col++)
                        {
                            rowPtr[col * 4 + 0] = qToBigEndian((uint16_t)(tileData[col * 4 + 0] * 0xFFFF));
                            rowPtr[col * 4 + 1] = qToBigEndian((uint16_t)(tileData[col * 4 + 1] * 0xFFFF));
                            rowPtr[col * 4 + 2] = qToBigEndian((uint16_t)(tileData[col * 4 + 2] * 0xFFFF));
                            rowPtr[col * 4 + 3] = qToBigEndian((uint16_t)(tileData[col * 4 + 3] * 0xFFFF));
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

        lodepng_encode_file("/Users/argon/Prog/Qt/layer.png", (unsigned char*)layerData,
                            bounds.width(), bounds.height(),
                            LCT_RGBA, 16);

        delete[] layerData;
    }
}
