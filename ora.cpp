#include "canvascontext.h"
#include "canvastile.h"
#include "ora.h"

#include <QRect>
#include <QDebug>
#include <QtEndian>
#include <QSaveFile>
#include <QBuffer>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <qzipwriter.h>
#include <qzipreader.h>
#include <cstdint>

#include "lodepng.h"
#include "imagefiles.h"

namespace {
QString blendModeToOraOp(BlendMode::Mode mode)
{
    if (mode == BlendMode::Over)
        return QStringLiteral("svg:src-over");
    if (mode == BlendMode::Multiply)
        return QStringLiteral("svg:multiply");
    if (mode == BlendMode::ColorBurn)
        return QStringLiteral("svg:color-burn");
    if (mode == BlendMode::ColorDodge)
        return QStringLiteral("svg:color-dodge");
    if (mode == BlendMode::Screen)
        return QStringLiteral("svg:screen");
    if (mode == BlendMode::Hue)
        return QStringLiteral("svg:hue");
    if (mode == BlendMode::Saturation)
        return QStringLiteral("svg:saturation");
    if (mode == BlendMode::Color)
        return QStringLiteral("svg:color");
    if (mode == BlendMode::Luminosity)
        return QStringLiteral("svg:luminosity");
    if (mode == BlendMode::DestinationIn)
        return QStringLiteral("svg:dst-in");
    if (mode == BlendMode::DestinationOut)
        return QStringLiteral("svg:dst-out");
    if (mode == BlendMode::SourceAtop)
        return QStringLiteral("svg:src-atop");
    if (mode == BlendMode::DestinationAtop)
        return QStringLiteral("svg:dst-atop");
    return QStringLiteral("svg:src-over");
}

BlendMode::Mode oraOpToMode(QString opName)
{
    if (opName == "svg:src-over")
        return BlendMode::Over;
    if (opName == "svg:multiply")
        return BlendMode::Multiply;
    if (opName == "svg:color-burn")
        return BlendMode::ColorBurn;
    if (opName == "svg:color-dodge")
        return BlendMode::ColorDodge;
    if (opName == "svg:screen")
        return BlendMode::Screen;
    if (opName == "svg:hue")
        return BlendMode::Hue;
    if (opName == "svg:saturation")
        return BlendMode::Saturation;
    if (opName == "svg:color")
        return BlendMode::Color;
    if (opName == "svg:luminosity")
        return BlendMode::Luminosity;
    if (opName == "svg:dst-in")
        return BlendMode::DestinationIn;
    if (opName == "svg:dst-out")
        return BlendMode::DestinationOut;
    if (opName == "svg:src-atop")
        return BlendMode::SourceAtop;
    if (opName == "svg:dst-atop")
        return BlendMode::DestinationAtop;
    return BlendMode::Over;
}

void writeBackground(QZipWriter &writer, QString const &path, CanvasTile *tile, QSize const &tileBounds)
{
    QSize resultBounds(TILE_PIXEL_WIDTH * tileBounds.width(),
                       TILE_PIXEL_HEIGHT * tileBounds.height());
    std::unique_ptr<uint16_t> layerData(new uint16_t[resultBounds.width() * resultBounds.height() * 3]);

    // Save the background as an RGB png, MyPaint can't load RGBA backgrounds
    {
        size_t rowComps = resultBounds.width() * 3;

        uint16_t *rowPtr = layerData.get();
        float *tileData = tile->mapHost();

        for (int row = 0; row < resultBounds.height(); row++)
        {
            int tileY = row % TILE_PIXEL_HEIGHT;
            float *srcPtr = tileData + (TILE_PIXEL_WIDTH * 4) * tileY;

            for (int col = 0; col < resultBounds.width(); col++)
            {
                int tileX = row % TILE_PIXEL_WIDTH;

                rowPtr[col * 3 + 0] = qToBigEndian((uint16_t)(srcPtr[tileX * 4 + 0] * 0xFFFF));
                rowPtr[col * 3 + 1] = qToBigEndian((uint16_t)(srcPtr[tileX * 4 + 1] * 0xFFFF));
                rowPtr[col * 3 + 2] = qToBigEndian((uint16_t)(srcPtr[tileX * 4 + 2] * 0xFFFF));
            }

            rowPtr += rowComps;
        }
    }

    unsigned char *pngData = nullptr;
    size_t pngDataSize;
    unsigned int png_error;

    png_error = lodepng_encode_memory(&pngData, &pngDataSize,
                                      (unsigned char*)layerData.get(),
                                      resultBounds.width(), resultBounds.height(),
                                      LCT_RGB, 16);

    if (png_error)
        qWarning() << "lodepng error:" << lodepng_error_text(png_error);

    writer.addFile(path, QByteArray::fromRawData((const char *)pngData, pngDataSize));

    if (pngData)
        free(pngData);
}

void writeStack(QXmlStreamWriter &stackXML,
                QZipWriter &oraZipWriter,
                int imageX,
                int imageY,
                QList<CanvasLayer *> const &layers,
                int &layerNum,
                int &progressStep,
                float progressStepTotal,
                std::function<void(QString const &, float)> progressCallback)
{
    for (int layerIdx = layers.size() - 1; layerIdx >= 0; layerIdx--)
    {
        CanvasLayer const *currentLayer = layers.at(layerIdx);

        if (currentLayer->type == LayerType::Layer)
        {
            QRect tileBounds = tileSetBounds(currentLayer->getTileSet());

            progressCallback(QStringLiteral("Saving layer \"%1\"").arg(currentLayer->name), progressStep++ / progressStepTotal);

            QRect bounds;
            uint16_t *layerData = NULL;

            if (tileBounds.isEmpty())
            {
                bounds = QRect(0, 0, 1, 1);
                layerData = new uint16_t[bounds.width() * bounds.height() * 4];
                memset(layerData, 0, bounds.width() * bounds.height() * 4 * sizeof(uint16_t));
            }
            else
            {
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
                        CanvasTile *tile = currentLayer->getTileMaybe(ix + tileBounds.x(), iy + tileBounds.y());
                        uint16_t *rowPtr = layerData + (rowComps * iy * TILE_PIXEL_HEIGHT)
                                                     + (4 * ix * TILE_PIXEL_WIDTH);
                        if (tile)
                        {
                            float *tileData = tile->mapHost();

                            for (int row = 0; row < TILE_PIXEL_HEIGHT; row++)
                            {
                                for (int col = 0; col < TILE_PIXEL_WIDTH; col++)
                                {

                                    rowPtr[col * 4 + 3] = qToBigEndian((uint16_t)(tileData[col * 4 + 3] * 0xFFFF));
                                    if (rowPtr[col * 4 + 3])
                                    {
                                        rowPtr[col * 4 + 0] = qToBigEndian((uint16_t)(tileData[col * 4 + 0] * 0xFFFF));
                                        rowPtr[col * 4 + 1] = qToBigEndian((uint16_t)(tileData[col * 4 + 1] * 0xFFFF));
                                        rowPtr[col * 4 + 2] = qToBigEndian((uint16_t)(tileData[col * 4 + 2] * 0xFFFF));
                                    }
                                    else
                                    {
                                        rowPtr[col * 4 + 0] = 0;
                                        rowPtr[col * 4 + 1] = 0;
                                        rowPtr[col * 4 + 2] = 0;
                                    }
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

            QString layerFileName = QString().sprintf("data/layer%03d.png", layerNum++);

            oraZipWriter.addFile(layerFileName, QByteArray::fromRawData((const char *)pngData, pngDataSize));

            free(pngData);
            delete[] layerData;

            stackXML.writeStartElement("layer");
            stackXML.writeAttribute("src", layerFileName);
            stackXML.writeAttribute("name", currentLayer->name);
            if (currentLayer->visible)
                stackXML.writeAttribute("visibility", "visible");
            else
                stackXML.writeAttribute("visibility", "hidden");
            if (!currentLayer->editable)
                stackXML.writeAttribute("edit-locked", "true");
            stackXML.writeAttribute("composite-op", blendModeToOraOp(currentLayer->mode));
            stackXML.writeAttribute("opacity", QString().sprintf("%f", currentLayer->opacity));
            stackXML.writeAttribute("x", QString().sprintf("%d", bounds.x() - imageX));
            stackXML.writeAttribute("y", QString().sprintf("%d", bounds.y() - imageY));
            stackXML.writeEndElement(); // layer
        }
        else
        {
            stackXML.writeStartElement("stack");
            stackXML.writeAttribute("isolation", "isolate");
            stackXML.writeAttribute("name", currentLayer->name);
            if (currentLayer->visible)
                stackXML.writeAttribute("visibility", "visible");
            else
                stackXML.writeAttribute("visibility", "hidden");
            if (!currentLayer->editable)
                stackXML.writeAttribute("edit-locked", "true");
            stackXML.writeAttribute("composite-op", blendModeToOraOp(currentLayer->mode));
            stackXML.writeAttribute("opacity", QString().sprintf("%f", currentLayer->opacity));
            writeStack(stackXML, oraZipWriter,
                       imageX, imageY,
                       currentLayer->children, layerNum,
                       progressStep, progressStepTotal, progressCallback);
            stackXML.writeEndElement(); // stack
        }
    }
}
}

void saveStackAs(CanvasStack *stack, QString path, std::function<void(QString const &, float)> progressCallback)
{
    int progressStep = 1;
    float progressStepTotal = (stack->layers.size() + 2) / 100.0f;

    QSaveFile saveFile(path);
    saveFile.open(QIODevice::WriteOnly);
    QZipWriter oraZipWriter(&saveFile);
    oraZipWriter.setCompressionPolicy(QZipWriter::NeverCompress);

    QBuffer stackBuffer;
    stackBuffer.open(QIODevice::ReadWrite);

    QXmlStreamWriter stackXML(&stackBuffer);
    stackXML.setAutoFormatting(true);
    stackXML.writeStartDocument();
    stackXML.writeStartElement("image");

    int layerNum = 0;

    QRect imageTileBounds = tileSetBounds(stack->getTileSet());
    if (imageTileBounds.isEmpty())
        imageTileBounds = QRect(0, 0, 1, 1);

    int imageWidth = imageTileBounds.width() * TILE_PIXEL_WIDTH;
    int imageHeight = imageTileBounds.height() * TILE_PIXEL_HEIGHT;
    int imageX = imageTileBounds.x() * TILE_PIXEL_WIDTH;
    int imageY = imageTileBounds.y() * TILE_PIXEL_HEIGHT;

    stackXML.writeAttribute("w", QString().sprintf("%d", imageWidth));
    stackXML.writeAttribute("h", QString().sprintf("%d", imageHeight));

    stackXML.writeStartElement("stack");

    writeStack(stackXML, oraZipWriter,
               imageX, imageY,
               stack->layers, layerNum,
               progressStep, progressStepTotal, progressCallback);

    {
        // Save the background tile
        CanvasTile *background = stack->backgroundTile.get();
        QString backgroundTilePath = QStringLiteral("data/background_tile.png");
        writeBackground(oraZipWriter, backgroundTilePath, background, QSize(1, 1));
        QString backgroundLayerPath = QStringLiteral("data/background_layer.png");
        writeBackground(oraZipWriter, backgroundLayerPath, background, imageTileBounds.size());

        stackXML.writeStartElement("layer");
        stackXML.writeAttribute("src", backgroundLayerPath);
        stackXML.writeAttribute("name", "background");
        stackXML.writeAttribute("visibility", "visible");
        stackXML.writeAttribute("composite-op", blendModeToOraOp(BlendMode::Over));
        stackXML.writeAttribute("background_tile", backgroundTilePath);
        stackXML.writeEndElement(); // layer
    }

    stackXML.writeEndElement(); // stack
    stackXML.writeEndElement(); // image
    stackXML.writeEndDocument();

    stackBuffer.close();

    progressCallback(QStringLiteral("Saving thumbnail"), progressStep++ / progressStepTotal);

    oraZipWriter.addFile("stack.xml", stackBuffer.buffer());
    QByteArray mimetypeData("image/openraster");
    oraZipWriter.addFile("mimetype", mimetypeData);

    QBuffer mergedImageBuffer;
    QImage mergedImage = stackToImage(stack);
    mergedImage.save(&mergedImageBuffer, "PNG");
    oraZipWriter.addFile("mergedimage.png", mergedImageBuffer.buffer());

    QBuffer thumbImageBuffer;
    QImage thumbImage = mergedImage.scaled(128, 128, Qt::KeepAspectRatio);
    thumbImage.save(&thumbImageBuffer, "PNG");
    oraZipWriter.addFile("Thumbnails/thumbnail.png", thumbImageBuffer.buffer());

    progressCallback(QStringLiteral("Writing ORA data"), progressStep++ / progressStepTotal);

    oraZipWriter.close();
    saveFile.commit();
}

namespace {
CanvasLayer *layerFromLinear(uint16_t *layerData, QRect bounds)
{
    QRect tileBounds = boundingTiles(bounds);
    const size_t dataCompStride = bounds.width() * 4;

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

CanvasTile *backgroundFromLinear(uint16_t *imageData, QSize imageSize)
{
    const size_t dataCompStride = imageSize.width() * 4;

    CanvasTile *result = new CanvasTile();
    float *newTileData = result->mapHost();

    for (int row = 0; row < TILE_PIXEL_HEIGHT; ++row)
        for (int col = 0; col < TILE_PIXEL_WIDTH; ++col)
        {
            int srcX = col % imageSize.width();
            int srcY = row % imageSize.height();
            uint16_t *inPtr = imageData + srcY * dataCompStride + srcX * 4;
            float *outPtr = newTileData + (row * TILE_PIXEL_WIDTH * 4) + (col * 4);

            outPtr[0] = float(qFromBigEndian(inPtr[0])) / 0xFFFF;
            outPtr[1] = float(qFromBigEndian(inPtr[1])) / 0xFFFF;
            outPtr[2] = float(qFromBigEndian(inPtr[2])) / 0xFFFF;
            outPtr[3] = float(qFromBigEndian(inPtr[3])) / 0xFFFF;
        }

    return result;
}

uint16_t *readZipPNG(QZipReader &reader, QString const &path, QSize *resultSize)
{
    *resultSize = QSize(0, 0);

    if (path.isEmpty())
        return nullptr;

    QByteArray layerPNGData = reader.fileData(path);

    if (!layerPNGData.size())
    {
        qDebug() << "Failed to read" << path;
        return nullptr;
    }

    uint16_t *layerData;
    unsigned int layerDataWidth;
    unsigned int layerDataHeight;

    lodepng_decode_memory((unsigned char **)&layerData, &layerDataWidth, &layerDataHeight,
                          (unsigned char *)layerPNGData.constData(), layerPNGData.size(), LCT_RGBA, 16);

    if (!layerData)
    {
        qDebug() << "Failed to decode" << path;
        return nullptr;
    }

    *resultSize = QSize(layerDataWidth, layerDataHeight);
    return layerData;
}

void setLayerAttributes(CanvasLayer *layer,
                        QXmlStreamAttributes const &attributes)
{
    QString mode;
    QString name;
    bool visible = true;
    bool editLocked = false;
    float opacity = 1.0f;


    if (attributes.hasAttribute("name"))
        name = attributes.value("name").toString();

    if (attributes.hasAttribute("visibility"))
        visible = attributes.value("visibility").toString() == QString("visible");

    if (attributes.hasAttribute("edit-locked"))
        editLocked = attributes.value("edit-locked").toString() == QString("true");

    if (attributes.hasAttribute("composite-op"))
        mode = attributes.value("composite-op").toString();

    if (attributes.hasAttribute("opacity"))
        opacity = attributes.value("opacity").toFloat();

    layer->name = name;
    layer->visible = visible;
    layer->editable = !editLocked;
    layer->mode = oraOpToMode(mode);
    layer->opacity = opacity;
}

QList<CanvasLayer *> readStack(QXmlStreamReader &stackXML,
                               std::unique_ptr<CanvasTile> &resultBackgroundTile,
                               QZipReader &oraZipReader)
{
    QList<CanvasLayer *> resultLayers;

    while(!stackXML.atEnd() && !stackXML.hasError())
    {
        QXmlStreamReader::TokenType token = stackXML.readNext();

        if (token == QXmlStreamReader::StartElement &&
            stackXML.name() == "stack")
        {
            CanvasLayer *layerGroup = new CanvasLayer("");
            layerGroup->type = LayerType::Group;
            setLayerAttributes(layerGroup, stackXML.attributes());
            layerGroup->children = readStack(stackXML, resultBackgroundTile, oraZipReader);
            resultLayers.prepend(layerGroup);
        }
        else if (token == QXmlStreamReader::EndElement &&
                 stackXML.name() == "stack")
        {
            return resultLayers;
        }
        else if (token == QXmlStreamReader::StartElement &&
                 stackXML.name() == "layer")
        {
            QXmlStreamAttributes attributes = stackXML.attributes();

            resultBackgroundTile.reset(nullptr);

            int x = 0;
            int y = 0;
            QString src;

            if (attributes.hasAttribute("x"))
                x = attributes.value("x").toDouble();

            if (attributes.hasAttribute("y"))
                y = attributes.value("y").toDouble();

            if (attributes.hasAttribute("src"))
                src = attributes.value("src").toString();

            if (attributes.hasAttribute("background_tile"))
            {
                QString backgroundPath = attributes.value("background_tile").toString();
                QSize layerSize;
                uint16_t *layerData = readZipPNG(oraZipReader, backgroundPath, &layerSize);

                if (layerData)
                {
                    CanvasTile *bgTile = backgroundFromLinear(layerData, layerSize);

                    if (bgTile)
                        resultBackgroundTile.reset(bgTile);

                    free(layerData);
                }
            }

            if (src.isEmpty())
                continue;

            QSize layerSize;

            uint16_t *layerData = readZipPNG(oraZipReader, src, &layerSize);

            if (!layerData)
                continue;

            CanvasLayer *maybeLayer = layerFromLinear(layerData, QRect(x, y, layerSize.width(), layerSize.height()));

            if (maybeLayer)
            {
                setLayerAttributes(maybeLayer, attributes);
                resultLayers.prepend(maybeLayer);
            }

            free(layerData);
        }
    }

    qDebug() << "Unclosed layer stack!";
    return resultLayers;
}
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

    //FIXME: Error check
    while (stackXML.name() != "stack")
        stackXML.readNextStartElement();

    std::unique_ptr<CanvasTile> resultBackgroundTile;
    QList<CanvasLayer *> resultLayers = readStack(stackXML, resultBackgroundTile, oraZipReader);

    if (!resultLayers.empty() && (resultLayers.first()->name == "background") && resultBackgroundTile)
    {
        // qDebug() << "Discarded background layer";
        delete resultLayers.first();
        resultLayers.pop_front();
    }

    if (!resultLayers.empty())
    {
        stack->clearLayers();
        stack->layers = resultLayers;

        if (resultBackgroundTile)
            stack->setBackground(std::move(resultBackgroundTile));
    }
}
