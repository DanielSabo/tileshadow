#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include "canvascontext.h"
#include "basicstrokecontext.h"
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
    ctx(NULL),
    activeBrush("default")
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
}

void CanvasWidget::resizeGL(int w, int h)
{
    glFuncs->glViewport( 0, 0, qMax(w, 1), qMax(h, 1));
}

void CanvasWidget::paintGL()
{
    ctx->closeTiles();

    int ix, iy;

    int widgetWidth  = width();
    int widgetHeight = height();

    float tileWidth  = (2.0f * TILE_PIXEL_WIDTH) / (widgetWidth);
    float tileHeight = (2.0f * TILE_PIXEL_HEIGHT) / (widgetHeight);

    glFuncs->glClearColor(0.2, 0.2, 0.4, 1.0);
    glFuncs->glClear(GL_COLOR_BUFFER_BIT);

    glFuncs->glUseProgram(ctx->program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glUniform1i(ctx->locationTileImage, 0);
    glFuncs->glUniform2f(ctx->locationTileSize, tileWidth, tileHeight);
    glFuncs->glUniform2f(ctx->locationTilePixels, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
    glFuncs->glBindVertexArray(ctx->vertexArray);

    for (ix = 0; ix * TILE_PIXEL_WIDTH < widgetWidth; ++ix)
        for (iy = 0; iy * TILE_PIXEL_HEIGHT < widgetHeight; ++iy)
        {
            float offsetX = (ix * tileWidth) - 1.0f;
            float offsetY = 1.0f - ((iy + 1) * tileHeight);

            CanvasTile *tile = ctx->getTile(ix, iy);
            glFuncs->glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, tile->tileBuffer);
            glFuncs->glUniform2f(ctx->locationTileOrigin, offsetX, offsetY);
            glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    // cout << "Click! " << ix << ", " << iy << endl;
    if (activeBrush == QString("debug"))
        ctx->stroke.reset(new BasicStrokeContext(ctx));
    else if (activeBrush == QString("default"))
        ctx->stroke.reset(new MyPaintStrokeContext(ctx));
    else
    {
        qWarning() << "Unknown tool set \'" << activeBrush << "\', using debug";
        ctx->stroke.reset(new BasicStrokeContext(ctx));
    }

    if(ctx->stroke->startStroke(event->localPos()))
        update();

    QGLWidget::mousePressEvent(event);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // cout << "Un-Click!" << endl;
    ctx->stroke.reset();
    QGLWidget::mouseReleaseEvent(event);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!ctx->stroke.isNull())
        if (ctx->stroke->strokeTo(event->localPos()))
            update();

    QGLWidget::mouseMoveEvent(event);
}

void CanvasWidget::setActiveTool(const QString &toolName)
{
    activeBrush = toolName;
}
