#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include "canvasrender.h"
#include "canvascontext.h"
#include "canvaseventthread.h"
#include "basetool.h"
#include "tiledebugtool.h"
#include "mypainttool.h"
#include "ora.h"
#include "imageexport.h"
#include <cmath>
#include <iostream>
#include <map>
#include <list>
#include <QMouseEvent>
#include <QFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include "glhelper.h"

static const QGLFormat &getFormatSingleton()
{
    static QGLFormat *single = NULL;

    if (!single)
    {
        single = new QGLFormat();
        single->setVersion(3, 2);
        single->setProfile(QGLFormat::CoreProfile);
        single->setDoubleBuffer(false);
        single->setSwapInterval(0);
    }

    return *single;
}

struct SavedTabletEvent
{
    bool valid;
    QPointF pos;
    double pressure;
    ulong timestamp;
};

class CanvasWidgetPrivate
{
public:
    CanvasWidgetPrivate();
    ~CanvasWidgetPrivate();

    QString activeToolPath;
    BaseTool *activeTool;
    QMap<QString, BaseTool *> tools;

    CanvasEventThread eventThread;

    SavedTabletEvent lastTabletEvent;
    ulong strokeEventTimestamp; // last stroke event time, in milliseconds

    int nextFrameDelay;
    QTimer frameTickTrigger;
    QElapsedTimer lastFrameTimer;
};

CanvasWidgetPrivate::CanvasWidgetPrivate()
{
    lastFrameTimer.invalidate();
    nextFrameDelay = 15;
    activeTool = NULL;
    lastTabletEvent = {0, };
    strokeEventTimestamp = 0;
}

CanvasWidgetPrivate::~CanvasWidgetPrivate()
{
    activeTool = NULL;
    for (auto iter = tools.begin(); iter != tools.end(); ++iter)
        delete iter.value();
    tools.clear();
}

CanvasWidget::CanvasWidget(QWidget *parent) :
    QGLWidget(getFormatSingleton(), parent),
    d_ptr(new CanvasWidgetPrivate),
    mouseEventRate(10),
    frameRate(10),
    render(NULL),
    context(NULL),
    action(CanvasAction::None),
    toolColor(QColor::fromRgbF(0.0, 0.0, 0.0)),
    viewScale(1.0f),
    showToolCursor(true),
    lastNewLayerNumber(0),
    modified(false),
    canvasOrigin(0, 0)
{
    Q_D(CanvasWidget);

    setMouseTracking(true);

    colorPickCursor = QCursor(QPixmap(":/cursors/eyedropper.png"));
    moveViewCursor = QCursor(QPixmap(":/cursors/move-view.png"));
    moveLayerCursor = QCursor(QPixmap(":/cursors/move-layer.png"));

    connect(this, &CanvasWidget::updateLayers, this, &CanvasWidget::canvasModified);

    d->frameTickTrigger.setInterval(1);
    d->frameTickTrigger.setSingleShot(true);
    d->frameTickTrigger.setTimerType(Qt::PreciseTimer);

    //FIXME: (As of QT5.2) Old style connect because timeout has QPrivateSignal, this should be fixed in some newer QT version
    connect(&d->frameTickTrigger, SIGNAL(timeout()), this, SLOT(update()));
}

CanvasWidget::~CanvasWidget()
{
    delete render;
    delete context;
}

void CanvasWidget::initializeGL()
{
    Q_D(CanvasWidget);

    context = new CanvasContext();
    render = new CanvasRender();

    SharedOpenCL::getSharedOpenCL();

    render->updateBackgroundTile(context);

    d->eventThread.ctx = context;
    d->eventThread.start();

    newDrawing();
}

void CanvasWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, qMax(w, 1), qMax(h, 1));
}

void CanvasWidget::paintGL()
{
    Q_D(CanvasWidget);
    QOpenGLFunctions_3_2_Core *glFuncs = render->glFuncs;

    qint64 elapsedTime = 0;
    if (d->lastFrameTimer.isValid())
        elapsedTime = d->lastFrameTimer.elapsed();
    else
        d->lastFrameTimer.start();

    if (elapsedTime > d->nextFrameDelay)
    {
        d->nextFrameDelay = 15 - elapsedTime;

        d->lastFrameTimer.restart();
        d->frameTickTrigger.stop();
    }
    else
    {
        if (!d->frameTickTrigger.isActive())
            d->frameTickTrigger.start(15 - elapsedTime);
        return;
    }

    frameRate.addEvents(1);
    emit updateStats();

    {
        d->eventThread.resultTilesMutex.lock();
        for (auto &iter: d->eventThread.resultTiles)
        {
            render->renderTile(iter.first.x(), iter.first.y(), iter.second);
            delete iter.second;
        }
        d->eventThread.resultTiles.clear();
        d->eventThread.resultTilesMutex.unlock();
    }

    // Render dirty tiles after result tiles to avoid stale tiles
    if (CanvasContext *ctx = getContextMaybe())
        render->closeTiles(ctx);

    int widgetWidth  = width();
    int widgetHeight = height();

    // fragmentWidth/Height is the size of a canvas pixel in GL coordinates
    float fragmentWidth = (2.0f / widgetWidth) * viewScale;
    float fragmentHeight = (2.0f / widgetHeight) * viewScale;

    float tileWidth  = TILE_PIXEL_WIDTH * fragmentWidth;
    float tileHeight = TILE_PIXEL_HEIGHT * fragmentHeight;

    float fragmentOriginX = canvasOrigin.x() / viewScale;
    float fragmentOriginY = canvasOrigin.y() / viewScale;

    int originTileX = tile_indice(fragmentOriginX, TILE_PIXEL_WIDTH);
    int originTileY = tile_indice(fragmentOriginY, TILE_PIXEL_HEIGHT);

    float tileShiftX = (fragmentOriginX - originTileX * TILE_PIXEL_WIDTH) * fragmentWidth;
    float tileShiftY = (fragmentOriginY - originTileY * TILE_PIXEL_HEIGHT) * fragmentHeight;

    int tileCountX = ceil((2.0f + tileShiftX) / tileWidth);
    int tileCountY = ceil((2.0f + tileShiftY) / tileHeight);

//    glFuncs->glClearColor(0.2, 0.2, 0.4, 1.0);
//    glFuncs->glClear(GL_COLOR_BUFFER_BIT);

    glFuncs->glUseProgram(render->program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glUniform1i(render->locationTileImage, 0);
    glFuncs->glUniform2f(render->locationTileSize, tileWidth, tileHeight);
    glFuncs->glUniform2f(render->locationTilePixels, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
    glFuncs->glBindVertexArray(render->vertexArray);

    for (int ix = 0; ix < tileCountX; ++ix)
        for (int iy = 0; iy < tileCountY; ++iy)
        {
            float offsetX = (ix * tileWidth) - 1.0f - tileShiftX;
            float offsetY = 1.0f - ((iy + 1) * tileHeight) + tileShiftY;

            GLuint tileBuffer = render->getGLBuf(ix + originTileX, iy + originTileY);
            glFuncs->glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, tileBuffer);
            glFuncs->glUniform2f(render->locationTileOrigin, offsetX, offsetY);
            glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }

    if (d->activeTool && showToolCursor)
    {
        float toolSize = d->activeTool->getPixelRadius() * 2.0f * viewScale;
        if (toolSize < 2.0f)
            toolSize = 2.0f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        QPoint cursorPos = mapFromGlobal(QCursor::pos());
        glFuncs->glUseProgram(render->cursorProgram);
        glFuncs->glUniform4f(render->cursorProgramDimensions,
                             (float(cursorPos.x()) / widgetWidth * 2.0f) - 1.0f,
                             1.0f - (float(cursorPos.y()) / widgetHeight * 2.0f),
                             toolSize / widgetWidth, toolSize / widgetHeight);
        glFuncs->glUniform1f(render->cursorProgramPixelRadius, toolSize / 2.0f);
        glFuncs->glBindVertexArray(render->cursorVertexArray);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisable(GL_BLEND);
    }
}

void CanvasWidget::startStroke(QPointF pos, float pressure)
{
    Q_D(CanvasWidget);
    CanvasContext *ctx = getContext(); //FIXME

    ctx->stroke.reset(NULL);

    if (ctx->layers.layers.empty())
        return;

    if (!d->activeTool)
        return;

    CanvasLayer *targetLayer = ctx->layers.layers.at(ctx->currentLayer);

    if (!targetLayer->visible)
        return;

    ctx->stroke.reset(d->activeTool->newStroke(targetLayer));

    auto msg = [pos, pressure](CanvasContext *ctx) {
        if (!ctx->stroke.isNull())
        {
            TileSet changedTiles = ctx->stroke->startStroke(pos, pressure);

            if(!changedTiles.empty())
            {
                ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
                ctx->strokeModifiedTiles.insert(changedTiles.begin(), changedTiles.end());
            }
        }
    };

    d->eventThread.enqueueCommand(msg);

    modified = true;
    emit canvasModified();
}

void CanvasWidget::strokeTo(QPointF pos, float pressure, float dt)
{
    Q_D(CanvasWidget);

    mouseEventRate.addEvents(1);

    auto msg = [pos, pressure, dt](CanvasContext *ctx) {
        if (!ctx->stroke.isNull())
        {
            TileSet changedTiles = ctx->stroke->strokeTo(pos, pressure, dt);

            if(!changedTiles.empty())
            {
                ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
                ctx->strokeModifiedTiles.insert(changedTiles.begin(), changedTiles.end());
            }
        }
    };

    d->eventThread.enqueueCommand(msg);
}

void CanvasWidget::endStroke()
{
    Q_D(CanvasWidget);
    CanvasContext *ctx = getContext(); //FIXME

    if (ctx->stroke.isNull())
        return;

    ctx->stroke.reset();

    CanvasUndoTiles *undoEvent = new CanvasUndoTiles();
    undoEvent->targetTileMap = ctx->layers.layers[ctx->currentLayer]->tiles;
    undoEvent->currentLayer = ctx->currentLayer;

    CanvasLayer *currentLayerObj = ctx->layers.layers[ctx->currentLayer];

    for (TileSet::iterator iter = ctx->strokeModifiedTiles.begin(); iter != ctx->strokeModifiedTiles.end(); iter++)
    {
        /* Move oldTile to the layer because we will are just going to overwrite the one it currentLayerCopy */
        CanvasTile *oldTile = ctx->currentLayerCopy->getTileMaybe(iter->x(), iter->y());
        undoEvent->tiles[*iter] = oldTile;

        /* FIXME: It's hypothetically possible that the stroke removed tiles */
        (*ctx->currentLayerCopy->tiles)[*iter] = (*currentLayerObj->tiles)[*iter]->copy();
    }

    ctx->clearRedoHistory(); //FIXME: This should probably happen at the start of the stroke
    ctx->undoHistory.prepend(undoEvent);
    ctx->strokeModifiedTiles.clear();
    modified = true;
    emit canvasModified();
}

float CanvasWidget::getScale()
{
    return viewScale;
}

void CanvasWidget::setScale(float newScale)
{
    newScale = qBound(0.25f, newScale, 4.0f);

    if (newScale == viewScale)
        return;

    // If the mouse is over the canvas, keep it over the same pixel after the zoom
    // as it was before. Otherwise keep the center of the canvas in the same position.
    QPoint centerPoint = mapFromGlobal(QCursor::pos());
    if (!rect().contains(centerPoint))
        centerPoint = QPoint(size().width(), size().height()) / 2;

    canvasOrigin = (canvasOrigin + centerPoint) * (newScale / viewScale) - centerPoint;
    viewScale = newScale;
    update();
}

int CanvasWidget::getActiveLayer()
{
    CanvasContext *ctx = getContext();

    if (!ctx)
        return 0;
    else
        return ctx->currentLayer;
}

void CanvasWidget::setActiveLayer(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (!ctx)
        return;

    if (layerIndex >= 0 && layerIndex < ctx->layers.layers.size())
    {
        bool update = (ctx->currentLayer != layerIndex);
        ctx->currentLayer = layerIndex;
        ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());

        if (update)
            emit updateLayers();
    }
}

void CanvasWidget::addLayerAbove(int layerIndex)
{
    CanvasContext *ctx = getContext();

    ctx->clearRedoHistory();
    CanvasUndoLayers *undoEvent = new CanvasUndoLayers(&ctx->layers, ctx->currentLayer);
    ctx->undoHistory.prepend(undoEvent);

    ctx->layers.newLayerAt(layerIndex + 1, QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    ctx->currentLayer = layerIndex + 1;
    ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());
    modified = true;
    emit updateLayers();
}

void CanvasWidget::removeLayer(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex > ctx->layers.layers.size())
        return;
    if (ctx->layers.layers.size() == 1)
        return

    ctx->clearRedoHistory();
    CanvasUndoLayers *undoEvent = new CanvasUndoLayers(&ctx->layers, ctx->currentLayer);
    ctx->undoHistory.prepend(undoEvent);

    /* Before we delete the layer, dirty all tiles it intersects */
    TileSet layerTiles = ctx->layers.layers[layerIndex]->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    ctx->layers.removeLayerAt(layerIndex);
    if (layerIndex == ctx->currentLayer)
    {
        ctx->currentLayer = qMax(0, ctx->currentLayer - 1);
        ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());
    }

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::moveLayer(int currentIndex, int targetIndex)
{
    /* Move the layer at currentIndex to targetIndex, the layers at targetIndex and
     * above will be shifted up to accommodate it.
     */
    CanvasContext *ctx = getContext();

    if (currentIndex < 0 || currentIndex >= ctx->layers.layers.size())
        return;
    if (ctx->layers.layers.size() == 1)
        return;
    if (targetIndex < 0)
        targetIndex = 0;
    if (targetIndex >= ctx->layers.layers.size())
        targetIndex = ctx->layers.layers.size() - 1;

    if (currentIndex == targetIndex)
        return;

    QList<CanvasLayer *> newLayers;

    for (int i = 0; i < ctx->layers.layers.size(); ++i)
        if (i != currentIndex)
            newLayers.append(ctx->layers.layers.at(i));
    newLayers.insert(targetIndex, ctx->layers.layers.at(currentIndex));

    // Generate undo
    ctx->clearRedoHistory();
    CanvasUndoLayers *undoEvent = new CanvasUndoLayers(&ctx->layers, ctx->currentLayer);
    ctx->undoHistory.prepend(undoEvent);

    // Replace the stack's list with the reordered one
    ctx->layers.layers = newLayers;
    ctx->currentLayer = targetIndex;
    ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());

    TileSet layerTiles = ctx->currentLayerCopy->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::renameLayer(int layerIndex, QString name)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    if (ctx->layers.layers[layerIndex]->name == name)
        return;

    ctx->undoHistory.prepend(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    ctx->layers.layers[layerIndex]->name = name;
    modified = true;
    emit updateLayers();
}

void CanvasWidget::duplicateLayer(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    ctx->clearRedoHistory();
    CanvasUndoLayers *undoEvent = new CanvasUndoLayers(&ctx->layers, ctx->currentLayer);
    ctx->undoHistory.prepend(undoEvent);

    CanvasLayer *oldLayer = ctx->layers.layers[layerIndex];
    CanvasLayer *newLayer = oldLayer->deepCopy();
    newLayer->name = oldLayer->name + " Copy";

    ctx->layers.layers.insert(layerIndex + 1, newLayer);
    ctx->currentLayer = layerIndex + 1;
    ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());

    TileSet layerTiles = newLayer->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::updateLayerTranslate(int x,  int y)
{
    CanvasContext *ctx = getContext();

    CanvasLayer *currentLayer = ctx->layers.layers[ctx->currentLayer];
    CanvasLayer *newLayer = ctx->currentLayerCopy->translated(x, y);

    TileSet layerTiles = currentLayer->getTileSet();
    TileSet newLayerTiles = newLayer->getTileSet();
    layerTiles.insert(newLayerTiles.begin(), newLayerTiles.end());

    currentLayer->takeTiles(newLayer);
    delete newLayer;
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
}

void CanvasWidget::translateCurrentLayer(int x,  int y)
{
    CanvasContext *ctx = getContext();

    CanvasLayer *currentLayer = ctx->layers.layers[ctx->currentLayer];
    CanvasLayer *newLayer = ctx->currentLayerCopy->translated(x, y);
    newLayer->prune();

    ctx->clearRedoHistory();

    CanvasUndoTiles *undoEvent = new CanvasUndoTiles();
    undoEvent->targetTileMap = currentLayer->tiles;
    undoEvent->currentLayer = ctx->currentLayer;

    // The undo tiles are the original layer + new layer
    TileSet layerTiles = ctx->currentLayerCopy->getTileSet();
    TileSet newLayerTiles = newLayer->getTileSet();
    layerTiles.insert(newLayerTiles.begin(), newLayerTiles.end());

    for (TileSet::iterator iter = layerTiles.begin(); iter != layerTiles.end(); iter++)
    {
        undoEvent->tiles[*iter] = ctx->currentLayerCopy->takeTileMaybe(iter->x(), iter->y());
    }
    ctx->undoHistory.prepend(undoEvent);

    // The dirty tiles are the (possibly moved) current layer + new layer
    layerTiles = currentLayer->getTileSet();
    layerTiles.insert(newLayerTiles.begin(), newLayerTiles.end());

    currentLayer->takeTiles(newLayer);
    delete newLayer;
    ctx->currentLayerCopy.reset(currentLayer->deepCopy());
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
    modified = true;
}

CanvasContext *CanvasWidget::getContext()
{
    Q_D(CanvasWidget);

    d->eventThread.sync();

    if (!d->eventThread.resultTiles.empty())
        update();

    return context;
}

CanvasContext *CanvasWidget::getContextMaybe()
{
    Q_D(CanvasWidget);

    if (d->eventThread.checkSync())
        return context;
    return NULL;
}

void CanvasWidget::setLayerVisible(int layerIndex, bool visible)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    if (ctx->layers.layers[layerIndex]->visible == visible)
        return;

    ctx->undoHistory.prepend(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    ctx->layers.layers[layerIndex]->visible = visible;

    TileSet layerTiles = ctx->layers.layers[layerIndex]->getTileSet();

    if(!layerTiles.empty())
    {
        ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());
        emit update();
    }

    modified = true;
    emit updateLayers();
}

bool CanvasWidget::getLayerVisible(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return false;

    return ctx->layers.layers[layerIndex]->visible;
}

void CanvasWidget::setLayerMode(int layerIndex, BlendMode::Mode mode)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    if (ctx->layers.layers[layerIndex]->mode == mode)
        return;

    ctx->undoHistory.prepend(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    ctx->layers.layers[layerIndex]->mode = mode;

    TileSet layerTiles = ctx->layers.layers[layerIndex]->getTileSet();

    if(!layerTiles.empty())
    {
        ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());
        emit update();
    }

    modified = true;
    emit updateLayers();
}

BlendMode::Mode CanvasWidget::getLayerMode(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return BlendMode::Over;

    return ctx->layers.layers[layerIndex]->mode;
}

QList<CanvasWidget::LayerInfo> CanvasWidget::getLayerList()
{
    CanvasContext *ctx = getContext();
    QList<LayerInfo> result;

    if (!ctx)
        return result;

    for (int i = ctx->layers.layers.size() - 1; i >= 0; --i)
    {
        CanvasLayer *layer = ctx->layers.layers[i];
        result.append((LayerInfo){layer->name, layer->visible, layer->mode});
    }

    return result;
}

void CanvasWidget::undo()
{
    CanvasContext *ctx = getContext();

    if (ctx->undoHistory.empty())
        return;

    if (!ctx->stroke.isNull())
        return;

    int newActiveLayer = ctx->currentLayer;

    CanvasUndoEvent *undoEvent = ctx->undoHistory.first();
    ctx->undoHistory.removeFirst();
    TileSet changedTiles = undoEvent->apply(&ctx->layers, &newActiveLayer);
    ctx->redoHistory.push_front(undoEvent);

    if(!changedTiles.empty())
    {
        ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
    }

    ctx->currentLayer = newActiveLayer;

    //FIXME: This only needs to copy changedTiles or if currentLayer changed
    ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::redo()
{
    CanvasContext *ctx = getContext();

    if (ctx->redoHistory.empty())
        return;

    if (!ctx->stroke.isNull())
        return;

    int newActiveLayer = ctx->currentLayer;

    CanvasUndoEvent *undoEvent = ctx->redoHistory.first();
    ctx->redoHistory.removeFirst();
    TileSet changedTiles = undoEvent->apply(&ctx->layers, &newActiveLayer);
    ctx->undoHistory.push_front(undoEvent);

    if(!changedTiles.empty())
    {
        ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
    }

    ctx->currentLayer = newActiveLayer;

    //FIXME: This only needs to copy changedTiles or if currentLayer changed
    ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());

    update();
    modified = true;
    emit updateLayers();
}

bool CanvasWidget::getModified()
{
    return modified;
}

void CanvasWidget::setModified(bool state)
{
    modified = true;
}

void CanvasWidget::pickColorAt(QPoint pos)
{
    CanvasContext *ctx = getContext();

    int ix = tile_indice(pos.x(), TILE_PIXEL_WIDTH);
    int iy = tile_indice(pos.y(), TILE_PIXEL_HEIGHT);
    CanvasTile *tile = ctx->layers.layers[ctx->currentLayer]->getTileMaybe(ix, iy);

    if (tile)
    {
        int offset = (pos.x() - ix * TILE_PIXEL_WIDTH) +
                     (pos.y() - iy * TILE_PIXEL_HEIGHT) * TILE_PIXEL_WIDTH;
        offset *= sizeof(float) * 4;

        float data[4];
        clEnqueueReadBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue, tile->unmapHost(), CL_TRUE,
                            offset, sizeof(float) * 4, data,
                            0, NULL, NULL);
        if (data[3] > 0.0f)
        {
            setToolColor(QColor::fromRgbF(data[0], data[1], data[2]));
            emit updateTool();
        }
    }
}

void CanvasWidget::keyPressEvent(QKeyEvent *event)
{
    updateModifiers(event);
}

void CanvasWidget::keyReleaseEvent(QKeyEvent *event)
{
    updateModifiers(event);

    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier)) == 0)
    {
        if (event->key() == Qt::Key_Up)
        {
            canvasOrigin.ry() -= 128;
            event->accept();
            update();
        }
        else if (event->key() == Qt::Key_Down)
        {
            canvasOrigin.ry() += 128;
            event->accept();
            update();
        }
        else if (event->key() == Qt::Key_Left)
        {
            canvasOrigin.rx() -= 128;
            event->accept();
            update();
        }
        else if (event->key() == Qt::Key_Right)
        {
            canvasOrigin.rx() += 128;
            event->accept();
            update();
        }
    }
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    Q_D(CanvasWidget);

    QPointF pos = (event->localPos() + canvasOrigin) / viewScale;
    float pressure = 1.0f;
    ulong timestamp = event->timestamp();
    bool wasTablet = false;
    Qt::MouseButton button = event->button();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    modifiers &= Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier;

#ifdef Q_OS_MAC
    // As of Qt 5.3 there is an overlap between ctrl for right click and the meta key on OSX
    if (button == 2 && modifiers.testFlag(Qt::MetaModifier))
        modifiers ^= Qt::MetaModifier;
#endif

    if (d->lastTabletEvent.valid)
    {
        d->lastTabletEvent.valid = false;
        pos = d->lastTabletEvent.pos;
        pressure = d->lastTabletEvent.pressure;
        timestamp = d->lastTabletEvent.timestamp;
        wasTablet = true;
    }

    if (button == 1)
    {
        if (action == CanvasAction::ColorPick)
        {
            pickColorAt(pos.toPoint());
        }
        else if (action == CanvasAction::None)
        {
            d->strokeEventTimestamp = timestamp;

            startStroke(pos, pressure);
            if (wasTablet)
                action = CanvasAction::TabletStroke;
            else
                action = CanvasAction::MouseStroke;
            actionButton = event->button();
        }
    }
    else if (button == 2)
    {
        if (action == CanvasAction::None &&
            modifiers == 0)
        {
            action = CanvasAction::MoveView;
            actionButton = event->button();
            actionOrigin = event->pos();
            setCursor(moveViewCursor);
        }
        //FIXME: This shouldn't depend on the binding for color pick
        //FIXME: This should refuse to move hidden layers
        else if ((action == CanvasAction::None || action == CanvasAction::ColorPick) &&
                 modifiers == Qt::ControlModifier)
        {
            action = CanvasAction::MoveLayer;
            actionButton = event->button();
            actionOrigin = event->pos();
            setCursor(moveLayerCursor);
        }
    }

    event->accept();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(CanvasWidget);

    if (event->button() == actionButton)
    {
        if (action == CanvasAction::MouseStroke)
        {
            endStroke();
            action = CanvasAction::None;
            actionButton = Qt::NoButton;
        }
        else if (action == CanvasAction::MoveView)
        {
            action = CanvasAction::None;
            actionButton = Qt::NoButton;
            unsetCursor();
        }
        else if (action == CanvasAction::MoveLayer)
        {
            QPoint offset = event->pos() - actionOrigin;
            offset /= viewScale;
            action = CanvasAction::None;
            actionButton = Qt::NoButton;
            unsetCursor();

            translateCurrentLayer(offset.x(), offset.y());
        }
    }

    event->accept();
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(CanvasWidget);

    updateModifiers(event);

    if (action == CanvasAction::MouseStroke)
    {
        QPointF pos = (event->localPos() + canvasOrigin) / viewScale;
        ulong newTimestamp = event->timestamp();

        strokeTo(pos, 1.0f, float(newTimestamp) - d->strokeEventTimestamp);

        d->strokeEventTimestamp = newTimestamp;
    }
    else if (action == CanvasAction::MoveView)
    {
        QPoint dPos = event->pos() - actionOrigin;
        actionOrigin = event->pos();
        canvasOrigin -= dPos;
    }
    else if (action == CanvasAction::MoveLayer)
    {
        QPoint offset = event->pos() - actionOrigin;
        offset /= viewScale;

        updateLayerTranslate(offset.x(), offset.y());
    }

    event->accept();

    update();
}

void CanvasWidget::tabletEvent(QTabletEvent *event)
{
    Q_D(CanvasWidget);

    QEvent::Type eventType = event->type();

    float xshift = event->hiResGlobalX() - event->globalX() + canvasOrigin.x();
    float yshift = event->hiResGlobalY() - event->globalY() + canvasOrigin.y();
    QPointF point = QPointF(event->x() + xshift, event->y() + yshift) / viewScale;

    updateModifiers(event);

    if (eventType == QEvent::TabletPress)
    {
        d->lastTabletEvent = {true, point, event->pressure(), event->timestamp()};
        event->ignore();
    }
    else if (eventType == QEvent::TabletRelease &&
             action == CanvasAction::TabletStroke)
    {
        endStroke();
        action = CanvasAction::None;
        event->accept();
    }
    else if (eventType == QEvent::TabletMove &&
             action == CanvasAction::TabletStroke)
    {
        ulong newTimestamp = event->timestamp();
        strokeTo(point, event->pressure(), float(newTimestamp) - d->strokeEventTimestamp);
        d->strokeEventTimestamp = newTimestamp;
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void CanvasWidget::wheelEvent(QWheelEvent *event)
{
    mouseEventRate.addEvents(1);
    canvasOrigin += event->pixelDelta();
    update();
}

void CanvasWidget::leaveEvent(QEvent * event)
{
    showToolCursor = false;
    update();
}

void CanvasWidget::enterEvent(QEvent * event)
{
    showToolCursor = true;
    update();
}

void CanvasWidget::updateModifiers(QInputEvent *event)
{
    Qt::KeyboardModifiers modState;

    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::KeyRelease)
    {
        QKeyEvent *keyEv = static_cast<QKeyEvent *>(event);

        // QKeyEvent overrides the non-virtual modifiers() method, as of Qt 5.2
        // this does not return the same values as the function on QInputEvent.
        modState = keyEv->modifiers();
    }
    else
    {
        modState = event->modifiers();
    }

    if ((action == CanvasAction::None) &&
        (modState == Qt::ControlModifier))
    {
        action = CanvasAction::ColorPick;
        setCursor(colorPickCursor);
    }
    else if ((action == CanvasAction::ColorPick) &&
             (modState != Qt::ControlModifier))
    {
        action = CanvasAction::None;
        unsetCursor();
    }
}

void CanvasWidget::setActiveTool(const QString &toolName)
{
    Q_D(CanvasWidget);

    d->activeToolPath = toolName;

    auto found = d->tools.find(toolName);

    if (found != d->tools.end())
    {
        d->activeTool = found.value();
    }
    else if (toolName.endsWith(".myb"))
    {
        d->activeTool = new MyPaintTool(QString(":/mypaint-tools/") + toolName);
        d->tools[toolName] = d->activeTool;
    }
    else if (toolName == QStringLiteral("debug"))
    {
        d->activeTool = new TileDebugTool();
        d->tools[toolName] = d->activeTool;
    }
    else
    {
        qWarning() << "Unknown tool set \'" << toolName << "\', using debug";
        d->activeToolPath = QStringLiteral("debug");
        d->activeTool = d->tools["debug"];

        if (d->activeTool == NULL)
        {
            d->activeTool = new TileDebugTool();
            d->tools[QStringLiteral("debug")] = d->activeTool;
        }
    }

    if (d->activeTool)
    {
        d->activeTool->setColor(toolColor);
    }

    emit updateTool();
    update();
}

QString CanvasWidget::getActiveTool()
{
    Q_D(CanvasWidget);

    return d->activeToolPath;
}

QList<ToolSettingInfo> CanvasWidget::getToolSettings()
{
    Q_D(CanvasWidget);

    if (d->activeTool)
        return d->activeTool->listToolSettings();
    else
        return QList<ToolSettingInfo>();
}

void CanvasWidget::setToolSetting(const QString &settingName, const QVariant &value)
{
    Q_D(CanvasWidget);

    if (!d->activeTool)
        return;

    d->activeTool->setToolSetting(settingName, value);

    emit updateTool();
    update();
}

QVariant CanvasWidget::getToolSetting(const QString &settingName)
{
    Q_D(CanvasWidget);

    if (!d->activeTool)
        return QVariant();

    return d->activeTool->getToolSetting(settingName);
}

void CanvasWidget::setToolColor(const QColor &color)
{
    Q_D(CanvasWidget);

    toolColor = color;
    if (d->activeTool)
        d->activeTool->setColor(toolColor);

    emit updateTool();
}

QColor CanvasWidget::getToolColor()
{
    return toolColor;
}

void CanvasWidget::newDrawing()
{
    CanvasContext *ctx = getContext();

    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    render->clearTiles();
    lastNewLayerNumber = 0;
    ctx->layers.clearLayers();
    ctx->layers.newLayerAt(0, QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    update();
    modified = false;
    emit updateLayers();
}

void CanvasWidget::openORA(QString path)
{
    CanvasContext *ctx = getContext();

    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    render->clearTiles();
    lastNewLayerNumber = 0;
    loadStackFromORA(&ctx->layers, path);
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    ctx->dirtyTiles = ctx->layers.getTileSet();
    update();
    modified = false;
    emit updateLayers();
}

void CanvasWidget::saveAsORA(QString path)
{
    CanvasContext *ctx = getContext();

    saveStackAs(&ctx->layers, path);
    modified = false;
    emit canvasModified();
}

QImage CanvasWidget::asImage()
{
    CanvasContext *ctx = getContext();

    return stackToImage(&ctx->layers);
}
