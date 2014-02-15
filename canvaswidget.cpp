#include "canvaswidget.h"
#include <iostream>
#include <map>
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

static inline void setBufferData(GLuint bufferObj, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{

    glFuncs->glBindBuffer(target, bufferObj);
    glFuncs->glBufferData(target, size, data, usage);
    glFuncs->glBindBuffer(target, 0);
}

static const int TILE_PIXEL_WIDTH  = 64;
static const int TILE_PIXEL_HEIGHT = 64;

class CanvasTile
{
public:
    CanvasTile() {}

    float  *tileData;
    GLuint  tileBuffer;
    GLuint  tileTex;
};

class StrokeContext
{
public:
    StrokeContext() : inStroke(false) {}

    bool    inStroke;
    QPointF start;
    QPointF lastDab;
};

class CanvasWidget::CanvasContext
{
public:
    CanvasContext() : vertexShader(0),
                      fragmentShader(0),
                      program(0),
                      vertexBuffer(0),
                      tileWidth(0),
                      tileHeight(0),
                      testPatternTex(0) {}

    GLuint vertexShader;
    GLuint fragmentShader;

    GLuint program;

    GLuint vertexBuffer;
    GLuint vertexArray;

    int tileWidth;
    int tileHeight;

    GLuint testPatternBuffer;
    GLuint testPatternTex;

    StrokeContext stroke;

    std::map<uint64_t, CanvasTile *> tiles;

    CanvasTile *getTile(int x, int y);
};

CanvasTile *
CanvasWidget::CanvasContext::getTile(int x, int y)
{
    static const int TILE_COMP_COUNT = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4;
    int i;
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;

    std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

    if (found != tiles.end())
        return found->second;

    CanvasTile *tile = new CanvasTile();

    tile->tileData = new float[TILE_COMP_COUNT];

    for (i = 0; i < TILE_COMP_COUNT; i += 4)
    {
        tile->tileData[i + 0] = 1.0f;
        tile->tileData[i + 1] = 1.0f;
        tile->tileData[i + 2] = 1.0f;
        tile->tileData[i + 3] = 1.0f;
    }

    glFuncs->glGenBuffers(1, &tile->tileBuffer);
    glFuncs->glGenTextures(1, &tile->tileTex);
    glFuncs->glBindTexture(GL_TEXTURE_2D, tile->tileTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFuncs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, tile->tileBuffer);
    glFuncs->glBufferData(GL_PIXEL_UNPACK_BUFFER,
                          sizeof(float) * TILE_COMP_COUNT,
                          tile->tileData,
                          GL_DYNAMIC_DRAW);
    glFuncs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT, 0, GL_BGRA, GL_FLOAT, 0);
    glFuncs->glBindTexture(GL_TEXTURE_2D, 0);
    glFuncs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    cout << "Added tile at " << x << "," << y << endl;
    tiles[key] = tile;

    return tile;
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

        single->setVersion( 3, 2 );
        single->setProfile( QGLFormat::CoreProfile );
        single->setSampleBuffers( true );
    }

    return *single;
}

CanvasWidget::CanvasWidget(QWidget *parent) :
    QGLWidget(getFormatSingleton(), parent), ctx(new CanvasContext)
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
    glFuncs->initializeOpenGLFunctions();

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
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glFuncs->glBindVertexArray(0);

    QImage tileDataImg = QImage(QString(":/TestPatternTile.png"));

    if (tileDataImg.isNull())
        qWarning() << "Failed to load test pattern.";
    else if (QImage::Format_RGB32 != tileDataImg.format())
        qWarning() << "Test pattern image has the wrong format.";
    else
    {
        cout << "Test Image:" << tileDataImg.width() << "x" << tileDataImg.height() << " " << tileDataImg.byteCount() << endl;

        glFuncs->glGenBuffers(1, &ctx->testPatternBuffer);
        glFuncs->glGenTextures(1, &ctx->testPatternTex);
        glFuncs->glBindTexture(GL_TEXTURE_2D, ctx->testPatternTex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFuncs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, ctx->testPatternBuffer);
        glFuncs->glBufferData(GL_PIXEL_UNPACK_BUFFER,
                              tileDataImg.byteCount(),
                              (const uchar *)tileDataImg.bits(),
                              GL_DYNAMIC_DRAW);
        check_gl_error();
        glFuncs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileDataImg.width(), tileDataImg.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
        check_gl_error();
        glFuncs->glBindTexture(GL_TEXTURE_2D, 0);
        glFuncs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
}

void CanvasWidget::resizeGL(int w, int h)
{
    glFuncs->glViewport( 0, 0, qMax(w, 1), qMax(h, 1));
}

void CanvasWidget::paintGL()
{
    int ix, iy;

    int widgetWidth  = width();
    int widgetHeight = height();

    float tileWidth  = (2.0f * TILE_PIXEL_WIDTH) / (widgetWidth);
    float tileHeight = (2.0f * TILE_PIXEL_HEIGHT) / (widgetHeight);

    GLuint locationTileOrigin = glFuncs->glGetUniformLocation(ctx->program, "tileOrigin");
    GLuint locationTileSize   = glFuncs->glGetUniformLocation(ctx->program, "tileSize");
    GLuint locationTileImage  = glFuncs->glGetUniformLocation(ctx->program, "tileImage");

    glFuncs->glClearColor(0.2, 0.2, 0.4, 1.0);
    glFuncs->glClear(GL_COLOR_BUFFER_BIT);

    glFuncs->glUseProgram(ctx->program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glBindTexture(GL_TEXTURE_2D, ctx->testPatternTex);
    glFuncs->glUniform1i(locationTileImage, 0);

    glFuncs->glUniform2f(locationTileSize, tileWidth, tileHeight);
    glFuncs->glBindVertexArray(ctx->vertexArray);

    for (ix = 0; ix * TILE_PIXEL_WIDTH < widgetWidth; ++ix)
        for (iy = 0; iy * TILE_PIXEL_HEIGHT < widgetHeight; ++iy)
        {
            float offsetX = (ix * tileWidth) - 1.0f;
            float offsetY = 1.0f - ((iy + 1) * tileHeight);

            CanvasTile *tile = ctx->getTile(ix, iy);
            glFuncs->glBindTexture(GL_TEXTURE_2D, tile->tileTex);

            glFuncs->glUniform2f(locationTileOrigin, offsetX, offsetY);
            glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }

    glFuncs->glUseProgram(0);

    glFuncs->glBindTexture(GL_TEXTURE_2D, 0);
    glFuncs->glBindVertexArray(0);
}

static void drawDab(CanvasWidget::CanvasContext *ctx, QMouseEvent *event)
{
    static const int TILE_COMP_COUNT = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4;
    int ix = event->x() / TILE_PIXEL_WIDTH;
    int iy = event->y() / TILE_PIXEL_HEIGHT;
    CanvasTile *tile = ctx->getTile(ix, iy);

    int offsetX = event->x() - (ix * TILE_PIXEL_WIDTH);
    int offsetY = event->y() - (iy * TILE_PIXEL_HEIGHT);

    float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f, };

    pixel[(ix + iy) % 3] = 0.0f;

    float *writePixel = tile->tileData + (TILE_PIXEL_WIDTH * offsetY + offsetX) * 4;

    writePixel[0] = pixel[0];
    writePixel[1] = pixel[1];
    writePixel[2] = pixel[2];
    writePixel[3] = pixel[3];

    ctx->stroke.lastDab = event->localPos();

    glFuncs->glBindTexture(GL_TEXTURE_2D, tile->tileTex);
    glFuncs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, tile->tileBuffer);
    glFuncs->glBufferData(GL_PIXEL_UNPACK_BUFFER,
                          sizeof(float) * TILE_COMP_COUNT,
                          tile->tileData,
                          GL_DYNAMIC_DRAW);
    glFuncs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT, 0, GL_BGRA, GL_FLOAT, 0);
    glFuncs->glBindTexture(GL_TEXTURE_2D, 0);
    glFuncs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    // cout << "Click! " << ix << ", " << iy << endl;
    ctx->stroke.inStroke = true;
    ctx->stroke.start = event->localPos();

    drawDab(ctx, event);
    update();

    QGLWidget::mousePressEvent(event);
}
void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // cout << "Un-Click!" << endl;
    ctx->stroke.inStroke = false;
    QGLWidget::mouseReleaseEvent(event);
}
void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (ctx->stroke.lastDab != event->localPos())
    {
        drawDab(ctx, event);
        update();
    }
    QGLWidget::mouseMoveEvent(event);
}
