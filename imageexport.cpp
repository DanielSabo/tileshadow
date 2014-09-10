#include "imageexport.h"
#include "canvastile.h"
#include <QImage>

QRect tileSetBounds(TileSet objTiles)
{
    QRect tileBounds;
    TileSet::iterator iter;

    if (objTiles.empty())
        tileBounds = QRect(0, 0, 1, 1);
    else
    {
        tileBounds = QRect(objTiles.begin()->x(), objTiles.begin()->y(), 1, 1);
    }

    for(iter = objTiles.begin(); iter != objTiles.end(); iter++)
    {
        tileBounds = tileBounds.united(QRect(iter->x(), iter->y(), 1, 1));
    }

    return tileBounds;
}

QImage stackToImage(CanvasStack *stack)
{
    TileSet stackTiles = stack->getTileSet();
    if (stackTiles.empty())
        return QImage();

    QRect tileBounds = tileSetBounds(stackTiles);
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
