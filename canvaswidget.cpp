#include "canvaswidget.h"
#include <iostream>
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
};

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

    float positionData[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };

    setBufferData(ctx->vertexBuffer, GL_ARRAY_BUFFER, sizeof(positionData), positionData, GL_STATIC_DRAW);

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

    for (ix = 0; ix < widgetWidth; ix += TILE_PIXEL_WIDTH)
        for (iy = 0; iy < widgetHeight; iy += TILE_PIXEL_HEIGHT)
        {
            float offsetX = (ix / TILE_PIXEL_WIDTH) * tileWidth - 1.0f;
            float offsetY = (iy / TILE_PIXEL_HEIGHT) * tileHeight - 1.0f;

            glFuncs->glUniform2f(locationTileOrigin, offsetX, offsetY);
            glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }

    glFuncs->glUseProgram(0);

    glFuncs->glBindTexture(GL_TEXTURE_2D, 0);
    glFuncs->glBindVertexArray(0);
}
