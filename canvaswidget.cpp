#include "canvaswidget.h"
#include "canvaswidget-opencl.h"
#include <iostream>
#include <map>
#include <list>
#include <algorithm>
#include <cmath>
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

void _check_cl_error(const char *file, int line, cl_int err);
#define check_cl_error(err) _check_cl_error(__FILE__,__LINE__,err)

void _check_cl_error(const char *file, int line, cl_int err) {
    if (err != CL_SUCCESS)
    {
        static const char* strings[] =
        {
            /* Error Codes */
              "success"                         /*  0  */
            , "device not found"                /* -1  */
            , "device not available"            /* -2  */
            , "compiler not available"          /* -3  */
            , "mem object allocation failure"   /* -4  */
            , "out of resources"                /* -5  */
            , "out of host memory"              /* -6  */
            , "profiling info not available"    /* -7  */
            , "mem copy overlap"                /* -8  */
            , "image format mismatch"           /* -9  */
            , "image format not supported"      /* -10 */
            , "build program failure"           /* -11 */
            , "map failure"                     /* -12 */
            , "unknown error"                   /* -13 */
            , "unknown error"                   /* -14 */
            , "unknown error"                   /* -15 */
            , "unknown error"                   /* -16 */
            , "unknown error"                   /* -17 */
            , "unknown error"                   /* -18 */
            , "unknown error"                   /* -19 */
            , "unknown error"                   /* -20 */
            , "unknown error"                   /* -21 */
            , "unknown error"                   /* -22 */
            , "unknown error"                   /* -23 */
            , "unknown error"                   /* -24 */
            , "unknown error"                   /* -25 */
            , "unknown error"                   /* -26 */
            , "unknown error"                   /* -27 */
            , "unknown error"                   /* -28 */
            , "unknown error"                   /* -29 */
            , "invalid value"                   /* -30 */
            , "invalid device type"             /* -31 */
            , "invalid platform"                /* -32 */
            , "invalid device"                  /* -33 */
            , "invalid context"                 /* -34 */
            , "invalid queue properties"        /* -35 */
            , "invalid command queue"           /* -36 */
            , "invalid host ptr"                /* -37 */
            , "invalid mem object"              /* -38 */
            , "invalid image format descriptor" /* -39 */
            , "invalid image size"              /* -40 */
            , "invalid sampler"                 /* -41 */
            , "invalid binary"                  /* -42 */
            , "invalid build options"           /* -43 */
            , "invalid program"                 /* -44 */
            , "invalid program executable"      /* -45 */
            , "invalid kernel name"             /* -46 */
            , "invalid kernel definition"       /* -47 */
            , "invalid kernel"                  /* -48 */
            , "invalid arg index"               /* -49 */
            , "invalid arg value"               /* -50 */
            , "invalid arg size"                /* -51 */
            , "invalid kernel args"             /* -52 */
            , "invalid work dimension"          /* -53 */
            , "invalid work group size"         /* -54 */
            , "invalid work item size"          /* -55 */
            , "invalid global offset"           /* -56 */
            , "invalid event wait list"         /* -57 */
            , "invalid event"                   /* -58 */
            , "invalid operation"               /* -59 */
            , "invalid gl object"               /* -60 */
            , "invalid buffer size"             /* -61 */
            , "invalid mip level"               /* -62 */
            , "invalid global work size"        /* -63 */
        };
        static const int strings_len = sizeof(strings) / sizeof(strings[0]);

        const char *error_str;

        if (-err < 0 || -err >= strings_len)
            error_str = "unknown error";
        else
            error_str = strings[-err];

        qWarning() << "OpenCL Error (" << err << "): " << error_str << " - " << file << ":" << line;
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
}

static const int TILE_PIXEL_WIDTH  = 64;
static const int TILE_PIXEL_HEIGHT = 64;
static const int TILE_COMP_TOTAL = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4;

class CanvasTile
{
public:
    CanvasTile();
    ~CanvasTile();

    bool    isOpen;
    cl_mem  tileMem;
    float  *tileData;
    GLuint  tileTex;

    void mapHost(void);
    void unmapHost(void);
};

class CanvasWidget::CanvasContext;

class StrokeContext
{
public:
    StrokeContext(CanvasWidget::CanvasContext *ctx) : ctx(ctx) {}
    virtual ~StrokeContext() {}

    virtual bool startStroke(QPointF point) = 0;
    virtual bool strokeTo(QPointF point) = 0;

    CanvasWidget::CanvasContext *ctx;
};

class BasicStrokeContext : public StrokeContext
{
public:
    BasicStrokeContext(CanvasWidget::CanvasContext *ctx) : StrokeContext(ctx) {}

    bool startStroke(QPointF point);
    bool strokeTo(QPointF point);
    void drawDab(QPointF point);

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
                      tileHeight(0) {}

    GLuint vertexShader;
    GLuint fragmentShader;

    GLuint program;
    GLuint locationTileOrigin;
    GLuint locationTileSize;
    GLuint locationTileImage;


    GLuint vertexBuffer;
    GLuint vertexArray;

    GLuint tileBuffer;

    int tileWidth;
    int tileHeight;

    QScopedPointer<StrokeContext> stroke;

    std::map<uint64_t, CanvasTile *> tiles;
    std::list<CanvasTile *> openTiles;

    CanvasTile *getTile(int x, int y);
    cl_mem clOpenTile(CanvasTile *tile);
    float *openTile(CanvasTile *tile);
    void closeTile(CanvasTile *tile);
    void closeTiles(void);
};

CanvasTile::CanvasTile()
{
    isOpen = false;
    tileMem = 0;
    tileData = NULL;
    tileMem = clCreateBuffer (SharedOpenCL::getSharedOpenCL()->ctx, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                              TILE_COMP_TOTAL * sizeof(float), tileData, NULL);
}

CanvasTile::~CanvasTile(void)
{
    unmapHost();
    clReleaseMemObject(tileMem);
}

void CanvasTile::mapHost(void)
{
    if (!tileData)
    {
        cl_int err = CL_SUCCESS;
        tileData = (float *)clEnqueueMapBuffer (SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem,
                                                      CL_TRUE, CL_MAP_READ | CL_MAP_WRITE,
                                                      0, TILE_COMP_TOTAL * sizeof(float),
                                                      0, NULL, NULL, &err);
    }
}

void CanvasTile::unmapHost()
{
    if (tileData)
    {
        clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, NULL, NULL);
        tileData = NULL;
    }
}

CanvasTile *CanvasWidget::CanvasContext::getTile(int x, int y)
{
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;

    std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

    if (found != tiles.end())
        return found->second;

    CanvasTile *tile = new CanvasTile();

    cl_int err = CL_SUCCESS;
    cl_mem data = clOpenTile(tile);
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->fillKernel;
    const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};
    float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
    err = clSetKernelArg(kernel, 1, sizeof(cl_float4), (void *)&pixel);
    err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                 kernel, 1,
                                 NULL, global_work_size, NULL,
                                 0, NULL, NULL);

    glFuncs->glGenTextures(1, &tile->tileTex);
    glFuncs->glBindTexture(GL_TEXTURE_2D, tile->tileTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);

    closeTile(tile);

    // cout << "Added tile at " << x << "," << y << endl;
    tiles[key] = tile;

    return tile;
}

cl_mem CanvasWidget::CanvasContext::clOpenTile(CanvasTile *tile)
{
    if (!tile->isOpen)
    {
        openTiles.push_front(tile);
    }

    tile->unmapHost();

    tile->isOpen = true;

    return tile->tileMem;
}

float *CanvasWidget::CanvasContext::openTile(CanvasTile *tile)
{
    if (!tile->isOpen)
    {
        openTiles.push_front(tile);
    }

    tile->mapHost();

    tile->isOpen = true;

    return tile->tileData;
}

void CanvasWidget::CanvasContext::closeTile(CanvasTile *tile)
{
    if (!tile->isOpen)
        return;

    tile->mapHost();

    /* Push the new data to OpenGL */
    glFuncs->glBindTexture(GL_TEXTURE_2D, tile->tileTex);
    glFuncs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, tileBuffer);
    glFuncs->glBufferData(GL_PIXEL_UNPACK_BUFFER,
                          sizeof(float) * TILE_COMP_TOTAL,
                          tile->tileData,
                          GL_STREAM_DRAW);
    glFuncs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);

    tile->isOpen = false;
}

void CanvasWidget::CanvasContext::closeTiles(void)
{
    while(!openTiles.empty())
    {
        CanvasTile *tile = openTiles.front();

        closeTile(tile);

        openTiles.pop_front();
    }
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
        ctx->locationTileOrigin = glFuncs->glGetUniformLocation(ctx->program, "tileOrigin");
        ctx->locationTileSize   = glFuncs->glGetUniformLocation(ctx->program, "tileSize");
        ctx->locationTileImage  = glFuncs->glGetUniformLocation(ctx->program, "tileImage");

    }

    glFuncs->glGenBuffers(1, &ctx->tileBuffer);
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
    glFuncs->glBindVertexArray(ctx->vertexArray);

    for (ix = 0; ix * TILE_PIXEL_WIDTH < widgetWidth; ++ix)
        for (iy = 0; iy * TILE_PIXEL_HEIGHT < widgetHeight; ++iy)
        {
            float offsetX = (ix * tileWidth) - 1.0f;
            float offsetY = 1.0f - ((iy + 1) * tileHeight);

            CanvasTile *tile = ctx->getTile(ix, iy);
            glFuncs->glBindTexture(GL_TEXTURE_2D, tile->tileTex);

            glFuncs->glUniform2f(ctx->locationTileOrigin, offsetX, offsetY);
            glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
}

void BasicStrokeContext::drawDab(QPointF point)
{
    cl_int radius = 10;
    int ix_start = (point.x() - radius) / TILE_PIXEL_WIDTH;
    int iy_start = (point.y() - radius) / TILE_PIXEL_HEIGHT;

    int ix_end   = (point.x() + radius) / TILE_PIXEL_WIDTH;
    int iy_end   = (point.y() + radius) / TILE_PIXEL_HEIGHT;

    const size_t global_work_size[2] = {TILE_PIXEL_HEIGHT, TILE_PIXEL_WIDTH};
    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->circleKernel;

    cl_int err = CL_SUCCESS;
    cl_int stride = TILE_PIXEL_WIDTH;

    err = clSetKernelArg(kernel, 1, sizeof(cl_int), (void *)&stride);
    err = clSetKernelArg(kernel, 4, sizeof(cl_int), (void *)&radius);

    for (int iy = iy_start; iy <= iy_end; ++iy)
    {
        for (int ix = ix_start; ix <= ix_end; ++ix)
        {
            CanvasTile *tile = ctx->getTile(ix, iy);

            float pixel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            pixel[(ix + iy) % 3] = 0.0f;

            cl_int offsetX = point.x() - (ix * TILE_PIXEL_WIDTH);
            cl_int offsetY = point.y() - (iy * TILE_PIXEL_HEIGHT);
            cl_mem data = ctx->clOpenTile(tile);

            err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&data);
            err = clSetKernelArg(kernel, 2, sizeof(cl_int), (void *)&offsetX);
            err = clSetKernelArg(kernel, 3, sizeof(cl_int), (void *)&offsetY);
            err = clSetKernelArg(kernel, 5, sizeof(cl_float4), (void *)&pixel);
            err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                         kernel, 2,
                                         NULL, global_work_size, NULL,
                                         0, NULL, NULL);
        }
    }

    lastDab = point;
}

bool BasicStrokeContext::startStroke(QPointF point)
{
    start = point;
    drawDab(point);
    return true;
}

bool BasicStrokeContext::strokeTo(QPointF point)
{
    float dist = sqrtf(powf(lastDab.x() - point.x(), 2.0f) + powf(lastDab.y() - point.y(), 2.0f));

    if (dist >= 1.0f)
    {
        int x0 = lastDab.x();
        int y0 = lastDab.y();
        int x1 = point.x();
        int y1 = point.y();
        int x = x0;
        int y = y0;

        bool steep = false;
        int sx, sy;
        int dx = abs(x1 - x0);
        if ((x1 - x0) > 0)
          sx = 1;
        else
          sx = -1;

        int dy = abs(y1 - y0);
        if ((y1 - y0) > 0)
          sy = 1;
        else
          sy = -1;

        if (dy > dx)
        {
          steep = true;
          std::swap(x, y);
          std::swap(dy, dx);
          std::swap(sy, sx);
        }

        int d = (2 * dy) - dx;

        for (int i = 0; i < dx; ++i)
        {
            if (steep)
                drawDab(QPointF(y, x));
            else
                drawDab(QPointF(x, y));

            while (d >= 0)
            {
                y = y + sy;
                d = d - (2 * dx);
            }

            x = x + sx;
            d = d + (2 * dy);
        }
        drawDab(QPointF(x1, y1));
        return true;
    }
    return false;
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    // cout << "Click! " << ix << ", " << iy << endl;
    ctx->stroke.reset(new BasicStrokeContext(ctx));

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
