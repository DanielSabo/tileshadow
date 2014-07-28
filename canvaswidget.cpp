#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include "canvascontext.h"
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
#include <QDebug>
#include <QOpenGLFunctions_3_2_Core>
#include "glhelper.h"

using namespace std;

static const QGLFormat &getFormatSingleton()
{
    static QGLFormat *single = NULL;

    if (!single)
    {
        single = new QGLFormat();
        single->setVersion(3, 2);
        single->setProfile(QGLFormat::CoreProfile);
    }

    return *single;
}

CanvasWidget::CanvasWidget(QWidget *parent) :
    QGLWidget(getFormatSingleton(), parent),
    mouseEventRate(10),
    frameRate(10),
    ctx(NULL),
    action(CanvasAction::None),
    toolSizeFactor(1.0f),
    toolColor(QColor::fromRgbF(0.0, 0.0, 0.0)),
    viewScale(1.0f),
    showToolCursor(true),
    lastNewLayerNumber(0),
    modified(false),
    canvasOrigin(0, 0)
{
    setMouseTracking(true);

    //FIXME: Creating the tool may do OpenCL things that require a valid context
    setActiveTool("debug");

    colorPickCursor = QCursor(QPixmap(":/cursors/eyedropper.png"));
    moveViewCursor = QCursor(QPixmap(":/cursors/move-view.png"));
    moveLayerCursor = QCursor(QPixmap(":/cursors/move-layer.png"));

    connect(this, &CanvasWidget::updateLayers, this, &CanvasWidget::canvasModified);
}

CanvasWidget::~CanvasWidget()
{
    delete ctx;
}

void CanvasWidget::initializeGL()
{
    ctx = new CanvasContext();
    QOpenGLFunctions_3_2_Core *glFuncs = ctx->glFuncs;

    /* Build canvas shaders */
    ctx->vertexShader = compileGLShaderFile(glFuncs, ":/CanvasShader.vert", GL_VERTEX_SHADER);
    ctx->fragmentShader = compileGLShaderFile(glFuncs, ":/CanvasShader.frag", GL_FRAGMENT_SHADER);
    ctx->program = buildGLProgram(glFuncs, ctx->vertexShader, ctx->fragmentShader);

    if (ctx->program)
    {
        ctx->locationTileOrigin = glFuncs->glGetUniformLocation(ctx->program, "tileOrigin");
        ctx->locationTileSize   = glFuncs->glGetUniformLocation(ctx->program, "tileSize");
        ctx->locationTileImage  = glFuncs->glGetUniformLocation(ctx->program, "tileImage");
        ctx->locationTilePixels = glFuncs->glGetUniformLocation(ctx->program, "tilePixels");
    }

    glFuncs->glGenBuffers(1, &ctx->vertexBuffer);
    glFuncs->glGenVertexArrays(1, &ctx->vertexArray);
    glFuncs->glBindVertexArray(ctx->vertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, ctx->vertexBuffer);

    float positionData[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };

    glFuncs->glBufferData(GL_ARRAY_BUFFER, sizeof(positionData), positionData, GL_STATIC_DRAW);

    glFuncs->glEnableVertexAttribArray(0);
    glFuncs->glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (GLubyte *)NULL);

    /* Build cursor */
    {
        GLuint cursorVert = compileGLShaderFile(glFuncs, ":/CursorCircle.vert", GL_VERTEX_SHADER);
        GLuint cursorFrag = compileGLShaderFile(glFuncs, ":/CursorCircle.frag", GL_FRAGMENT_SHADER);
        ctx->cursorProgram = buildGLProgram(glFuncs, cursorVert, cursorFrag);

        if (ctx->cursorProgram)
        {
            ctx->cursorProgramDimensions = glFuncs->glGetUniformLocation(ctx->cursorProgram, "dimensions");
            ctx->cursorProgramPixelRadius = glFuncs->glGetUniformLocation(ctx->cursorProgram, "pixelRadius");
        }

        float cursorVertData[] = {
            -1.0f, -1.0f,
            1.0f,  -1.0f,
            1.0f,  1.0f,
            -1.0f, 1.0f,
        };

        glFuncs->glGenBuffers(1, &ctx->cursorVertexBuffer);
        glFuncs->glGenVertexArrays(1, &ctx->cursorVertexArray);
        glFuncs->glBindVertexArray(ctx->cursorVertexArray);
        glFuncs->glBindBuffer(GL_ARRAY_BUFFER, ctx->cursorVertexBuffer);

        glFuncs->glBufferData(GL_ARRAY_BUFFER, sizeof(cursorVertData), cursorVertData, GL_STATIC_DRAW);

        glFuncs->glEnableVertexAttribArray(0);
        glFuncs->glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (GLubyte *)NULL);

        if (cursorVert)
            glFuncs->glDeleteShader(cursorVert);
        if (cursorFrag)
            glFuncs->glDeleteShader(cursorFrag);
    }

    /* Set up widget */
    SharedOpenCL::getSharedOpenCL();

    ctx->updateBackgroundTile();

    newDrawing();
}

void CanvasWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, qMax(w, 1), qMax(h, 1));
}

void CanvasWidget::paintGL()
{
    QOpenGLFunctions_3_2_Core *glFuncs = ctx->glFuncs;

    frameRate.addEvents(1);
    emit updateStats();

    ctx->closeTiles();

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

    glFuncs->glUseProgram(ctx->program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glUniform1i(ctx->locationTileImage, 0);
    glFuncs->glUniform2f(ctx->locationTileSize, tileWidth, tileHeight);
    glFuncs->glUniform2f(ctx->locationTilePixels, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
    glFuncs->glBindVertexArray(ctx->vertexArray);

    for (int ix = 0; ix < tileCountX; ++ix)
        for (int iy = 0; iy < tileCountY; ++iy)
        {
            float offsetX = (ix * tileWidth) - 1.0f - tileShiftX;
            float offsetY = 1.0f - ((iy + 1) * tileHeight) + tileShiftY;

            GLuint tileBuffer = ctx->getGLBuf(ix + originTileX, iy + originTileY);
            glFuncs->glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, tileBuffer);
            glFuncs->glUniform2f(ctx->locationTileOrigin, offsetX, offsetY);
            glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }

    if (!activeTool.isNull() && showToolCursor)
    {
        float toolSize = activeTool->getPixelRadius() * 2.0f * viewScale;
        if (toolSize < 2.0f)
            toolSize = 2.0f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        QPoint cursorPos = mapFromGlobal(QCursor::pos());
        glFuncs->glUseProgram(ctx->cursorProgram);
        glFuncs->glUniform4f(ctx->cursorProgramDimensions,
                             (float(cursorPos.x()) / widgetWidth * 2.0f) - 1.0f,
                             1.0f - (float(cursorPos.y()) / widgetHeight * 2.0f),
                             toolSize / widgetWidth, toolSize / widgetHeight);
        glFuncs->glUniform1f(ctx->cursorProgramPixelRadius, toolSize / 2.0f);
        glFuncs->glBindVertexArray(ctx->cursorVertexArray);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisable(GL_BLEND);
    }
}

void CanvasWidget::startStroke(QPointF pos, float pressure)
{
    ctx->stroke.reset(NULL);

    if (ctx->layers.layers.empty())
        return;

    if (activeTool.isNull())
        return;

    CanvasLayer *targetLayer = ctx->layers.layers.at(ctx->currentLayer);

    if (!targetLayer->visible)
        return;

    ctx->stroke.reset(activeTool->newStroke(ctx, targetLayer));

    TileSet changedTiles = ctx->stroke->startStroke(pos, pressure);

    if(!changedTiles.empty())
    {
        ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
        ctx->strokeModifiedTiles.insert(changedTiles.begin(), changedTiles.end());
        update();
    }
    modified = true;
    emit canvasModified();
}

void CanvasWidget::strokeTo(QPointF pos, float pressure, float dt)
{
    mouseEventRate.addEvents(1);
    TileSet changedTiles;
    if (!ctx->stroke.isNull())
    {
        changedTiles = ctx->stroke->strokeTo(pos, pressure, dt);
    }

    if(!changedTiles.empty())
    {
        ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
        ctx->strokeModifiedTiles.insert(changedTiles.begin(), changedTiles.end());
        update();
    }
}

void CanvasWidget::endStroke()
{
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
    if (!ctx)
        return 0;

    return ctx->currentLayer;
}

void CanvasWidget::setActiveLayer(int layerIndex)
{
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

void CanvasWidget::setLayerVisible(int layerIndex, bool visible)
{
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
    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return false;

    return ctx->layers.layers[layerIndex]->visible;
}

void CanvasWidget::setLayerMode(int layerIndex, BlendMode::Mode mode)
{
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
    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return BlendMode::Over;

    return ctx->layers.layers[layerIndex]->mode;
}

QList<CanvasWidget::LayerInfo> CanvasWidget::getLayerList()
{
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
    QPointF pos = (event->localPos() + canvasOrigin) / viewScale;
    Qt::MouseButton button = event->button();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    modifiers &= Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier;

#ifdef Q_OS_MAC
    // As of Qt 5.3 there is an overlap between ctrl for right click and the meta key on OSX
    if (button == 2 && modifiers.testFlag(Qt::MetaModifier))
        modifiers ^= Qt::MetaModifier;
#endif

    if (button == 1)
    {
        if (action == CanvasAction::ColorPick)
        {
            pickColorAt(pos.toPoint());
        }
        else if (action == CanvasAction::None)
        {
            ctx->strokeEventTimestamp = event->timestamp();

            startStroke(pos, 1.0f);
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
    updateModifiers(event);

    if (action == CanvasAction::MouseStroke)
    {
        QPointF pos = (event->localPos() + canvasOrigin) / viewScale;
        ulong newTimestamp = event->timestamp();

        strokeTo(pos, 1.0f, float(newTimestamp) - ctx->strokeEventTimestamp);

        ctx->strokeEventTimestamp = newTimestamp;
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
    QEvent::Type eventType = event->type();

    float xshift = event->hiResGlobalX() - event->globalX() + canvasOrigin.x();
    float yshift = event->hiResGlobalY() - event->globalY() + canvasOrigin.y();
    QPointF point = QPointF(event->x() + xshift, event->y() + yshift) / viewScale;

    updateModifiers(event);

    if (eventType == QEvent::TabletPress &&
        action == CanvasAction::None &&
        event->modifiers() == 0)
    {
        ctx->strokeEventTimestamp = event->timestamp();
        startStroke(point, event->pressure());
        action = CanvasAction::TabletStroke;
        event->accept();
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
        strokeTo(point, event->pressure(), float(newTimestamp) - ctx->strokeEventTimestamp);
        ctx->strokeEventTimestamp = newTimestamp;
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
    activeToolName = toolName;

    if (toolName.endsWith(".myb"))
    {
        activeTool.reset(new MyPaintTool(QString(":/mypaint-tools/") + toolName));
    }
    else if (toolName == QString("debug"))
    {
        activeTool.reset(new TileDebugTool());
    }
    else
    {
        qWarning() << "Unknown tool set \'" << toolName << "\', using debug";
        activeTool.reset(new TileDebugTool());
    }

    if (!activeTool.isNull())
    {
        activeTool->setSizeMod(toolSizeFactor);
        activeTool->setColor(toolColor);
    }

    emit updateTool();
}

QString CanvasWidget::getActiveTool()
{
    return activeToolName;
}

void CanvasWidget::setToolSizeFactor(float multipler)
{
    multipler = max(min(multipler, 10.0f), 0.25f);
    toolSizeFactor = multipler;
    if (!activeTool.isNull())
        activeTool->setSizeMod(toolSizeFactor);

    emit updateTool();
    update();
}

float CanvasWidget::getToolSizeFactor()
{
    return toolSizeFactor;
}

void CanvasWidget::setToolColor(const QColor &color)
{
    toolColor = color;
    if (!activeTool.isNull())
        activeTool->setColor(toolColor);

    emit updateTool();
}

QColor CanvasWidget::getToolColor()
{
    return toolColor;
}

void CanvasWidget::newDrawing()
{
    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    ctx->clearTiles();
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
    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    ctx->clearTiles();
    lastNewLayerNumber = 0;
    loadStackFromORA(&ctx->layers, path);
    setActiveLayer(0); // Sync up the undo layer
    canvasOrigin = QPoint(0, 0);
    update();
    modified = false;
    emit updateLayers();
}

void CanvasWidget::saveAsORA(QString path)
{
    saveStackAs(&ctx->layers, path);
    modified = false;
    emit canvasModified();
}

QImage CanvasWidget::asImage()
{
    return stackToImage(&ctx->layers);
}
