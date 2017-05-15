#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include "canvasrender.h"
#include "canvascontext.h"
#include "canvaseventthread.h"
#include "canvasindex.h"
#include "basetool.h"
#include "ora.h"
#include "imagefiles.h"
#include "toolfactory.h"
#include <list>
#include <qmath.h>
#include <QApplication>
#include <QWindow>
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

namespace {
/* Return the set of tiles modified by this layer, taking
 * masking into account.
 */
TileSet dirtyTilesForLayer(CanvasStack *stack, int index)
{
    auto path = pathFromAbsoluteIndex(stack, index);
    if (path.isEmpty())
    {
        return {};
    }
    else
    {
        CanvasLayer *parent = stack->layers[path[0]];
        for (int i = 1; i < path.size(); ++i)
        {
            CanvasLayer *next = parent->children[path[i]];
            if (BlendMode::isMasking(next->mode))
                return parent->getTileSet();
            parent = next;
        }
        return parent->getTileSet();
    }
}
}

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
    bool currentLayerMoveable;

    bool deviceIsEraser;
    CanvasAction::Action primaryAction;
    Qt::KeyboardModifiers keyModifiers;
    SavedTabletEvent lastTabletEvent;
    ulong strokeEventTimestamp; // last stroke event time, in milliseconds
    std::list<CanvasStrokePoint> savedStrokePoints;

    int nextFrameDelay;
    QTimer frameTickTrigger;
    QElapsedTimer lastFrameTimer;
    RenderMode::Mode renderMode;
    QTimer layerFlashTimeout;
    QColor dotPreviewColor;
    std::shared_ptr<QAtomicInt> motionCoalesceToken;

    CanvasAction::Action actionForMouseEvent(int button, Qt::KeyboardModifiers modifiers);
    void updateEditable(CanvasContext *ctx);
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
    currentLayerEditable = false;
    currentLayerMoveable = false;
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
        else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier))
            return CanvasAction::ColorPickMerged;
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

void CanvasWidgetPrivate::updateEditable(CanvasContext *ctx)
{
    auto path = pathFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);
    CanvasLayer *layer = nullptr;
    QList<CanvasLayer *> *container = &ctx->layers.layers;
    currentLayerMoveable = true;
    for (auto const i: path)
    {
        layer = container->at(i);
        container = &layer->children;
        if (!layer->visible || !layer->editable)
        {
            currentLayerMoveable = false;
            break;
        }
    }

    if (layer && layer->type == LayerType::Layer)
        currentLayerEditable = currentLayerMoveable;
    else
        currentLayerEditable = false;
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

    QString cursorString = (qApp->devicePixelRatio() > 1.0) ?
                QStringLiteral(":/cursors/%1@2x.png") :
                QStringLiteral(":/cursors/%1.png");

    colorPickCursor = QCursor(QPixmap(cursorString.arg("eyedropper")));
    moveViewCursor = QCursor(QPixmap(cursorString.arg("move-view")));
    moveLayerCursor = QCursor(QPixmap(cursorString.arg("move-layer")));

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

    if (window() && window()->windowHandle())
        render->viewPixelRatio = window()->windowHandle()->devicePixelRatio();

    d->eventThread.ctx = context;
    d->eventThread.start();

    newDrawing();
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

    frameRate.addEvents(1);
    // Emit updateStats() outside of the paintEvent
    QMetaObject::invokeMethod(this, "updateStats", Qt::QueuedConnection);

    TileMap renderTiles;

    if (d->renderMode == RenderMode::FlashLayer)
    {
        CanvasContext *ctx = getContext();
        if (ctx->flashStack)
        {
            TileSet flashSet = ctx->flashStack->getTileSet();
            for (auto iter: flashSet)
                renderTiles[iter] = ctx->flashStack->getTileMaybe(iter.x(), iter.y());
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
    }

    render->renderTileMap(renderTiles);

    render->renderView(canvasOrigin, size(), viewScale, false);

    if (d->activeTool && d->currentLayerEditable && showToolCursor)
    {
        QPoint cursorPos = mapFromGlobal(QCursor::pos());
        float toolSize = std::max(d->activeTool->getPixelRadius() * 2.0f * viewScale, 6.0f);

        render->drawToolCursor(cursorPos, toolSize, Qt::black, Qt::white);
    }

    if (d->dotPreviewColor.isValid())
        render->drawColorDots(d->dotPreviewColor);
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

        CanvasLayer *targetLayer = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);

        if (!targetLayer)
            return;
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

            CanvasLayer *currentLayerObj = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);
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

        if (ctx->layers.empty())
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

        CanvasLayer *targetLayer = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);

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

        CanvasLayer *currentLayerObj = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);
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
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (layerFromAbsoluteIndex(&ctx->layers, layerIndex))
    {
        bool update = (ctx->currentLayer != layerIndex);
        resetCurrentLayer(ctx, layerIndex);

        if (update)
            emit updateLayers();
    }
}

void CanvasWidget::addLayerAbove(int layerIndex)
{
    CanvasLayer *layer = new CanvasLayer(QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    insertLayerAbove(layerIndex, std::unique_ptr<CanvasLayer>(layer));
}

void CanvasWidget::addLayerAbove(int layerIndex, QImage image, QString name)
{
    if (image.isNull())
        return;

    std::unique_ptr<CanvasLayer> imported = layerFromImage(image);

    if (name.isEmpty())
        name = QString().sprintf("Layer %02d", ++lastNewLayerNumber);
    imported->name = name;

    insertLayerAbove(layerIndex, std::move(imported));
}

void CanvasWidget::addGroupAbove(int layerIndex)
{
    CanvasLayer *layer = new CanvasLayer(QStringLiteral("Group"));
    layer->type = LayerType::Group;
    insertLayerAbove(layerIndex, std::unique_ptr<CanvasLayer>(layer));
}

void CanvasWidget::insertLayerAbove(int layerIndex, std::unique_ptr<CanvasLayer> newLayer)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    auto parentInfo = parentFromAbsoluteIndex(&ctx->layers, layerIndex);
    CanvasLayer *currentLayer = parentInfo.element;
    if (!currentLayer)
        return; // Nothing to do

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    CanvasLayer *insertedLayer = newLayer.release();
    parentInfo.container->insert(parentInfo.index + 1, insertedLayer);
    resetCurrentLayer(ctx, absoluteIndexOfLayer(&ctx->layers, insertedLayer));

    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, ctx->currentLayer));

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::removeLayer(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    auto parentInfo = parentFromAbsoluteIndex(&ctx->layers, layerIndex);

    if (!parentInfo.element)
        return;
    // Block removing the last layer or group
    if (parentInfo.container == &(ctx->layers.layers) && ctx->layers.size() <= 1)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    /* Before we delete the layer, dirty all tiles it intersects */
    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, layerIndex));

    parentInfo.element->swapOut();

    delete parentInfo.container->takeAt(parentInfo.index);

    // Keep the selection inside a group when removing it's bottom layer
    if (parentInfo.index == 0 && parentInfo.container->size())
        resetCurrentLayer(ctx, qMax(0, ctx->currentLayer));
    else
        resetCurrentLayer(ctx, qMax(0, ctx->currentLayer - 1));

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::moveLayerUp(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    // 0. Save the undo before we modify anything
    auto undoEvent = std::unique_ptr<CanvasUndoLayers>(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    // 1. Is "index" a valid layer?
    auto parentInfo = parentFromAbsoluteIndex(&ctx->layers, layerIndex);
    CanvasLayer *currentLayer = parentInfo.element;
    if (!currentLayer)
        return; // Nothing to do

    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, layerIndex));

    // 2. In non-absolute terms, what is the object above it?
    int aboveIndex = parentInfo.index + 1;
    if (aboveIndex >= parentInfo.container->size())
    {
        // If currentLayer is at the top of it's container, is there an outer group?
        if (!parentInfo.parentContainer)
            return; // Nothing to do
        // Move the layer out of the group
        parentInfo.parentContainer->insert(parentInfo.parentIndex + 1, parentInfo.container->takeAt(parentInfo.index));
    }
    else
    {
        CanvasLayer *aboveLayer = parentInfo.container->at(aboveIndex);
        if (aboveLayer->type == LayerType::Layer)
        {
            // If it's a layer swap pointers with it
            (*parentInfo.container)[aboveIndex] = currentLayer;
            (*parentInfo.container)[parentInfo.index] = aboveLayer;
        }
        else
        {
            // If it's a group move it to the bottom of the group
            aboveLayer->children.prepend(parentInfo.container->takeAt(parentInfo.index));
        }
    }

    // Finish up
    ctx->addUndoEvent(undoEvent.release());
    resetCurrentLayer(ctx, absoluteIndexOfLayer(&ctx->layers, currentLayer));

    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, ctx->currentLayer));

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::moveLayerDown(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    // 0. Save the undo before we modify anything
    auto undoEvent = std::unique_ptr<CanvasUndoLayers>(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    // 1. Is "index" a valid layer?
    auto parentInfo = parentFromAbsoluteIndex(&ctx->layers, layerIndex);
    CanvasLayer *currentLayer = parentInfo.element;
    if (!currentLayer)
        return; // Nothing to do

    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, layerIndex));

    // 2. In non-absolute terms, what is the object below it?
    int belowIndex = parentInfo.index - 1;
    if (belowIndex < 0)
    {
        // currentLayer is at the bottom of it's container, is there an outer group?
        if (!parentInfo.parentContainer)
            return; // Nothing to do
        // Move the layer out of the group
        parentInfo.parentContainer->insert(parentInfo.parentIndex, parentInfo.container->takeAt(parentInfo.index));
    }
    else
    {
        CanvasLayer *belowLayer = parentInfo.container->at(belowIndex);
        if (belowLayer->type == LayerType::Layer)
        {
            // If it's a layer swap pointers with it
            (*parentInfo.container)[belowIndex] = currentLayer;
            (*parentInfo.container)[parentInfo.index] = belowLayer;
        }
        else
        {
            // If it's a group move it to the top of the group
            belowLayer->children.append(parentInfo.container->takeAt(parentInfo.index));
        }
    }

    // Finish up
    ctx->addUndoEvent(undoEvent.release());
    resetCurrentLayer(ctx, absoluteIndexOfLayer(&ctx->layers, currentLayer));

    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, ctx->currentLayer));

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::renameLayer(int layerIndex, QString name)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();
    CanvasLayer *currentLayer = layerFromAbsoluteIndex(&ctx->layers, layerIndex);

    if (!currentLayer)
        return;

    if (currentLayer->name == name)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    currentLayer->name = name;
    modified = true;
    emit updateLayers();
}

void CanvasWidget::mergeLayerDown(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();
    auto parentInfo = parentFromAbsoluteIndex(&ctx->layers, layerIndex);

    if (!parentInfo.element || parentInfo.index == 0)
        return;

    CanvasLayer *above = parentInfo.container->at(parentInfo.index);
    CanvasLayer *below = parentInfo.container->at(parentInfo.index - 1);
    std::unique_ptr<CanvasLayer> mergedPtr;

    if (above->type == LayerType::Layer && below->type == LayerType::Layer)
    {
        mergedPtr = above->mergeDown(below);
    }
    else if (above->type == LayerType::Group && below->type == LayerType::Layer)
    {
        mergedPtr = above->flattened();
        mergedPtr = mergedPtr->mergeDown(below);
    }
    else
    {
        return;
    }

    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, ctx->currentLayer));

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    CanvasLayer *merged = mergedPtr.release();
    delete above;
    parentInfo.container->replace(parentInfo.index, merged);
    delete below;
    parentInfo.container->removeAt(parentInfo.index - 1);

    resetCurrentLayer(ctx, absoluteIndexOfLayer(&ctx->layers, merged));

    tileSetInsert(ctx->dirtyTiles, dirtyTilesForLayer(&ctx->layers, ctx->currentLayer));

    update();
    modified = true;
    emit updateLayers();
}

void CanvasWidget::duplicateLayer(int layerIndex)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    if (CanvasLayer *oldLayer = layerFromAbsoluteIndex(&ctx->layers, layerIndex))
    {
        std::unique_ptr<CanvasLayer> newLayer = oldLayer->deepCopy();
        newLayer->name = oldLayer->name + " Copy";
        insertLayerAbove(layerIndex, std::move(newLayer));
    }
}

void CanvasWidget::updateLayerTranslate(int x,  int y)
{
    Q_D(CanvasWidget);

    if (!d->motionCoalesceToken)
        d->motionCoalesceToken.reset(new QAtomicInt(1));
    else
        d->motionCoalesceToken->ref();

    auto coalesceToken = d->motionCoalesceToken;

    auto msg = [x, y, coalesceToken](CanvasContext *ctx) {
        if (coalesceToken && coalesceToken->deref() == true)
            return;

        CanvasLayer *currentLayer = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);
        if (!ctx->currentLayerCopy)
        {
            // If the current layer is a group generate a cached rendering of it
            ctx->currentLayerCopy = currentLayer->flattened();
        }
        std::unique_ptr<CanvasLayer> newLayer = ctx->currentLayerCopy->translated(x, y);

        tileSetInsert(ctx->dirtyTiles, currentLayer->getTileSet());
        tileSetInsert(ctx->dirtyTiles, newLayer->getTileSet());

        currentLayer->takeTiles(newLayer.get());
    };

    d->eventThread.enqueueCommand(msg);
}

void CanvasWidget::translateCurrentLayer(int x,  int y)
{
    Q_D(CanvasWidget);

    // Ref'ing the token will discard any pending updateLayerTranslate() events
    if (d->motionCoalesceToken)
        d->motionCoalesceToken->ref();
    d->motionCoalesceToken.reset();

    CanvasContext *ctx = getContext();

    CanvasLayer *currentLayer = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);
    if (currentLayer->type == LayerType::Layer)
    {
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
    }
    else if (currentLayer->type == LayerType::Group)
    {
        /* Add the layers currently visible tiles to the dirty list */
        TileSet dirtyTiles = currentLayer->getTileSet();
        ctx->dirtyTiles.insert(dirtyTiles.begin(), dirtyTiles.end());
        /* Discard the temporary render */
        currentLayer->tiles->clear();
        ctx->currentLayerCopy.reset(nullptr);

        std::unique_ptr<CanvasLayer> newLayer = currentLayer->translated(x, y);
        newLayer->prune();

        /* Add the newly translated tiles*/
        dirtyTiles = newLayer->getTileSet();
        ctx->dirtyTiles.insert(dirtyTiles.begin(), dirtyTiles.end());

        ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

        /* Take the translated children */
        std::swap(currentLayer->children, newLayer->children);
    }

    update();
    modified = true;
}

void CanvasWidget::resetCurrentLayer(CanvasContext *ctx, int index)
{
    Q_D(CanvasWidget);

    CanvasLayer *currentLayerObj = layerFromAbsoluteIndex(&ctx->layers, index);

    if (!currentLayerObj)
    {
        qWarning() << "Invalid layer index" << index;
        index = 0;
        currentLayerObj = layerFromAbsoluteIndex(&ctx->layers, index);
    }

    ctx->currentLayer = index;
    d->updateEditable(ctx);

    if(currentLayerObj->type == LayerType::Layer)
        ctx->currentLayerCopy = currentLayerObj->deepCopy();
    else
        ctx->currentLayerCopy.reset(nullptr);
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

    CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex);

    if (!layerObj)
        return;

    if (layerObj->visible == visible)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    layerObj->visible = visible;

    d->updateEditable(ctx);

    TileSet layerTiles = dirtyTilesForLayer(&ctx->layers, layerIndex);

    if (!layerTiles.empty())
    {
        tileSetInsert(ctx->dirtyTiles, layerTiles);
        emit update();
    }

    modified = true;
    emit updateLayers();
}

bool CanvasWidget::getLayerVisible(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex))
        return layerObj->visible;
    return false;
}

void CanvasWidget::setLayerEditable(int layerIndex, bool editable)
{
    Q_D(CanvasWidget);

    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();

    CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex);

    if (!layerObj)
        return;

    if (layerObj->editable == editable)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    layerObj->editable = editable;

    d->updateEditable(ctx);

    modified = true;
    emit updateLayers();
}

bool CanvasWidget::getLayerEditable(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex))
        return layerObj->editable;
    return false;
}

void CanvasWidget::setLayerTransientOpacity(int layerIndex, float opacity)
{
    Q_D(CanvasWidget);

    if (action != CanvasAction::None)
        return;

    //FIXME: This should probably be clampled in the layer object
    opacity = qBound(0.0f, opacity, 1.0f);

    auto msg = [layerIndex, opacity](CanvasContext *ctx) {
        CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex);

        if (!layerObj)
            return;

        if (layerObj->opacity == opacity)
            return;

        if (!ctx->inTransientOpacity)
        {
            ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));
            ctx->inTransientOpacity = true;
        }

        layerObj->opacity = opacity;

        tileSetInsert(ctx->dirtyTiles, layerObj->getTileSet());
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

    if (CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex))
        return layerObj->opacity;
    return 0.0f;
}

void CanvasWidget::setLayerMode(int layerIndex, BlendMode::Mode mode)
{
    if (action != CanvasAction::None)
        return;

    CanvasContext *ctx = getContext();
    CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex);
    if (!layerObj)
        return;

    if (layerObj->mode == mode)
        return;

    ctx->addUndoEvent(new CanvasUndoLayers(&ctx->layers, ctx->currentLayer));

    TileSet layerTiles = dirtyTilesForLayer(&ctx->layers, layerIndex);

    layerObj->mode = mode;

    if (BlendMode::isMasking(layerObj->mode))
        tileSetInsert(layerTiles, dirtyTilesForLayer(&ctx->layers, layerIndex));

    if (!layerTiles.empty())
    {
        tileSetInsert(ctx->dirtyTiles, layerTiles);
        emit update();
    }

    modified = true;
    emit updateLayers();
}

BlendMode::Mode CanvasWidget::getLayerMode(int layerIndex)
{
    CanvasContext *ctx = getContext();

    if (CanvasLayer *layerObj = layerFromAbsoluteIndex(&ctx->layers, layerIndex))
        return layerObj->mode;
    return BlendMode::Over;
}

namespace {
    void layerListAppend(QList<CanvasWidget::LayerInfo> &result, QList<CanvasLayer *> const &layers, int &index) {
        for (CanvasLayer *layer: layers)
        {
            CanvasWidget::LayerInfo info = {layer->name, layer->visible, layer->editable, layer->opacity, layer->mode, layer->type, {}, index++};
            if (!layer->children.empty())
                layerListAppend(info.children, layer->children, index);
            result.append(info);
        }
    }

    void layerListAppend(QList<CanvasWidget::LayerInfo> &result, QList<CanvasLayer *> const &layers) {
        int index = 0;
        layerListAppend(result, layers, index);
    }
}

QList<CanvasWidget::LayerInfo> CanvasWidget::getLayerList()
{
    CanvasContext *ctx = getContext();
    QList<LayerInfo> result;

    if (!ctx)
        return result;

    layerListAppend(result, ctx->layers.layers);

    return result;
}

void CanvasWidget::flashCurrentLayer()
{
    Q_D(CanvasWidget);

    CanvasContext *ctx = getContext();
    CanvasLayer *currentLayerObj = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer);

    if (!currentLayerObj || !currentLayerObj->visible)
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
    if (action != CanvasAction::None)
        return;

    if (color.isValid())
    {
        CanvasContext *ctx = getContext();

        ctx->addUndoEvent(new CanvasUndoBackground(&ctx->layers));

        ctx->layers.backgroundTile->fill(color.redF(), color.greenF(), color.blueF(), 1.0f);
        ctx->layers.backgroundTileCL->fill(color.redF(), color.greenF(), color.blueF(), 1.0f);

        render->updateBackgroundTile(ctx);
        ctx->dirtyTiles = ctx->layers.getTileSet();
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
        ctx->dirtyTiles = ctx->layers.getTileSet();
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
        ctx->dirtyTiles = ctx->layers.getTileSet();
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

std::vector<CanvasStrokePoint> CanvasWidget::getLastStrokeData()
{
    Q_D(CanvasWidget);

    std::vector<CanvasStrokePoint> result(d->savedStrokePoints.begin(), d->savedStrokePoints.end());
    return result;
}

void CanvasWidget::pickColorAt(QPoint pos, bool merged)
{
    std::unique_ptr<CanvasTile> mergedTile;
    CanvasContext *ctx = getContext();

    int ix = tile_indice(pos.x(), TILE_PIXEL_WIDTH);
    int iy = tile_indice(pos.y(), TILE_PIXEL_HEIGHT);
    CanvasTile *tile = nullptr;
    if (merged)
    {
        mergedTile = ctx->layers.getTileMaybe(ix, iy);
        if (mergedTile)
            tile = mergedTile.get();
        else
            tile = ctx->layers.backgroundTileCL.get();
    }
    else
    {
        tile = layerFromAbsoluteIndex(&ctx->layers, ctx->currentLayer)->getTileMaybe(ix, iy);
    }

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

bool CanvasWidget::eventFilter(QObject *obj, QEvent *event)
{
    Q_D(CanvasWidget);

    bool deviceWasEraser = d->deviceIsEraser;

    if (event->type() == QEvent::TabletEnterProximity)
    {
        if (static_cast<QTabletEvent *>(event)->pointerType() == QTabletEvent::Eraser)
            d->deviceIsEraser = true;
    }
    else if (event->type() == QEvent::TabletLeaveProximity)
    {
        d->deviceIsEraser = false;
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
        if (action == CanvasAction::ColorPickMerged)
        {
            pickColorAt(pos.toPoint(), true);
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
            if (d->currentLayerMoveable)
            {
                actionButton = event->button();
                actionOrigin = event->pos();
            }
            else
            {
                action = CanvasAction::None;
            }

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

        strokeTo(pos, 0.5f, float(newTimestamp - d->strokeEventTimestamp));

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
        strokeTo(point, event->pressure(), float(newTimestamp - d->strokeEventTimestamp));
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

/* As of Qt 5.4 the cursor is not correctly set over OpenGL windows on OSX,
 * this hacks around that by setting the cursor for the toplevel window instead.
 */
#ifdef Q_OS_MAC
#define CURSOR_WIDGET topLevelWidget()
#else
#define CURSOR_WIDGET this
#endif

void CanvasWidget::leaveEvent(QEvent * event)
{
    showToolCursor = false;
    CURSOR_WIDGET->unsetCursor();
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

    if (!underMouse())
        return;

    CanvasAction::Action cursorAction = action;
    if (cursorAction == CanvasAction::None)
        cursorAction = d->actionForMouseEvent(1, d->keyModifiers);

    if ((cursorAction == CanvasAction::MouseStroke ||
         cursorAction == CanvasAction::TabletStroke) &&
         d->activeTool && d->currentLayerEditable)
    {
       CURSOR_WIDGET->setCursor(Qt::BlankCursor);
    }
    else if (cursorAction == CanvasAction::ColorPick ||
             cursorAction == CanvasAction::ColorPickMerged )
    {
        CURSOR_WIDGET->setCursor(colorPickCursor);
    }
    else if ((cursorAction == CanvasAction::MoveLayer) &&
             d->currentLayerMoveable)
    {
        CURSOR_WIDGET->setCursor(moveLayerCursor);
    }
    else if (cursorAction == CanvasAction::MoveView)
    {
        CURSOR_WIDGET->setCursor(moveViewCursor);
    }
    else if (cursorAction == CanvasAction::DrawLine)
    {
        CURSOR_WIDGET->setCursor(Qt::CrossCursor);
    }
    else
    {
        CURSOR_WIDGET->setCursor(Qt::ArrowCursor);
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

bool CanvasWidget::getToolSaveable()
{
    Q_D(CanvasWidget);

    if (!d->activeTool)
        return false;

    QString extension = d->activeTool->saveExtension();
    if (!extension.isEmpty() && !ToolFactory::savePathForExtension(extension).isEmpty())
        return true;

    return false;
}

CanvasWidget::ToolSaveInfo CanvasWidget::serializeTool()
{
    Q_D(CanvasWidget);

    if (!d->activeTool)
        return {};

    QString extension = d->activeTool->saveExtension();
    if (extension.isEmpty())
        return {};

    QString savePath = ToolFactory::savePathForExtension(extension);
    if (savePath.isEmpty())
        return {};

    QByteArray output = d->activeTool->serialize();
    if (!output.isEmpty())
    {
        ToolSaveInfo result;
        result.fileExtension = extension;
        result.saveDirectory = savePath;
        result.serialzedTool = output;

        return result;
    }

    return {};
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

QImage CanvasWidget::previewTool(std::vector<CanvasStrokePoint> const &stroke)
{
    Q_D(CanvasWidget);

    if (!d->activeTool)
        return QImage();

    if (stroke.empty())
        return QImage();

    /* Nothing here uses the context but it must be in sync for thread safety */
    getContext();

    std::unique_ptr<CanvasLayer> targetLayer(new CanvasLayer);
    std::unique_ptr<CanvasLayer> undoLayer(new CanvasLayer);

    { // The stroke needs to go out of scope before anything else does
        StrokeContextArgs args = {targetLayer.get(), undoLayer.get()};
        std::unique_ptr<StrokeContext> strokeContext(d->activeTool->newStroke(args));

        auto iter = stroke.begin();
        strokeContext->startStroke({iter->x, iter->y}, iter->p);

        for (++iter; iter != stroke.end(); ++iter)
        {
            strokeContext->strokeTo({iter->x, iter->y}, iter->p, iter->dt);
        }
    }

    QPoint offset = tileSetBounds(targetLayer->getTileSet()).topLeft();
    offset = {offset.x() * -TILE_PIXEL_WIDTH, offset.y() * -TILE_PIXEL_HEIGHT};
    QImage result = layerToImage(targetLayer.get());
    result.setOffset(offset);

    return result;
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
    ctx->layers.layers.append(new CanvasLayer(QString().sprintf("Layer %02d", ++lastNewLayerNumber)));
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    update();
    modified = false;
    emit updateLayers();
}

namespace {
    int findHighestLayerNumber(QList<CanvasLayer *> const &layers, QRegExp &regex)
    {
        int high = 0;
        for (CanvasLayer const *layer: layers)
        {
            if (regex.exactMatch(layer->name))
                high = qMax(high, regex.cap(1).toInt());
            high = qMax(high, findHighestLayerNumber(layer->children, regex));
        }
        return high;
    }
}

void CanvasWidget::openORA(QString path)
{
    QRegExp layerNameReg("Layer (\\d+)");
    CanvasContext *ctx = getContext();

    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    render->clearTiles();
    loadStackFromORA(&ctx->layers, path);
    lastNewLayerNumber = findHighestLayerNumber(ctx->layers.layers, layerNameReg);
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    ctx->dirtyTiles = ctx->layers.getTileSet();
    render->updateBackgroundTile(context);
    update();
    modified = false;
    emit updateLayers();
}

void CanvasWidget::openImage(QImage image)
{
    CanvasContext *ctx = getContext();

    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    render->clearTiles();
    lastNewLayerNumber = 0;
    ctx->layers.clearLayers();
    CanvasLayer *imageLayer = new CanvasLayer(QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    ctx->layers.layers.append(imageLayer);
    if (image.isNull())
    {
        qWarning() << "CanvasWidget::openImage called with a null image";
    }
    else
    {
        std::unique_ptr<CanvasLayer> imported = layerFromImage(image);
        imageLayer->takeTiles(imported.get());
    }
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
