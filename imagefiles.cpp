#include "imagefiles.h"
#include "canvastile.h"
#include "canvaslayer.h"
#include <QImage>
#include <QDebug>

QImage stackToImage(CanvasStack *stack, QRect frame)
{
    QRect tileBounds;
    frame = frame.normalized();

    if (!frame.isEmpty())
        tileBounds = boundingTiles(frame);
    else
        tileBounds = tileSetBounds(stack->getTileSet());

    if (tileBounds.isEmpty())
        return QImage();

    QRect bounds = QRect(tileBounds.x() * TILE_PIXEL_WIDTH,
                         tileBounds.y() * TILE_PIXEL_HEIGHT,
                         tileBounds.width() * TILE_PIXEL_WIDTH,
                         tileBounds.height() * TILE_PIXEL_HEIGHT);

    QImage result(bounds.width(), bounds.height(), QImage::Format_ARGB32);

    for (int iy = 0; iy < tileBounds.height(); ++iy)
        for (int ix = 0; ix < tileBounds.width(); ++ix)
        {
            float *tileData;
            std::unique_ptr<CanvasTile> tile = stack->getTileMaybe(ix + tileBounds.x(), iy + tileBounds.y());

            if (tile)
                tileData = tile->mapHost();
            else
                tileData = stack->backgroundTile->mapHost();

            for (int row = 0; row < TILE_PIXEL_HEIGHT; row++)
            {
                QRgb *rowPixels = (QRgb *)result.scanLine(iy * TILE_PIXEL_HEIGHT + row);
                rowPixels += ix * TILE_PIXEL_WIDTH;

                for (int col = 0; col < TILE_PIXEL_WIDTH; col++)
                {
                    rowPixels[col] = qRgba(tileData[col * 4 + 0] * 0xFF,
                                           tileData[col * 4 + 1] * 0xFF,
                                           tileData[col * 4 + 2] * 0xFF,
                                           tileData[col * 4 + 3] * 0xFF);
                }

                tileData += TILE_PIXEL_WIDTH * 4;
            }
        }

    if (!frame.isEmpty() && (bounds.size() != frame.size()))
    {
        // Crop the rendered image to the frame
        int x = frame.x() - bounds.x();
        int y = frame.y() - bounds.y();
        result = result.copy(x, y, frame.width(), frame.height());
    }

    return result;
}

QImage layerToImage(CanvasLayer *layer)
{
    QRect tileBounds = tileSetBounds(layer->getTileSet());
    if (tileBounds.isEmpty())
        return QImage();

    QRect bounds = QRect(tileBounds.x() * TILE_PIXEL_WIDTH,
                         tileBounds.y() * TILE_PIXEL_HEIGHT,
                         tileBounds.width() * TILE_PIXEL_WIDTH,
                         tileBounds.height() * TILE_PIXEL_HEIGHT);

    QImage result(bounds.width(), bounds.height(), QImage::Format_ARGB32);

    for (int iy = 0; iy < tileBounds.height(); ++iy)
        for (int ix = 0; ix < tileBounds.width(); ++ix)
        {
            CanvasTile *tile = layer->getTileMaybe(ix + tileBounds.x(), iy + tileBounds.y());

            if (tile)
            {
                float *tileData = tile->mapHost();

                for (int row = 0; row < TILE_PIXEL_HEIGHT; row++)
                {
                    QRgb *rowPixels = (QRgb *)result.scanLine(iy * TILE_PIXEL_HEIGHT + row);
                    rowPixels += ix * TILE_PIXEL_WIDTH;

                    for (int col = 0; col < TILE_PIXEL_WIDTH; col++)
                    {
                        rowPixels[col] = qRgba(tileData[col * 4 + 0] * 0xFF,
                                               tileData[col * 4 + 1] * 0xFF,
                                               tileData[col * 4 + 2] * 0xFF,
                                               tileData[col * 4 + 3] * 0xFF);
                    }

                    tileData += TILE_PIXEL_WIDTH * 4;
                }
            }
            else
            {
                for (int row = 0; row < TILE_PIXEL_HEIGHT; row++)
                {
                    QRgb *rowPixels = (QRgb *)result.scanLine(iy * TILE_PIXEL_HEIGHT + row);
                    rowPixels += ix * TILE_PIXEL_WIDTH;

                    memset(rowPixels, 0, sizeof(QRgb) * TILE_PIXEL_WIDTH);
                }
            }
        }

    return result;
}

std::unique_ptr<CanvasLayer> layerFromImage(QImage image)
{
    image = image.convertToFormat(QImage::Format_ARGB32);
    QRect bounds = QRect(QPoint(0, 0), image.size());
    QRect tileBounds = boundingTiles(bounds);

    std::unique_ptr<float[]> newTileData(new float[TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4]);
    std::unique_ptr<CanvasLayer> result(new CanvasLayer(""));

    for (int iy = 0; iy < tileBounds.height(); ++iy)
        for (int ix = 0; ix < tileBounds.width(); ++ix)
        {
            int realPixels = 0;
            int srcXOffset = (ix + tileBounds.x()) * TILE_PIXEL_WIDTH - bounds.x();
            int srcYOffset = (iy + tileBounds.y()) * TILE_PIXEL_HEIGHT - bounds.y();

            for (int row = 0; row < TILE_PIXEL_HEIGHT; ++row)
                for (int col = 0; col < TILE_PIXEL_WIDTH; ++col)
                {
                    float *outPtr = newTileData.get() + (row * TILE_PIXEL_WIDTH * 4) + (col * 4);
                    int srcX = col + srcXOffset;
                    int srcY = row + srcYOffset;
                    QRgb *inPtr = (QRgb *)(image.scanLine(srcY)) + srcX;

                    if (bounds.contains(srcX + bounds.x(), srcY + bounds.y()))
                    {
                        // Grab pixel
                        uint8_t inPixel[4];
                        inPixel[0] = qRed(*inPtr);
                        inPixel[1] = qGreen(*inPtr);
                        inPixel[2] = qBlue(*inPtr);
                        inPixel[3] = qAlpha(*inPtr);

                        if (inPixel[0] || inPixel[1] || inPixel[2] || inPixel[3])
                        {
                            realPixels++;
                            outPtr[0] = float(inPixel[0]) / 0xFF;
                            outPtr[1] = float(inPixel[1]) / 0xFF;
                            outPtr[2] = float(inPixel[2]) / 0xFF;
                            outPtr[3] = float(inPixel[3]) / 0xFF;
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
                       newTileData.get(), TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * sizeof(float) * 4);
            }
        }

    return result;
}
