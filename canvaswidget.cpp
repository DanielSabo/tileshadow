#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include "canvascontext.h"
#include "basicstrokecontext.h"
#include "ora.h"
#include <iostream>
#include <map>
#include <list>
#include <QMouseEvent>
#include <QFile>
#include <QDebug>
#include <QOpenGLFunctions_3_2_Core>

using namespace std;

#include "mypaintstrokecontext.h"

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
    activeBrush("debug"),
    toolSizeFactor(1.0f),
    toolColor(QColor::fromRgbF(0.0, 0.0, 0.0)),
    viewScale(1.0f),
    lastNewLayerNumber(0)
{
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

    ctx->vertexShader = compileFile(*glFuncs, ":/CanvasShader.vert", GL_VERTEX_SHADER);
    ctx->fragmentShader = compileFile(*glFuncs, ":/CanvasShader.frag", GL_FRAGMENT_SHADER);

    if (ctx->vertexShader && ctx->fragmentShader)
    {
        GLuint program = glFuncs->glCreateProgram();
        glFuncs->glAttachShader(program, ctx->vertexShader);
        glFuncs->glAttachShader(program, ctx->fragmentShader);
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

        ctx->program = program;
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

    glFuncs->glClearColor(0.2, 0.2, 0.4, 1.0);
    glFuncs->glClear(GL_COLOR_BUFFER_BIT);

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
}

void CanvasWidget::startStroke(QPointF pos, float pressure)
{
    ctx->stroke.reset(NULL);

    if (ctx->layers.layers.empty())
        return;

    CanvasLayer *targetLayer = ctx->layers.layers.at(ctx->currentLayer);

    if (activeBrush.endsWith(".myb"))
    {
        MyPaintStrokeContext *mypaint = new MyPaintStrokeContext(ctx, targetLayer);
        if (mypaint->fromJsonFile(QString(":/mypaint-tools/") + activeBrush))
            ctx->stroke.reset(mypaint);
        else
            delete mypaint;
    }
    else if (activeBrush == QString("debug"))
        ctx->stroke.reset(new BasicStrokeContext(ctx, targetLayer));

    if (ctx->stroke.isNull())
    {
        qWarning() << "Unknown tool set \'" << activeBrush << "\', using debug";
        ctx->stroke.reset(new BasicStrokeContext(ctx, targetLayer));
    }

    ctx->stroke->multiplySize(toolSizeFactor);
    ctx->stroke->setColor(toolColor);

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
    CanvasUndoLayers *undoEvent = new CanvasUndoLayers(&ctx->layers, ctx->currentLayer);
    ctx->undoHistory.prepend(undoEvent);

    ctx->layers.newLayerAt(layerIndex + 1, QString().sprintf("Layer %02d", ++lastNewLayerNumber));
    emit updateLayers();
}

void CanvasWidget::removeLayer(int layerIndex)
{
    if (layerIndex < 0 || layerIndex > ctx->layers.layers.size())
        return;

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

QList<QString> CanvasWidget::getLayerList()
{
    QList<QString> result;

    if (!ctx)
        return result;

    for (int i = ctx->layers.layers.size() - 1; i >= 0; --i)
        result.append(ctx->layers.layers[i]->name);

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

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF pos = event->localPos() / viewScale;

    startStroke(pos, 1.0f);

    event->accept();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    endStroke();

    event->accept();
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPointF pos = event->localPos() / viewScale;

    strokeTo(pos, 1.0f);

    event->accept();
}

void CanvasWidget::tabletEvent(QTabletEvent *event)
{
    QEvent::Type eventType = event->type();

    float xshift = event->hiResGlobalX() - event->globalX();
    float yshift = event->hiResGlobalY() - event->globalY();
    QPointF point = QPointF(event->x() + xshift, event->y() + yshift) / viewScale;

    if (eventType == QEvent::TabletPress)
        startStroke(point, event->pressure());
    else if (eventType == QEvent::TabletRelease)
        endStroke();
    else if (eventType == QEvent::TabletMove)
        strokeTo(point, event->pressure());
    else
        return;

    event->accept();
}

void CanvasWidget::setActiveTool(const QString &toolName)
{
    activeBrush = toolName;
}

void CanvasWidget::setToolSizeFactor(float multipler)
{
    toolSizeFactor = multipler;
}

void CanvasWidget::setToolColor(const QColor &color)
{
    toolColor = color;
}

void CanvasWidget::openORA(QString path)
{
    loadStackFromORA(&ctx->layers, path);
    ctx->glTiles.clear();
    update();
    emit updateLayers();
}

void CanvasWidget::saveAsORA(QString path)
{
    saveStackAs(&ctx->layers, path);
}
