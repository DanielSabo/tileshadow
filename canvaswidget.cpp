#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include "canvascontext.h"
#include "basetool.h"
#include "tiledebugtool.h"
#include "mypainttool.h"
#include "ora.h"
#include <iostream>
#include <map>
#include <list>
#include <QMouseEvent>
#include <QFile>
#include <QDebug>
#include <QOpenGLFunctions_3_2_Core>

using namespace std;

void _check_gl_error(const char *file, int line);
#define check_gl_error() _check_gl_error(__FILE__,__LINE__)

void _check_gl_error(const char *file, int line) {
        GLenum err (glGetError());

        while(err!=GL_NO_ERROR) {
                string error;

                switch(err) {
                        case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
                        case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
                        case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
                        case GL_OUT_OF_MEMORY:          error="OUT_OF_MEMORY";          break;
                        case GL_INVALID_FRAMEBUFFER_OPERATION:  error="INVALID_FRAMEBUFFER_OPERATION";  break;
                }

                cerr << "GL_" << error.c_str() <<" - "<<file<<":"<<line<<endl;
                err=glGetError();
        }
}

static QOpenGLFunctions_3_2_Core *glFuncs = NULL;

static inline GLint getShaderInt(GLuint program, GLenum pname)
{
    GLint value;
    glFuncs->glGetShaderiv(program, pname, &value);
    return value;
}

static inline GLint getProgramInt(GLuint program, GLenum pname)
{
    GLint value;
    glFuncs->glGetProgramiv(program, pname, &value);
    return value;
}

static GLuint compileFile (QOpenGLFunctions_3_2_Core &gl,
                           const QString &path,
                           GLenum shaderType)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Shader compile failed, couldn't open " << path;
        return 0;
    }

    QByteArray source = file.readAll();
    if (source.isNull())
    {
        qWarning() << "Shader compile failed, couldn't read " << path;
        return 0;
    }

    GLuint shader = gl.glCreateShader(shaderType);

    const char *source_str = source.data();

    gl.glShaderSource(shader, 1, &source_str, NULL);
    gl.glCompileShader(shader);

    if (!getShaderInt(shader, GL_COMPILE_STATUS)) {
        GLint logSize = getShaderInt(shader, GL_INFO_LOG_LENGTH);
        char* logMsg = new char[logSize];
        gl.glGetShaderInfoLog(shader, logSize, NULL, logMsg);

        qWarning() << "Shader compile failed, couldn't build " << path;
        qWarning() << logMsg;

        delete[] logMsg;
        gl.glDeleteShader(shader);
        return 0;
    }

    cout << "Compiled " << path.toUtf8().constData() << endl;

    return shader;
}

static GLuint buildProgram (QOpenGLFunctions_3_2_Core &gl,
                            GLuint vertShader,
                            GLuint fragShader)
{
    if (!vertShader || !fragShader)
        return 0;

    GLuint program = glFuncs->glCreateProgram();
    glFuncs->glAttachShader(program, vertShader);
    glFuncs->glAttachShader(program, fragShader);
    glFuncs->glBindFragDataLocation(program, 0, "fragColor");
    glFuncs->glLinkProgram(program);

    if (!getProgramInt(program, GL_LINK_STATUS)) {
        GLint logSize = getProgramInt(program, GL_INFO_LOG_LENGTH);
        char* logMsg = new char[logSize];
        glFuncs->glGetProgramInfoLog(program, logSize, NULL, logMsg);

        qWarning() << "Failed to link shader";
        qWarning() << logMsg;

        delete[] logMsg;
        glFuncs->glDeleteProgram(program);
        program = 0;
    }

    return program;
}

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
    toolSizeFactor(1.0f),
    toolColor(QColor::fromRgbF(0.0, 0.0, 0.0)),
    viewScale(1.0f),
    showToolCursor(true),
    lastNewLayerNumber(0)
{
    setMouseTracking(true);

    //FIXME: Creating the tool may do OpenCL things that require a valid context
    setActiveTool("debug");
}

CanvasWidget::~CanvasWidget()
{
    delete ctx;
}

void CanvasWidget::initializeGL()
{
    if (!glFuncs)
        glFuncs = new QOpenGLFunctions_3_2_Core();
    if (!glFuncs->initializeOpenGLFunctions())
    {
        qWarning() << "Could not initialize OpenGL Core Profile 3.2";
        exit(1);
    }

    ctx = new CanvasContext(glFuncs);

    /* Build canvas shaders */
    ctx->vertexShader = compileFile(*glFuncs, ":/CanvasShader.vert", GL_VERTEX_SHADER);
    ctx->fragmentShader = compileFile(*glFuncs, ":/CanvasShader.frag", GL_FRAGMENT_SHADER);
    ctx->program = buildProgram (*glFuncs, ctx->vertexShader, ctx->fragmentShader);

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
        GLuint cursorVert = compileFile(*glFuncs, ":/CursorCircle.vert", GL_VERTEX_SHADER);
        GLuint cursorFrag = compileFile(*glFuncs, ":/CursorCircle.frag", GL_FRAGMENT_SHADER);

        ctx->cursorProgram = buildProgram(*glFuncs, cursorVert, cursorFrag);

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

    ctx->layers.newLayerAt(0, QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    ctx->currentLayerCopy.reset(ctx->layers.layers[ctx->currentLayer]->deepCopy());

    emit updateLayers();
}

void CanvasWidget::resizeGL(int w, int h)
{
    glFuncs->glViewport( 0, 0, qMax(w, 1), qMax(h, 1));
}

void CanvasWidget::paintGL()
{
    frameRate.addEvents(1);
    emit updateStats();

    ctx->closeTiles();

    int ix, iy;

    int widgetWidth  = width();
    int widgetHeight = height();

    float tileWidth  = ((2.0f * TILE_PIXEL_WIDTH) / (widgetWidth)) * viewScale;
    float tileHeight = ((2.0f * TILE_PIXEL_HEIGHT) / (widgetHeight)) * viewScale;

//    glFuncs->glClearColor(0.2, 0.2, 0.4, 1.0);
//    glFuncs->glClear(GL_COLOR_BUFFER_BIT);

    glFuncs->glUseProgram(ctx->program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glUniform1i(ctx->locationTileImage, 0);
    glFuncs->glUniform2f(ctx->locationTileSize, tileWidth, tileHeight);
    glFuncs->glUniform2f(ctx->locationTilePixels, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
    glFuncs->glBindVertexArray(ctx->vertexArray);

    for (ix = 0; ix * TILE_PIXEL_WIDTH * viewScale < widgetWidth; ++ix)
        for (iy = 0; iy * TILE_PIXEL_HEIGHT * viewScale < widgetHeight; ++iy)
        {
            float offsetX = (ix * tileWidth) - 1.0f;
            float offsetY = 1.0f - ((iy + 1) * tileHeight);

            GLuint tileBuffer = ctx->getGLBuf(ix, iy);
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

    ctx->stroke.reset(activeTool->newStroke(ctx, targetLayer));

    TileSet changedTiles = ctx->stroke->startStroke(pos, pressure);

    if(!changedTiles.empty())
    {
        ctx->dirtyTiles.insert(changedTiles.begin(), changedTiles.end());
        ctx->strokeModifiedTiles.insert(changedTiles.begin(), changedTiles.end());
        update();
    }
}

void CanvasWidget::strokeTo(QPointF pos, float pressure)
{
    mouseEventRate.addEvents(1);
    TileSet changedTiles;
    if (!ctx->stroke.isNull())
    {
        changedTiles = ctx->stroke->strokeTo(pos, pressure);
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
}

float CanvasWidget::getScale()
{
    return viewScale;
}

void CanvasWidget::setScale(float newScale)
{
    viewScale = qBound(0.25f, newScale, 4.0f);
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

    emit updateLayers();
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

    emit updateLayers();
}

bool CanvasWidget::getLayerVisible(int layerIndex)
{
    if (layerIndex < 0 || layerIndex >= ctx->layers.layers.size())
        return false;

    return ctx->layers.layers[layerIndex]->visible;
}

QList<CanvasWidget::LayerInfo> CanvasWidget::getLayerList()
{
    QList<LayerInfo> result;

    if (!ctx)
        return result;

    for (int i = ctx->layers.layers.size() - 1; i >= 0; --i)
    {
        CanvasLayer *layer = ctx->layers.layers[i];
        result.append((LayerInfo){layer->name, layer->visible});
    }

    return result;
}

void CanvasWidget::undo()
{
    if (ctx->undoHistory.empty())
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
    emit updateLayers();
}

void CanvasWidget::redo()
{
    if (ctx->redoHistory.empty())
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
    emit updateLayers();
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

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    if (ctx->inTabletStroke)
        return;

    QPointF pos = event->localPos() / viewScale;

    if (event->modifiers() && Qt::ControlModifier)
    {
        pickColorAt(pos.toPoint());
    }
    else
    {
        startStroke(pos, 1.0f);
    }

    event->accept();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (ctx->inTabletStroke)
        return;

    endStroke();

    event->accept();
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (ctx->inTabletStroke)
        return;

    QPointF pos = event->localPos() / viewScale;

    strokeTo(pos, 1.0f);

    event->accept();

    update();
}

void CanvasWidget::tabletEvent(QTabletEvent *event)
{
    QEvent::Type eventType = event->type();

    float xshift = event->hiResGlobalX() - event->globalX();
    float yshift = event->hiResGlobalY() - event->globalY();
    QPointF point = QPointF(event->x() + xshift, event->y() + yshift) / viewScale;

    if (eventType == QEvent::TabletPress)
    {
        if (event->modifiers() && Qt::ControlModifier)
        {
            pickColorAt(point.toPoint());
        }
        else
        {
            startStroke(point, event->pressure());
            ctx->inTabletStroke = true;
        }
    }
    else if (eventType == QEvent::TabletRelease)
    {
        ctx->inTabletStroke = false;
        endStroke();
    }
    else if (eventType == QEvent::TabletMove)
    {
        strokeTo(point, event->pressure());
    }
    else
        return;

    event->accept();
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

void CanvasWidget::openORA(QString path)
{
    ctx->clearUndoHistory();
    ctx->clearRedoHistory();
    ctx->clearTiles();
    loadStackFromORA(&ctx->layers, path);
    update();
    emit updateLayers();
}

void CanvasWidget::saveAsORA(QString path)
{
    saveStackAs(&ctx->layers, path);
}
