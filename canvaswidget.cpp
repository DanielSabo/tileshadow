#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include "canvasrender.h"
#include "canvascontext.h"
#include "canvaseventthread.h"
#include "basetool.h"
#include "ora.h"
#include "imagefiles.h"
#include "toolfactory.h"
#include <list>
#include <qmath.h>
#include <QApplication>
#include <QProgressDialog>
#include <QRegExp>
#include <QMouseEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QAction>
#include <QDebug>

static const QGLFormat &getFormatSingleton()
{
    static QGLFormat *single = nullptr;

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

namespace RenderMode
{
    enum {
        Normal,
        FlashLayer
    } typedef Mode;
}

struct SavedTabletEvent
{
    bool valid;
    QPointF pos;
    double pressure;
    ulong timestamp;
};

struct CanvasStrokePoint
{
    float x;
    float y;
    float p;
    float dt;
};

class CanvasWidgetPrivate
{
public:
    CanvasWidgetPrivate();
    ~CanvasWidgetPrivate();

    QString penToolPath;
    QString eraserToolPath;

    QString activeToolPath;
    BaseTool *activeTool;
    QMap<QString, BaseTool *> tools;

    CanvasEventThread eventThread;

    bool currentLayerEditable;

    bool deviceIsEraser;
    CanvasAction::Action primaryAction;
    Qt::KeyboardModifiers keyModifiers;
    SavedTabletEvent lastTabletEvent;
    ulong strokeEventTimestamp; // last stroke event time, in milliseconds
    std::list<CanvasStrokePoint> savedStrokePoints;

    int nextFrameDelay;
    QTimer frameTickTrigger;
    QElapsedTimer lastFrameTimer;
    bool fullRedraw;
    RenderMode::Mode renderMode;
    QTimer layerFlashTimeout;
    QColor dotPreviewColor;
    std::shared_ptr<QAtomicInt> motionCoalesceToken;

    CanvasAction::Action actionForMouseEvent(int button, Qt::KeyboardModifiers modifiers);
};

CanvasWidgetPrivate::CanvasWidgetPrivate()
{
    lastFrameTimer.invalidate();
    nextFrameDelay = 15;
    activeTool = nullptr;
    deviceIsEraser = false;
    primaryAction = CanvasAction::MouseStroke;
    keyModifiers = 0;
    lastTabletEvent = {0, };
    strokeEventTimestamp = 0;
    fullRedraw = true;
    currentLayerEditable = false;
    renderMode = RenderMode::Normal;
    layerFlashTimeout.setSingleShot(true);
    dotPreviewColor = QColor();
}

CanvasWidgetPrivate::~CanvasWidgetPrivate()
{
    activeTool = nullptr;
    for (auto iter = tools.begin(); iter != tools.end(); ++iter)
        delete iter.value();
    tools.clear();
}


CanvasAction::Action CanvasWidgetPrivate::actionForMouseEvent(int button, Qt::KeyboardModifiers modifiers)
{
#ifdef Q_OS_MAC
    // If Meta is active button 1 will becomes button 2 when clicked
    if (button == 1 && modifiers.testFlag(Qt::MetaModifier))
        button = 2;
    modifiers &= Qt::ShiftModifier | Qt::ControlModifier;
#else
    modifiers &= Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier;
#endif

    if (button == 1)
    {
        if (modifiers == 0)
            return primaryAction;
        else if (modifiers == Qt::ControlModifier)
            return CanvasAction::ColorPick;
    }
    else if (button == 2)
    {
        if (modifiers == 0)
            return CanvasAction::MoveView;
        else if (modifiers == Qt::ControlModifier)
            return CanvasAction::MoveLayer;
    }

    return CanvasAction::None;
}

CanvasWidget::CanvasWidget(QWidget *parent) :
    QGLWidget(getFormatSingleton(), parent),
    mouseEventRate(10),
    frameRate(10),
    d_ptr(new CanvasWidgetPrivate),
    render(nullptr),
    context(nullptr),
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

    QApplication::instance()->installEventFilter(this);

    colorPickCursor = QCursor(QPixmap(":/cursors/eyedropper.png"));
    moveViewCursor = QCursor(QPixmap(":/cursors/move-view.png"));
    moveLayerCursor = QCursor(QPixmap(":/cursors/move-layer.png"));

    connect(this, &CanvasWidget::updateLayers, this, &CanvasWidget::canvasModified);

    d->frameTickTrigger.setInterval(1);
    d->frameTickTrigger.setSingleShot(true);
    d->frameTickTrigger.setTimerType(Qt::PreciseTimer);

    //FIXME: (As of QT5.2) Old style connect because timeout has QPrivateSignal, this should be fixed in some newer QT version
    connect(&d->frameTickTrigger, SIGNAL(timeout()), this, SLOT(update()));
    connect(&d->layerFlashTimeout, SIGNAL(timeout()), this, SLOT(endLayerFlash()));

    connect(&d->eventThread, SIGNAL(hasResultTiles()), this, SLOT(update()), Qt::QueuedConnection);

    d->penToolPath = ToolFactory::defaultToolName();
    d->eraserToolPath = ToolFactory::defaultEraserName();
    setActiveTool(d->penToolPath);

    auto addShiftAction = [this, d](Qt::Key key, QPoint shift)
    {
        QAction *shiftAction = new QAction(this);
        shiftAction->setShortcut(key);
        connect(shiftAction, &QAction::triggered, [this, d, shift](){
            canvasOrigin += shift;
            d->fullRedraw = true;
            update();
        });
        addAction(shiftAction);
    };

    addShiftAction(Qt::Key_Up, QPoint(0, -128));
    addShiftAction(Qt::Key_Down, QPoint(0, 128));
    addShiftAction(Qt::Key_Left, QPoint(-128, 0));
    addShiftAction(Qt::Key_Right, QPoint(128, 0));
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
    Q_D(CanvasWidget);

    render->resizeFramebuffer(w, h);
    d->fullRedraw = true;
}

void CanvasWidget::paintEvent(QPaintEvent *)
{
    Q_D(CanvasWidget);

    if (!updatesEnabled() || !isValid())
        return;

    if (!d->lastFrameTimer.isValid())
    {
        d->lastFrameTimer.start();
        glDraw();
    }
    else
    {
        qint64 elapsedTime = d->lastFrameTimer.elapsed();

        if (elapsedTime >= d->nextFrameDelay)
        {
            d->nextFrameDelay = 30 - elapsedTime;

            d->lastFrameTimer.restart();
            d->frameTickTrigger.stop();
            glDraw();
        }
        else
        {
            if (!d->frameTickTrigger.isActive())
                d->frameTickTrigger.start(qMax<int>(30 - elapsedTime, 0));
        }
    }
}

void CanvasWidget::paintGL()
{
    Q_D(CanvasWidget);
    QOpenGLFunctions_3_2_Core *glFuncs = render->glFuncs;

    frameRate.addEvents(1);
    // Emit updateStats() outside of the paintEvent
    QMetaObject::invokeMethod(this, "updateStats", Qt::QueuedConnection);

    TileMap renderTiles;
    TileSet tilesToDraw;

    if (d->renderMode == RenderMode::FlashLayer)
    {
        CanvasContext *ctx = getContext();
        if (ctx->flashStack)
        {
            TileSet flashSet = ctx->flashStack->getTileSet();
            for (auto iter: flashSet)
                renderTiles[iter] = ctx->flashStack->getTileMaybe(iter.x(), iter.y());
            tilesToDraw = ctx->layers.getTileSet();
            ctx->flashStack.reset();
        }
    }
    else
    {
        d->eventThread.resultTilesMutex.lock();
        d->eventThread.resultTiles.swap(renderTiles);
        d->eventThread.needResultTiles = true;
        d->eventThread.resultTilesMutex.unlock();

        // Render dirty tiles after result tiles to avoid stale tiles
        if (CanvasContext *ctx = getContextMaybe())
        {
            for (auto iter: ctx->dirtyTiles)
                renderTiles[iter] = ctx->layers.getTileMaybe(iter.x(), iter.y());
            ctx->dirtyTiles.clear();
        }

        //FIXME: Bounds check tiles to view
        if (!d->fullRedraw)
            for (auto iter = renderTiles.cbegin(); iter != renderTiles.cend(); ++iter)
                tilesToDraw.insert(iter->first);
    }

    render->renderTileMap(renderTiles);

    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, render->backbufferFramebuffer);
    glViewport(0, 0, render->viewSize.width(), render->viewSize.height());

    int widgetWidth  = render->viewSize.width();
    int widgetHeight = render->viewSize.height();

    int originTileX = tile_indice(canvasOrigin.x() / viewScale, TILE_PIXEL_WIDTH);
    int originTileY = tile_indice(canvasOrigin.y() / viewScale, TILE_PIXEL_HEIGHT);

    float worldTileWidth = (2.0f / widgetWidth) * viewScale * TILE_PIXEL_WIDTH;
    float worldOriginX = -canvasOrigin.x() * (2.0f / widgetWidth) - 1.0f;

    float worldTileHeight = (2.0f / widgetHeight) * viewScale * TILE_PIXEL_HEIGHT;
    float worldOriginY = canvasOrigin.y() * (2.0f / widgetHeight) - worldTileHeight + 1.0f;

    int tileCountX = ceil(widgetWidth / (TILE_PIXEL_WIDTH * viewScale)) + 1;
    int tileCountY = ceil(widgetHeight / (TILE_PIXEL_HEIGHT * viewScale)) + 1;

//    glFuncs->glClearColor(0.2, 0.2, 0.4, 1.0);
//    glFuncs->glClear(GL_COLOR_BUFFER_BIT);

    int zoomFactor = viewScale >= 1.0f ? 1 : 1 / viewScale;

    auto const &tileShader = render->tileShader;

    glFuncs->glUseProgram(tileShader.program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glUniform1i(tileShader.tileImage, 0);
    glFuncs->glUniform2f(tileShader.tileSize, worldTileWidth, worldTileHeight);
    glFuncs->glUniform2f(tileShader.tilePixels, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
    glFuncs->glUniform1i(tileShader.binSize, zoomFactor);
    glFuncs->glBindVertexArray(tileShader.vertexArray);

    auto drawOneTile = [&](int ix, int iy) {
        float offsetX = ix * worldTileWidth + worldOriginX;
        float offsetY = worldOriginY - iy * worldTileHeight;

        GLuint tileBuffer = render->getGLBuf(ix, iy);
        glFuncs->glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, tileBuffer);
        glFuncs->glUniform2f(tileShader.tileOrigin, offsetX, offsetY);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    };

    if (d->fullRedraw)
    {
        d->fullRedraw = false;
        for (int ix = originTileX; ix < tileCountX + originTileX; ++ix)
            for (int iy = originTileY; iy < tileCountY + originTileY; ++iy)
                drawOneTile(ix, iy);
    }
    else
    {
        for (QPoint const &iter: tilesToDraw)
            drawOneTile(iter.x(), iter.y());
    }

    glFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GL_NONE);
    glFuncs->glBlitFramebuffer(0, 0, render->viewSize.width(), render->viewSize.height(),
                               0, 0, render->viewSize.width(), render->viewSize.height(),
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);

    if (d->activeTool && d->currentLayerEditable && showToolCursor)
    {
        float toolSize = d->activeTool->getPixelRadius() * 2.0f * viewScale;
        if (toolSize < 6.0f)
            toolSize = 6.0f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        QPoint cursorPos = mapFromGlobal(QCursor::pos());
        glFuncs->glUseProgram(render->cursorShader.program);
        glFuncs->glUniform4f(render->cursorShader.dimensions,
                             (float(cursorPos.x()) / widgetWidth * 2.0f) - 1.0f,
                             1.0f - (float(cursorPos.y()) / widgetHeight * 2.0f),
                             toolSize / widgetWidth, toolSize / widgetHeight);
        glFuncs->glUniform1f(render->cursorShader.pixelRadius, toolSize / 2.0f);
        glFuncs->glBindVertexArray(render->cursorShader.vertexArray);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisable(GL_BLEND);
    }

    if (d->dotPreviewColor.isValid())
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float dotRadius = 10.5f;
        glFuncs->glUseProgram(render->colorDotShader.program);
        glFuncs->glUniform4f(render->colorDotShader.previewColor,
                             d->dotPreviewColor.redF(),
                             d->dotPreviewColor.greenF(),
                             d->dotPreviewColor.blueF(),
                             d->dotPreviewColor.alphaF());
        glFuncs->glUniform1f(render->colorDotShader.pixelRadius, dotRadius);
        glFuncs->glBindVertexArray(render->colorDotShader.vertexArray);

        float dotHeight = dotRadius * 2.0f / widgetHeight;
        float dotWidth  = dotRadius * 2.0f / widgetWidth;

        float xShift = dotWidth * 6.0f;
        float yShift = dotHeight * 6.0f;

        glFuncs->glUniform4f(render->colorDotShader.dimensions,
                             0.0f, 0.0f, dotWidth, dotHeight);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        glFuncs->glUniform4f(render->colorDotShader.dimensions,
                             -xShift, 0.0f, dotWidth, dotHeight);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        glFuncs->glUniform4f(render->colorDotShader.dimensions,
                             xShift, 0.0f, dotWidth, dotHeight);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        glFuncs->glUniform4f(render->colorDotShader.dimensions,
                             0.0f, -yShift, dotWidth, dotHeight);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        glFuncs->glUniform4f(render->colorDotShader.dimensions,
                             0.0f, yShift, dotWidth, dotHeight);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisable(GL_BLEND);
    }
}

void CanvasWidget::startStroke(QPointF pos, float pressure)
{
    Q_D(CanvasWidget);

    if (!d->activeTool)
        return;

    if (!d->currentLayerEditable)
        return;

    BaseTool *strokeTool = d->activeTool->clone();

    if (strokeTool->coalesceMovement())
        d->motionCoalesceToken.reset(new QAtomicInt(0));
    else
        d->motionCoalesceToken.reset();

    auto msg = [strokeTool, pos, pressure](CanvasContext *ctx) {
        ctx->strokeTool.reset(strokeTool);
        ctx->stroke.reset(nullptr);

        if (ctx->layers.layers.empty())
            return;

        CanvasLayer *targetLayer = ctx->layers.layers.at(ctx->currentLayer);

        if (!targetLayer->visible)
            return;

        StrokeContextArgs args = {targetLayer, ctx->currentLayerCopy.get()};
        ctx->stroke.reset(strokeTool->newStroke(args));

        TileSet changedTiles = ctx->stroke->startStroke(pos, pressure);

        if(!changedTiles.empty())
        {
            ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
            ctx->strokeModifiedTiles.insert(changedTiles.begin(), changedTiles.end());
        }
    };

    d->eventThread.enqueueCommand(msg);

    d->savedStrokePoints.clear();
    d->savedStrokePoints.push_back({float(pos.x()), float(pos.y()), pressure, 0.0f});

    modified = true;
    emit canvasModified();
}

void CanvasWidget::strokeTo(QPointF pos, float pressure, float dt)
{
    Q_D(CanvasWidget);

    mouseEventRate.addEvents(1);

    if (d->motionCoalesceToken)
        d->motionCoalesceToken->ref();

    auto coalesceToken = d->motionCoalesceToken;

    auto msg = [pos, pressure, dt, coalesceToken](CanvasContext *ctx) {
        if (ctx->stroke)
        {
            if (coalesceToken && coalesceToken->deref() == true)
                return;

            TileSet changedTiles = ctx->stroke->strokeTo(pos, pressure, dt);

            if(!changedTiles.empty())
            {
                ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
                ctx->strokeModifiedTiles.insert(changedTiles.begin(), changedTiles.end());
            }
        }
    };

    d->eventThread.enqueueCommand(msg);

    d->savedStrokePoints.push_back({float(pos.x()), float(pos.y()), pressure, dt});
}

static void transferUndoEventTiles(CanvasUndoTiles *undoEvent,
                                   TileSet const &modifiedTiles,
                                   CanvasLayer *originalLayer,
                                   CanvasLayer const *modifiedLayer)
{
    /* Move modified tiles from the backup layer to the event, and from the new layer to the backup */
    for (QPoint const &iter : modifiedTiles)
    {
        std::unique_ptr<CanvasTile> oldTile = originalLayer->takeTileMaybe(iter.x(), iter.y());

        if (oldTile)
            oldTile->swapHost();

        undoEvent->tiles[iter] = std::move(oldTile);

        if (CanvasTile *newTile = modifiedLayer->getTileMaybe(iter.x(), iter.y()))
            (*originalLayer->tiles)[iter] = newTile->copy();
    }
}

void CanvasWidget::endStroke()
{
    Q_D(CanvasWidget);

    auto msg = [](CanvasContext *ctx) {
        if (ctx->stroke)
        {
            ctx->stroke.reset();
            ctx->strokeTool.reset();

            CanvasLayer *currentLayerObj = ctx->layers.layers[ctx->currentLayer];
            CanvasUndoTiles *undoEvent = new CanvasUndoTiles();
            undoEvent->targetTileMap = currentLayerObj->tiles;
            undoEvent->currentLayer = ctx->currentLayer;
            transferUndoEventTiles(undoEvent, ctx->strokeModifiedTiles, ctx->currentLayerCopy.get(), currentLayerObj);

            ctx->addUndoEvent(undoEvent);
            ctx->strokeModifiedTiles.clear();
        }
    };

    d->eventThread.enqueueCommand(msg);
}

void CanvasWidget::startLine()
{
    Q_D(CanvasWidget);

    BaseTool *strokeTool = nullptr;

    if (d->activeTool && d->currentLayerEditable)
        strokeTool = d->activeTool->clone();

    d->motionCoalesceToken.reset(new QAtomicInt(0));

    auto msg = [strokeTool](CanvasContext *ctx) {
        ctx->strokeTool.reset(strokeTool);
        ctx->stroke.reset();
        ctx->strokeModifiedTiles.clear();

        if (ctx->layers.layers.empty())
            ctx->strokeTool.reset();
    };

    d->eventThread.enqueueCommand(msg);

    modified = true;
    emit canvasModified();
}

void CanvasWidget::lineTo(QPointF start, QPointF end)
{
    Q_D(CanvasWidget);

    mouseEventRate.addEvents(1);

    d->motionCoalesceToken->ref();

    auto coalesceToken = d->motionCoalesceToken;

    auto msg = [start, end, coalesceToken](CanvasContext *ctx) {
        if (!ctx->strokeTool)
            return;

        if (coalesceToken && coalesceToken->deref() == true)
            return;

        CanvasLayer *targetLayer = ctx->layers.layers.at(ctx->currentLayer);

        for (QPoint const &iter : ctx->strokeModifiedTiles)
        {
            CanvasTile *oldTile = ctx->currentLayerCopy->getTileMaybe(iter.x(), iter.y());

            if (oldTile)
                (*targetLayer->tiles)[iter] = oldTile->copy();
            else
                targetLayer->tiles->erase(iter);

            ctx->dirtyTiles.insert(iter);
        }

        ctx->strokeModifiedTiles.clear();

        StrokeContextArgs args = {targetLayer, ctx->currentLayerCopy.get()};
        std::unique_ptr<StrokeContext> stroke(ctx->strokeTool->newStroke(args));


        float dt = QLineF(start, end).length() * 0.5f;
        TileSet startTiles  = stroke->startStroke(start, 0.5f);
        TileSet strokeTiles = stroke->strokeTo(end, 0.5f, dt);
        strokeTiles.insert(startTiles.begin(), startTiles.end());

        if(!strokeTiles.empty())
        {
            ctx->dirtyTiles.insert(strokeTiles.begin(), strokeTiles.end());
            ctx->strokeModifiedTiles.insert(strokeTiles.begin(), strokeTiles.end());
        }
    };

    d->eventThread.enqueueCommand(msg);
}

void CanvasWidget::endLine()
{
    Q_D(CanvasWidget);

    auto msg = [](CanvasContext *ctx) {
        ctx->strokeTool.reset();
        ctx->stroke.reset();

        if (ctx->strokeModifiedTiles.empty())
            return;

        CanvasLayer *currentLayerObj = ctx->layers.layers[ctx->currentLayer];
        CanvasUndoTiles *undoEvent = new CanvasUndoTiles();
        undoEvent->targetTileMap = currentLayerObj->tiles;
        undoEvent->currentLayer = ctx->currentLayer;
        transferUndoEventTiles(undoEvent, ctx->strokeModifiedTiles, ctx->currentLayerCopy.get(), currentLayerObj);

        ctx->addUndoEvent(undoEvent);
        ctx->strokeModifiedTiles.clear();
    };

    d->eventThread.enqueueCommand(msg);
}

float CanvasWidget::getScale()
{
    return viewScale;
}

void CanvasWidget::setScale(float newScale)
{
    Q_D(CanvasWidget);

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

    d->fullRedraw = true;
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
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerIndex >= 0 && layerIndex < ctx->layers.layers.size())
    {
        bool update = (ctx->currentLayer != layerIndex);
        resetCurrentLayer(ctx, layerIndex);

        if (update)
            emit updateLayers();
    }
}

void CanvasWidget::addLayerAbove(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    ctx->layers.newLayerAt(layerIndex + 1, QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    resetCurrentLayer(ctx, layerIndex + 1);
    modified = true;
    emit updateLayers();
}


void CanvasWidget::addLayerAbove(int layerIndex, QImage image, QString name)
{
    CanvasContext *ctx = getContext();

    if (image.isNull())
        return;

    std::unique_ptr<CanvasLayer> imported = layerFromImage(image);

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    if (name.isEmpty())
        name = QString().sprintf("Layer %02d", ++lastNewLayerNumber);

    ctx->layers.newLayerAt(layerIndex + 1, name);
    CanvasLayer *insertedLayer = ctx->layers.layers.at(layerIndex + 1);
    insertedLayer->takeTiles(imported.get());
    resetCurrentLayer(ctx, layerIndex + 1);
    TileSet layerTiles = insertedLayer->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::removeLayer(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex > ctx->layers.layers.size())
        return;
    if (ctx->layers.layers.size() == 1)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    /* Before we delete the layer, dirty all tiles it intersects */
    TileSet layerTiles = ctx->layers.layers[layerIndex]->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    ctx->layers.layers[layerIndex]->swapOut();

    ctx->layers.removeLayerAt(layerIndex);
    if (layerIndex == ctx->currentLayer)
    {
        resetCurrentLayer(ctx, qMax(0, ctx->currentLayer - 1));
    }

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::moveLayer(int currentIndex, int targetIndex)
{
    if (action != CanvasAction::None)
        return;

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
    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    // Replace the stack's list with the reordered one
    ctx->layers.layers = newLayers;
    resetCurrentLayer(ctx, targetIndex);

    TileSet layerTiles = ctx->currentLayerCopy->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::renameLayer(int layerIndex, QString name)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    if (ctx->layers.layers[layerIndex]->name == name)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    ctx->layers.layers[layerIndex]->name = name;
    modified = true;
    emit updateLayers();
}

void CanvasWidget::mergeLayerDown(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    auto &layerList = ctx->layers.layers;

    if (layerIndex < 1 || layerIndex >= layerList.size())
        return;

    if (layerList.size() <= 1)
        return;

    CanvasLayer *above = layerList[layerIndex];
    CanvasLayer *below = layerList[layerIndex - 1];

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    CanvasLayer *merged = above->mergeDown(below).release();

    delete layerList.takeAt(layerIndex - 1);
    delete layerList.takeAt(layerIndex - 1);
    layerList.insert(layerIndex - 1, merged);
    resetCurrentLayer(ctx, layerIndex - 1);

    TileSet layerTiles = merged->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::duplicateLayer(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    CanvasLayer *oldLayer = ctx->layers.layers[layerIndex];
    CanvasLayer *newLayer = oldLayer->deepCopy().release();
    newLayer->name = oldLayer->name + " Copy";

    ctx->layers.layers.insert(layerIndex + 1, newLayer);
    resetCurrentLayer(ctx, layerIndex + 1);

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
    std::unique_ptr<CanvasLayer> newLayer = ctx->currentLayerCopy->translated(x, y);

    TileSet layerTiles = currentLayer->getTileSet();
    TileSet newLayerTiles = newLayer->getTileSet();
    layerTiles.insert(newLayerTiles.begin(), newLayerTiles.end());

    currentLayer->takeTiles(newLayer.get());
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    update();
}

void CanvasWidget::translateCurrentLayer(int x,  int y)
{
    CanvasContext *ctx = getContext();

    CanvasLayer *currentLayer = ctx->layers.layers[ctx->currentLayer];
    std::unique_ptr<CanvasLayer> newLayer = ctx->currentLayerCopy->translated(x, y);
    newLayer->prune();

    CanvasUndoTiles *undoEvent = new CanvasUndoTiles();
    undoEvent->targetTileMap = currentLayer->tiles;
    undoEvent->currentLayer = ctx->currentLayer;

    /* Add the layers currently visible tiles (which may differ from the undo tiles) to the dirty list */
    TileSet layerTiles = currentLayer->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());
    /* Add the newly translated tiles*/
    layerTiles = newLayer->getTileSet();
    ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());

    // The undo tiles are the original layer + new layer
    TileSet originalTiles = ctx->currentLayerCopy->getTileSet();
    layerTiles.insert(originalTiles.begin(), originalTiles.end());

    currentLayer->takeTiles(newLayer.get());

    transferUndoEventTiles(undoEvent, layerTiles, ctx->currentLayerCopy.get(), currentLayer);
    ctx->addUndoEvent(undoEvent);

    update();
    modified = true;
}

void CanvasWidget::resetCurrentLayer(CanvasContext *ctx, int index)
{
    Q_D(CanvasWidget);

    if (index >= 0 && index < ctx->layers.layers.size())
        ctx->currentLayer = index;
    else
        qWarning() << "Invalid layer index" << index;

    CanvasLayer *currentLayerObj = ctx->layers.layers[ctx->currentLayer];
    ctx->currentLayerCopy = currentLayerObj->deepCopy();

    d->currentLayerEditable = currentLayerObj->visible && currentLayerObj->editable;
}

CanvasContext *CanvasWidget::getContext()
{
    Q_D(CanvasWidget);

    d->eventThread.sync();

    return context;
}

CanvasContext *CanvasWidget::getContextMaybe()
{
    Q_D(CanvasWidget);

    if (d->eventThread.checkSync())
        return context;
    return nullptr;
}

void CanvasWidget::setLayerVisible(int layerIndex, bool visible)
{
    Q_D(CanvasWidget);

    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    CanvasLayer *layerObj = ctx->layers.layers[layerIndex];

    if (layerObj->visible == visible)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    layerObj->visible = visible;

    if (layerIndex == ctx->currentLayer)
        d->currentLayerEditable = layerObj->visible && layerObj->editable;

    TileSet layerTiles = layerObj->getTileSet();

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

void CanvasWidget::setLayerEditable(int layerIndex, bool editable)
{
    Q_D(CanvasWidget);

    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    CanvasLayer *layerObj = ctx->layers.layers[layerIndex];

    if (layerObj->editable == editable)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    layerObj->editable = editable;

    if (layerIndex == ctx->currentLayer)
        d->currentLayerEditable = layerObj->visible && layerObj->editable;

    modified = true;
    emit updateLayers();
}

bool CanvasWidget::getLayerEditable(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return false;

    return ctx->layers.layers[layerIndex]->editable;
}

void CanvasWidget::setLayerTransientOpacity(int layerIndex, float opacity)
{
    Q_D(CanvasWidget);

    if (action != CanvasAction::None)
        return;

    //FIXME: This should probably be clampled in the layer object
    opacity = qBound(0.0f, opacity, 1.0f);

    auto msg = [layerIndex, opacity](CanvasContext *ctx) {
        if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
            return;

        CanvasLayer *layerObj = ctx->layers.layers[layerIndex];

        if (layerObj->opacity == opacity)
            return;

        if (!ctx->inTransientOpacity)
        {
            ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));
            ctx->inTransientOpacity = true;
        }

        layerObj->opacity = opacity;

        TileSet layerTiles = layerObj->getTileSet();

        if(!layerTiles.empty())
        {
            ctx->dirtyTiles.insert(layerTiles.begin(), layerTiles.end());
        }
    };

    d->eventThread.enqueueCommand(msg);

    modified = true;
    emit canvasModified();
}

void CanvasWidget::setLayerOpacity(int layerIndex, float opacity)
{
    if (action != CanvasAction::None)
        return;

    setLayerTransientOpacity(layerIndex, opacity);

    CanvasContext *ctx = getContext();
    ctx->inTransientOpacity = false;
    emit updateLayers();
}

float CanvasWidget::getLayerOpacity(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return 0.0f;

    return ctx->layers.layers[layerIndex]->opacity;
}

void CanvasWidget::setLayerMode(int layerIndex, BlendMode::Mode mode)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return;

    if (ctx->layers.layers[layerIndex]->mode == mode)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

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

    for (CanvasLayer *layer: ctx->layers.layers)
        result.append({layer->name, layer->visible, layer->editable, layer->opacity, layer->mode});

    return result;
}

void CanvasWidget::flashCurrentLayer()
{
    Q_D(CanvasWidget);

    CanvasContext *ctx = getContext();
    CanvasLayer *currentLayerObj = ctx->layers.layers.at(ctx->currentLayer);

    if (!currentLayerObj->visible)
        return;

    ctx->flashStack.reset(new CanvasStack);
    ctx->flashStack->setBackground(ctx->layers.backgroundTileCL->copy());
    (*ctx->flashStack).layers.append(new CanvasLayer(*currentLayerObj));
    render->clearTiles();
    d->renderMode = RenderMode::FlashLayer;
    d->layerFlashTimeout.start(300);
    update();
}

void CanvasWidget::showColorPreview(const QColor &color)
{
    Q_D(CanvasWidget);

    d->dotPreviewColor = color;
    update();
}

void CanvasWidget::hideColorPreview()
{
    Q_D(CanvasWidget);

    d->dotPreviewColor = QColor();
    update();
}

void CanvasWidget::setBackgroundColor(const QColor &color)
{
    Q_D(CanvasWidget);

    if (action != CanvasAction::None)
        return;

    if (color.isValid())
    {
        CanvasContext *ctx = getContext();

        ctx->addUndoEvent(new CanvasUndoBackground(&ctx->layers));

        ctx->layers.backgroundTile->fill(color.redF(), color.greenF(), color.blueF(), 1.0f);
        ctx->layers.backgroundTileCL->fill(color.redF(), color.greenF(), color.blueF(), 1.0f);

        render->updateBackgroundTile(ctx);
        render->clearTiles();
        ctx->dirtyTiles = ctx->layers.getTileSet();
        d->fullRedraw = true;
        modified = true;
        emit canvasModified();
        update();
    }
}

void CanvasWidget::lineDrawMode()
{
    Q_D(CanvasWidget);

    if (d->primaryAction != CanvasAction::DrawLine)
        d->primaryAction = CanvasAction::DrawLine;
    else
        d->primaryAction = CanvasAction::MouseStroke;

    updateCursor();
}

void CanvasWidget::undo()
{
    Q_D(CanvasWidget);

    CanvasContext *ctx = getContext();

    if (ctx->undoHistory.empty())
        return;

    if (action != CanvasAction::None)
        return;

    int newActiveLayer = ctx->currentLayer;

    ctx->inTransientOpacity = false; //FIXME: This should be implicit
    CanvasUndoEvent *undoEvent = ctx->undoHistory.first();
    ctx->undoHistory.removeFirst();
    bool changedBackground = undoEvent->modifiesBackground();
    TileSet changedTiles = undoEvent->apply(&ctx->layers, &newActiveLayer);

    ctx->redoHistory.push_front(undoEvent);

    if (changedBackground)
    {
        render->updateBackgroundTile(ctx);
        render->clearTiles();
        ctx->dirtyTiles = ctx->layers.getTileSet();
        d->fullRedraw = true;
    }
    else if(!changedTiles.empty())
    {
        ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
    }

    resetCurrentLayer(ctx, newActiveLayer);

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::redo()
{
    Q_D(CanvasWidget);

    CanvasContext *ctx = getContext();

    if (ctx->redoHistory.empty())
        return;

    if (action != CanvasAction::None)
        return;

    int newActiveLayer = ctx->currentLayer;

    ctx->inTransientOpacity = false; //FIXME: This should be implicit
    CanvasUndoEvent *undoEvent = ctx->redoHistory.first();
    ctx->redoHistory.removeFirst();
    bool changedBackground = undoEvent->modifiesBackground();
    TileSet changedTiles = undoEvent->apply(&ctx->layers, &newActiveLayer);
    ctx->undoHistory.push_front(undoEvent);

    if (changedBackground)
    {
        render->updateBackgroundTile(ctx);
        render->clearTiles();
        ctx->dirtyTiles = ctx->layers.getTileSet();
        d->fullRedraw = true;
    }
    else if(!changedTiles.empty())
    {
        ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
    }

    resetCurrentLayer(ctx, newActiveLayer);

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::endLayerFlash()
{
    Q_D(CanvasWidget);

    if (d->renderMode == RenderMode::FlashLayer)
    {
        d->layerFlashTimeout.stop();

        CanvasContext *ctx = getContext();
        ctx->flashStack.reset();
        ctx->dirtyTiles = ctx->layers.getTileSet();
        render->clearTiles();
        d->renderMode = RenderMode::Normal;
        update();
    }
}

bool CanvasWidget::getModified()
{
    return modified;
}

void CanvasWidget::setModified(bool state)
{
    modified = state;
}

bool CanvasWidget::getSynchronous()
{
    Q_D(CanvasWidget);

    return d->eventThread.getSynchronous();
}

void CanvasWidget::setSynchronous(bool synced)
{
    Q_D(CanvasWidget);

    return d->eventThread.setSynchronous(synced);
}

QVariant CanvasWidget::getLastStrokeData()
{
    Q_D(CanvasWidget);

    QVariantList result;
    for (auto const &point : d->savedStrokePoints)
        result.push_back(QVariantList{point.x, point.y, point.p, point.dt});

    return result;
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
                            0, nullptr, nullptr);
        if (data[3] > 0.0f)
        {
            setToolColor(QColor::fromRgbF(data[0], data[1], data[2]));
            emit updateTool();
        }
    }
}

static void runCursorHack(QWidget *target)
{
    //FIXME: As of Qt 5.4 the cursor does not correctly reset on OSX
#ifdef Q_OS_MAC
    // This works most reliably with a 1ms delay
    QTimer::singleShot(1, target, [target]() {
        QCursor prevCursor = target->cursor();
        target->unsetCursor();
        target->setCursor(prevCursor);
    });
#endif
}

bool CanvasWidget::eventFilter(QObject *obj, QEvent *event)
{
    Q_D(CanvasWidget);

    bool deviceWasEraser = d->deviceIsEraser;

    if (event->type() == QEvent::TabletEnterProximity)
    {
        if (static_cast<QTabletEvent *>(event)->pointerType() == QTabletEvent::Eraser)
            d->deviceIsEraser = true;
        runCursorHack(this);
    }
    else if (event->type() == QEvent::TabletLeaveProximity)
    {
        d->deviceIsEraser = false;
        runCursorHack(this);
    }
    else if (event->type() == QEvent::KeyPress ||
             event->type() == QEvent::KeyRelease)
    {
        updateModifiers(static_cast<QInputEvent *>(event));
    }

    if (deviceWasEraser != d->deviceIsEraser)
    {
        if (d->deviceIsEraser)
            setActiveTool(d->eraserToolPath);
        else
            setActiveTool(d->penToolPath);
    }

    return QGLWidget::eventFilter(obj, event);
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    Q_D(CanvasWidget);

    QPointF pos = (event->localPos() + canvasOrigin) / viewScale;
    float pressure = 0.5f;
    ulong timestamp = event->timestamp();
    bool wasTablet = false;
    Qt::MouseButton button = event->button();

    if (d->lastTabletEvent.valid)
    {
        d->lastTabletEvent.valid = false;
        pos = d->lastTabletEvent.pos;
        pressure = d->lastTabletEvent.pressure;
        timestamp = d->lastTabletEvent.timestamp;
        wasTablet = true;
    }

    if (action == CanvasAction::None)
    {
        action = d->actionForMouseEvent(button, event->modifiers());

        if (action == CanvasAction::ColorPick)
        {
            pickColorAt(pos.toPoint());
            action = CanvasAction::None;
        }
        else if (action == CanvasAction::MouseStroke)
        {
            actionButton = event->button();
            if (wasTablet)
                action = CanvasAction::TabletStroke;

            d->strokeEventTimestamp = timestamp;

            startStroke(pos, pressure);
        }
        else if (action == CanvasAction::DrawLine)
        {
            actionButton = event->button();
            actionOrigin = event->pos();

            startLine();
        }
        else if (action == CanvasAction::MoveView)
        {
            actionButton = event->button();
            actionOrigin = event->pos();
        }
        else if (action == CanvasAction::MoveLayer)
        {
            actionButton = event->button();
            actionOrigin = event->pos();
        }

        updateCursor();
    }

    event->accept();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == actionButton)
    {
        if (action == CanvasAction::MouseStroke)
        {
            endStroke();
            action = CanvasAction::None;
            actionButton = Qt::NoButton;
        }
        else if (action == CanvasAction::DrawLine)
        {
            action = CanvasAction::None;
            actionButton = Qt::NoButton;
            endLine();
        }
        else if (action == CanvasAction::MoveView)
        {
            action = CanvasAction::None;
            actionButton = Qt::NoButton;
        }
        else if (action == CanvasAction::MoveLayer)
        {
            QPoint offset = event->pos() - actionOrigin;
            offset /= viewScale;
            action = CanvasAction::None;
            actionButton = Qt::NoButton;

            translateCurrentLayer(offset.x(), offset.y());
        }

        updateCursor();
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

        strokeTo(pos, 0.5f, float(newTimestamp) - d->strokeEventTimestamp);

        d->strokeEventTimestamp = newTimestamp;
    }
    else if (action == CanvasAction::DrawLine)
    {
        QPointF start = (actionOrigin + canvasOrigin) / viewScale;
        QPointF end = (event->localPos() + canvasOrigin) / viewScale;
        lineTo(start, end);
    }
    else if (action == CanvasAction::MoveView)
    {
        QPoint dPos = event->pos() - actionOrigin;
        actionOrigin = event->pos();
        canvasOrigin -= dPos;
        d->fullRedraw = true;
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
    QPointF point = (event->posF() + canvasOrigin) / viewScale;

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
    Q_D(CanvasWidget);

    mouseEventRate.addEvents(1);
    canvasOrigin += event->pixelDelta();
    d->fullRedraw = true;
    update();
}

void CanvasWidget::leaveEvent(QEvent * event)
{
    showToolCursor = false;
    unsetCursor();
    update();
}

void CanvasWidget::enterEvent(QEvent * event)
{
    showToolCursor = true;
    updateCursor();
    update();
}

void CanvasWidget::updateModifiers(QInputEvent *event)
{
    Q_D(CanvasWidget);

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

    if (d->keyModifiers != modState)
    {
        d->keyModifiers = modState;
        updateCursor();
    }
}

void CanvasWidget::updateCursor()
{
    Q_D(CanvasWidget);

    CanvasAction::Action cursorAction = action;
    if (cursorAction == CanvasAction::None)
        cursorAction = d->actionForMouseEvent(1, d->keyModifiers);

    if ((cursorAction == CanvasAction::MouseStroke ||
         cursorAction == CanvasAction::TabletStroke) &&
         d->activeTool && d->currentLayerEditable)
    {
        setCursor(Qt::BlankCursor);
    }
    else if (cursorAction == CanvasAction::ColorPick)
    {
        setCursor(colorPickCursor);
    }
    else if ((cursorAction == CanvasAction::MoveLayer) &&
             d->currentLayerEditable)
    {
        setCursor(moveLayerCursor);
    }
    else if (cursorAction == CanvasAction::MoveView)
    {
        setCursor(moveViewCursor);
    }
    else if (cursorAction == CanvasAction::DrawLine)
    {
        setCursor(Qt::CrossCursor);
    }
    else
    {
        setCursor(Qt::ArrowCursor);
    }
}

void CanvasWidget::setActiveTool(const QString &toolName)
{
    Q_D(CanvasWidget);

    if (d->activeToolPath == toolName)
        return;

    d->activeToolPath = toolName;

    auto found = d->tools.find(toolName);

    if (found != d->tools.end())
    {
        d->activeTool = found.value();
    }
    else if ((d->activeTool = ToolFactory::loadTool(toolName)))
    {
        d->tools[toolName] = d->activeTool;
    }
    else
    {
        qWarning() << "Unknown tool set \'" << toolName << "\', using debug";
        d->activeToolPath = QStringLiteral("debug");
        d->activeTool = d->tools["debug"];

        if (d->activeTool == nullptr)
        {
            d->activeTool = ToolFactory::loadTool(d->activeToolPath);
            d->tools[d->activeToolPath] = d->activeTool;
        }
    }

    if (d->activeTool)
    {
        d->activeTool->setColor(toolColor);
    }

    if (d->deviceIsEraser)
        d->eraserToolPath = d->activeToolPath;
    else
        d->penToolPath = d->activeToolPath;

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

QList<ToolSettingInfo> CanvasWidget::getAdvancedToolSettings()
{
    Q_D(CanvasWidget);

    if (d->activeTool)
        return d->activeTool->listAdvancedSettings();
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

void CanvasWidget::resetToolSettings()
{
    Q_D(CanvasWidget);

    if (!d->activeTool)
        return;

    d->activeTool->reset();
    d->activeTool->setColor(toolColor);

    emit updateTool();
    update();
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
    Q_D(CanvasWidget);

    CanvasContext *ctx = getContext();

    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    render->clearTiles();
    lastNewLayerNumber = 0;
    ctx->layers.clearLayers();
    ctx->layers.newLayerAt(0, QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    d->fullRedraw = true;
    update();
    modified = false;
    emit updateLayers();
}

void CanvasWidget::openORA(QString path)
{
    Q_D(CanvasWidget);

    QRegExp layerNameReg("Layer (\\d+)");
    CanvasContext *ctx = getContext();

    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    render->clearTiles();
    loadStackFromORA(&ctx->layers, path);
    lastNewLayerNumber = 0;
    for (CanvasLayer const *layer: ctx->layers.layers)
    {
        if (layerNameReg.exactMatch(layer->name))
            lastNewLayerNumber = qMax(lastNewLayerNumber, layerNameReg.cap(1).toInt());
    }
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    ctx->dirtyTiles = ctx->layers.getTileSet();
    render->updateBackgroundTile(context);
    d->fullRedraw = true;
    update();
    modified = false;
    emit updateLayers();
}

void CanvasWidget::openImage(QImage image)
{
    Q_D(CanvasWidget);

    CanvasContext *ctx = getContext();

    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    render->clearTiles();
    lastNewLayerNumber = 0;
    ctx->layers.clearLayers();
    ctx->layers.newLayerAt(0, QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    if (image.isNull())
    {
        qWarning() << "CanvasWidget::openImage called with a null image";
    }
    else
    {
        std::unique_ptr<CanvasLayer> imported = layerFromImage(image);
        ctx->layers.layers.at(0)->takeTiles(imported.get());
    }
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    ctx->dirtyTiles = ctx->layers.getTileSet();
    d->fullRedraw = true;
    update();
    modified = false;
    emit updateLayers();
}

void CanvasWidget::saveAsORA(QString path)
{
    CanvasContext *ctx = getContext();

    QProgressDialog dialog("Saving...", QString(), 0, 100, topLevelWidget());
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setMinimumDuration(0);
    dialog.setAutoClose(false);
    // Value must be set to 0 first or the dialog does not display on Win32
    dialog.setValue(0);
    dialog.setValue(1);

    auto callback = [&dialog](QString const &msg, float percent){
        dialog.setLabelText(msg);
        dialog.setValue(qBound<int>(1, percent, 99));
    };

    saveStackAs(&ctx->layers, path, callback);

    dialog.close();

    modified = false;
    emit canvasModified();
}

QImage CanvasWidget::asImage()
{
    CanvasContext *ctx = getContext();

    return stackToImage(&ctx->layers);
}
