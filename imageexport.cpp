#include "imageexport.h"
#include "canvastile.h"
#include <QImage>

QImage stackToImage(CanvasStack *stack)
{
    QRect tileBounds = tileSetBounds(stack->getTileSet());
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

    return result;
}
