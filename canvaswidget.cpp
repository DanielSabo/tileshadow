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


static QOpenGLFunctions_3_2_Core *glFuncs;

class CanvasWidget::CanvasContext
{
public:
    CanvasContext() : vertexShader(0),
                      fragmentShader(0),
                      program(0),
                      vertexBuffer(0) {}

    GLuint vertexShader;
    GLuint fragmentShader;

    GLuint program;

    GLuint vertexBuffer;
    GLuint vertexArray;
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

    GLint compiled;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        qWarning() << "Shader compile failed, couldn't build " << path;
        GLint logSize;
        gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
        char* logMsg = new char[logSize];
        gl.glGetShaderInfoLog(shader, logSize, NULL, logMsg);
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
    glFuncs = new QOpenGLFunctions_3_2_Core();
    QOpenGLFunctions_3_2_Core &gl = *glFuncs;
    gl.initializeOpenGLFunctions();

    ctx->vertexShader = compileFile(gl, ":/CanvasShader.vert", GL_VERTEX_SHADER);
    ctx->fragmentShader = compileFile(gl, ":/CanvasShader.frag", GL_FRAGMENT_SHADER);

    if (ctx->vertexShader && ctx->fragmentShader)
    {
        GLuint program = gl.glCreateProgram();
        gl.glAttachShader(program, ctx->vertexShader);
        gl.glAttachShader(program, ctx->fragmentShader);
        gl.glBindFragDataLocation(program, 0, "fragColor");
        gl.glLinkProgram(program);

        GLint linked;
        gl.glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            qWarning() << "Failed to link shader";
            GLint logSize;
            gl.glGetProgramiv( program, GL_INFO_LOG_LENGTH, &logSize);
            char* logMsg = new char[logSize];
            gl.glGetProgramInfoLog(program, logSize, NULL, logMsg);
            qWarning() << logMsg;
            delete[] logMsg;
            gl.glDeleteProgram(program);
            program = 0;
        }

        ctx->program = program;
    }

    float positionData[] = {
            -0.2f, -0.2f, 0.0f,
             0.2f, -0.2f, 0.0f,
             0.2f,  0.2f, 0.0f
    };

    gl.glGenBuffers(1, &ctx->vertexBuffer);

    gl.glBindBuffer(GL_ARRAY_BUFFER, ctx->vertexBuffer);
    gl.glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), positionData,
                 GL_STATIC_DRAW);

    gl.glGenVertexArrays(1, &ctx->vertexArray);
    gl.glBindVertexArray(ctx->vertexArray);
    gl.glEnableVertexAttribArray(0);
    gl.glBindBuffer(GL_ARRAY_BUFFER, ctx->vertexBuffer);
    gl.glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte *)NULL);


//    cout << "GL Initialize" << endl;
    glClearColor(0.2, 0.2, 0.4, 1.0);
}

void CanvasWidget::resizeGL(int w, int h)
{
    glViewport( 0, 0, qMax(w, 1), qMax(h, 1));
}

void CanvasWidget::paintGL()
{
    QOpenGLFunctions_3_2_Core &gl = *glFuncs;

    glClear(GL_COLOR_BUFFER_BIT);
    gl.glUseProgram(ctx->program);
    check_gl_error();
    gl.glBindVertexArray(ctx->vertexArray);
    check_gl_error();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    check_gl_error();
    gl.glUseProgram(0);
    check_gl_error();
}
