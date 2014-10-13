#include "canvasrender.h"
#include "canvascontext.h"
#include "canvastile.h"
#include "glhelper.h"

#include <QColor>
#include <QDebug>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenCL/cl_gl.h>
#else
#include <CL/cl_gl.h>
#endif

CanvasRender::CanvasRender() :
    glFuncs(new QOpenGLFunctions_3_2_Core()),
    backgroundGLTile(0)
{
    if (!glFuncs->initializeOpenGLFunctions())
    {
        qWarning() << "Could not initialize OpenGL Core Profile 3.2";
        exit(1);
    }

#ifdef __APPLE__
    {
        // Extra steps to disable vsync on OSX
        GLint value = 0;
        CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &value);
    }
#endif

    /* Set up canvas tile data & shaders */
    GLuint vertexShader = compileGLShaderFile(glFuncs, ":/CanvasShader.vert", GL_VERTEX_SHADER);
    GLuint fragmentShader = compileGLShaderFile(glFuncs, ":/CanvasShader.frag", GL_FRAGMENT_SHADER);
    tileShader.program = buildGLProgram(glFuncs, vertexShader, fragmentShader);

    if (vertexShader)
        glFuncs->glDeleteShader(vertexShader);
    if (fragmentShader)
        glFuncs->glDeleteShader(fragmentShader);

    if (tileShader.program)
    {
        tileShader.tileOrigin = glFuncs->glGetUniformLocation(tileShader.program, "tileOrigin");
        tileShader.tileSize   = glFuncs->glGetUniformLocation(tileShader.program, "tileSize");
        tileShader.tileImage  = glFuncs->glGetUniformLocation(tileShader.program, "tileImage");
        tileShader.tilePixels = glFuncs->glGetUniformLocation(tileShader.program, "tilePixels");
        tileShader.binSize    = glFuncs->glGetUniformLocation(tileShader.program, "binSize");
    }

    glFuncs->glGenBuffers(1, &tileShader.vertexBuffer);
    glFuncs->glGenVertexArrays(1, &tileShader.vertexArray);
    glFuncs->glBindVertexArray(tileShader.vertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, tileShader.vertexBuffer);

    float positionData[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };

    glFuncs->glBufferData(GL_ARRAY_BUFFER, sizeof(positionData), positionData, GL_STATIC_DRAW);

    glFuncs->glEnableVertexAttribArray(0);
    glFuncs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    /* Set up cursor data & shaders */
    vertexShader = compileGLShaderFile(glFuncs, ":/CursorCircle.vert", GL_VERTEX_SHADER);
    fragmentShader = compileGLShaderFile(glFuncs, ":/CursorCircle.frag", GL_FRAGMENT_SHADER);
    cursorShader.program = buildGLProgram(glFuncs, vertexShader, fragmentShader);

    if (vertexShader)
        glFuncs->glDeleteShader(vertexShader);
    if (fragmentShader)
        glFuncs->glDeleteShader(fragmentShader);

    if (cursorShader.program)
    {
        cursorShader.dimensions = glFuncs->glGetUniformLocation(cursorShader.program, "dimensions");
        cursorShader.pixelRadius = glFuncs->glGetUniformLocation(cursorShader.program, "pixelRadius");
    }

    float cursorVertData[] = {
        -1.0f, -1.0f,
        1.0f,  -1.0f,
        1.0f,  1.0f,
        -1.0f, 1.0f,
    };

    glFuncs->glGenBuffers(1, &cursorShader.vertexBuffer);
    glFuncs->glGenVertexArrays(1, &cursorShader.vertexArray);
    glFuncs->glBindVertexArray(cursorShader.vertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, cursorShader.vertexBuffer);

    glFuncs->glBufferData(GL_ARRAY_BUFFER, sizeof(cursorVertData), cursorVertData, GL_STATIC_DRAW);

    glFuncs->glEnableVertexAttribArray(0);
    glFuncs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    /* Color dot data & shaders */
    vertexShader = compileGLShaderFile(glFuncs, ":/CursorCircle.vert", GL_VERTEX_SHADER);
    fragmentShader = compileGLShaderFile(glFuncs, ":/ColorDot.frag", GL_FRAGMENT_SHADER);
    colorDotShader.program = buildGLProgram(glFuncs, vertexShader, fragmentShader);

    if (vertexShader)
        glFuncs->glDeleteShader(vertexShader);
    if (fragmentShader)
        glFuncs->glDeleteShader(fragmentShader);

    if (colorDotShader.program)
    {
        colorDotShader.dimensions = glFuncs->glGetUniformLocation(colorDotShader.program, "dimensions");
        colorDotShader.pixelRadius = glFuncs->glGetUniformLocation(colorDotShader.program, "pixelRadius");
        colorDotShader.previewColor =  glFuncs->glGetUniformLocation(colorDotShader.program, "previewColor");
    }

    float dotVertData[] = {
        -1.0f, -1.0f,
        1.0f,  -1.0f,
        1.0f,  1.0f,
        -1.0f, 1.0f,
    };

    glFuncs->glGenBuffers(1, &colorDotShader.vertexBuffer);
    glFuncs->glGenVertexArrays(1, &colorDotShader.vertexArray);
    glFuncs->glBindVertexArray(colorDotShader.vertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, colorDotShader.vertexBuffer);

    glFuncs->glBufferData(GL_ARRAY_BUFFER, sizeof(dotVertData), dotVertData, GL_STATIC_DRAW);

    glFuncs->glEnableVertexAttribArray(0);
    glFuncs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    /* Framebuffers */
    glFuncs->glGenFramebuffers(1, &backbufferFramebuffer);
    glFuncs->glGenRenderbuffers(1, &backbufferRenderbuffer);
}

CanvasRender::~CanvasRender()
{
    clearTiles();

    if (tileShader.vertexBuffer)
        glFuncs->glDeleteBuffers(1, &tileShader.vertexBuffer);

    if (tileShader.vertexArray)
        glFuncs->glDeleteVertexArrays(1, &tileShader.vertexArray);

    if (tileShader.program)
        glFuncs->glDeleteProgram(tileShader.program);

    if (cursorShader.program)
        glFuncs->glDeleteProgram(cursorShader.program);

    if (cursorShader.vertexBuffer)
        glFuncs->glDeleteBuffers(1, &cursorShader.vertexBuffer);

    if (cursorShader.vertexArray)
        glFuncs->glDeleteVertexArrays(1, &cursorShader.vertexArray);

    if (colorDotShader.program)
        glFuncs->glDeleteProgram(colorDotShader.program);

    if (colorDotShader.vertexBuffer)
        glFuncs->glDeleteBuffers(1, &colorDotShader.vertexBuffer);

    if (colorDotShader.vertexArray)
        glFuncs->glDeleteVertexArrays(1, &colorDotShader.vertexArray);

    if (backgroundGLTile)
        glFuncs->glDeleteBuffers(1, &backgroundGLTile);

    if (backbufferFramebuffer)
        glFuncs->glDeleteFramebuffers(1, &backbufferFramebuffer);

    if (backbufferRenderbuffer)
        glFuncs->glDeleteRenderbuffers(1, &backbufferRenderbuffer);
}

void CanvasRender::resizeFramebuffer(int w, int h)
{
    QSize view = QSize(w, h);

    if (viewSize != view)
    {
        viewSize = view;

        if (viewSize.width() <= 0)
            viewSize.setWidth(1);
        if (viewSize.height() <= 0)
            viewSize.setHeight(1);

        glFuncs->glBindRenderbuffer(GL_RENDERBUFFER, backbufferRenderbuffer);
        glFuncs->glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, viewSize.width(), viewSize.height());

        glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, backbufferFramebuffer);
        glFuncs->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, backbufferRenderbuffer);

        glFuncs->glClearColor(1.0, 0.0, 0.0, 1.0);
        glFuncs->glClear(GL_COLOR_BUFFER_BIT);

        glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    }
}

void CanvasRender::clearTiles()
{
    GLTileMap::iterator iter;
    for (iter = glTiles.begin(); iter != glTiles.end(); ++iter)
    {
        GLuint tileBuffer = iter->second;

        if (tileBuffer)
            glFuncs->glDeleteBuffers(1, &tileBuffer);
    }
    glTiles.clear();
}

void CanvasRender::updateBackgroundTile(CanvasContext *ctx)
{
    if (!backgroundGLTile)
    {
        glFuncs->glGenBuffers(1, &backgroundGLTile);
    }

    glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, backgroundGLTile);
    glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                          sizeof(GLubyte) * TILE_COMP_TOTAL,
                          nullptr,
                          GL_STATIC_DRAW);

    GLubyte *dstData = (GLubyte *)glFuncs->glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
    float *srcData = ctx->layers.backgroundTile->mapHost();
    for (int i = 0; i < TILE_COMP_TOTAL; ++i)
        dstData[i] = srcData[i] * 0xFF;

    glFuncs->glUnmapBuffer(GL_TEXTURE_BUFFER);
}

GLuint CanvasRender::getGLBuf(int x, int y)
{
    GLTileMap::iterator found = glTiles.find(QPoint(x, y));

    if (found != glTiles.end())
    {
        if (found->second)
            return found->second;
        else
            return backgroundGLTile;
    }

    return backgroundGLTile;
}

void CanvasRender::renderTile(int x, int y, CanvasTile *tile)
{
    GLuint tileBuffer = glTiles[QPoint(x, y)];

    if (!tileBuffer && tile)
    {
        qDebug() << "renderTile can't render uninitialized tile" << x << y;
        return;
    }

    if (!tile)
    {
        if (tileBuffer)
            qDebug() << "renderTile can't delete" << x << y;

        return;
    }

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
    {
        cl_int err = CL_SUCCESS;
        cl_command_queue cmdQueue = SharedOpenCL::getSharedOpenCL()->cmdQueue;
        cl_context context = SharedOpenCL::getSharedOpenCL()->ctx;

        cl_mem output = clCreateFromGLBuffer(context,
                                             CL_MEM_WRITE_ONLY,
                                             tileBuffer,
                                             &err);

        err = clEnqueueAcquireGLObjects(cmdQueue, 1, &output, 0, nullptr, nullptr);

        cl_mem input = tile->unmapHost();

        cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->floatToU8;

        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&input);
        err = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&output);

        size_t workSize = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT;

        err = clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                                     kernel, 1,
                                     nullptr, &workSize, nullptr,
                                     0, nullptr, nullptr);

        err = clEnqueueReleaseGLObjects(cmdQueue, 1, &output, 0, nullptr, nullptr);

        err = clReleaseMemObject(output);
    }
    else
    {
        glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, tileBuffer);
        GLubyte *dstData = (GLubyte *)glFuncs->glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
        float *srcData = tile->mapHost();
        for (int i = 0; i < TILE_COMP_TOTAL; ++i)
            dstData[i] = srcData[i] * 0xFF;
        glFuncs->glUnmapBuffer(GL_TEXTURE_BUFFER);
    }
}

void CanvasRender::ensureTiles(TileMap const &tiles)
{
    if (tiles.empty())
        return;

    for (auto &iter: tiles)
    {
        GLuint &ref = glTiles[iter.first];

        if (iter.second)
        {
            if (!ref)
            {
                glFuncs->glGenBuffers(1, &ref);
                glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, ref);
                glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                                      sizeof(GLubyte) * TILE_COMP_TOTAL,
                                      nullptr,
                                      GL_DYNAMIC_DRAW);
            }
        }
        else
        {
            if (ref)
                glFuncs->glDeleteBuffers(1, &ref);
            ref = 0;
        }
    }

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
        glFinish();
}

void CanvasRender::renderTileMap(TileMap &tiles)
{
    if (tiles.empty())
        return;

    ensureTiles(tiles);

    for (auto &iter: tiles)
        renderTile(iter.first.x(), iter.first.y(), iter.second.get());

    tiles.clear();

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
        clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);
}
