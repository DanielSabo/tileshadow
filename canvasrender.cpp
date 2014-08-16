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
    program(0),
    cursorProgram(0),
    vertexBuffer(0),
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
    program = buildGLProgram(glFuncs, vertexShader, fragmentShader);

    if (vertexShader)
        glFuncs->glDeleteShader(vertexShader);
    if (fragmentShader)
        glFuncs->glDeleteShader(fragmentShader);

    if (program)
    {
        locationTileOrigin = glFuncs->glGetUniformLocation(program, "tileOrigin");
        locationTileSize   = glFuncs->glGetUniformLocation(program, "tileSize");
        locationTileImage  = glFuncs->glGetUniformLocation(program, "tileImage");
        locationTilePixels = glFuncs->glGetUniformLocation(program, "tilePixels");
    }

    glFuncs->glGenBuffers(1, &vertexBuffer);
    glFuncs->glGenVertexArrays(1, &vertexArray);
    glFuncs->glBindVertexArray(vertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

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
    GLuint cursorVert = compileGLShaderFile(glFuncs, ":/CursorCircle.vert", GL_VERTEX_SHADER);
    GLuint cursorFrag = compileGLShaderFile(glFuncs, ":/CursorCircle.frag", GL_FRAGMENT_SHADER);
    cursorProgram = buildGLProgram(glFuncs, cursorVert, cursorFrag);

    if (cursorProgram)
    {
        cursorProgramDimensions = glFuncs->glGetUniformLocation(cursorProgram, "dimensions");
        cursorProgramPixelRadius = glFuncs->glGetUniformLocation(cursorProgram, "pixelRadius");
    }

    float cursorVertData[] = {
        -1.0f, -1.0f,
        1.0f,  -1.0f,
        1.0f,  1.0f,
        -1.0f, 1.0f,
    };

    glFuncs->glGenBuffers(1, &cursorVertexBuffer);
    glFuncs->glGenVertexArrays(1, &cursorVertexArray);
    glFuncs->glBindVertexArray(cursorVertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, cursorVertexBuffer);

    glFuncs->glBufferData(GL_ARRAY_BUFFER, sizeof(cursorVertData), cursorVertData, GL_STATIC_DRAW);

    glFuncs->glEnableVertexAttribArray(0);
    glFuncs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    if (cursorVert)
        glFuncs->glDeleteShader(cursorVert);
    if (cursorFrag)
        glFuncs->glDeleteShader(cursorFrag);

    glFuncs->glGenFramebuffers(1, &backbufferFramebuffer);
    glFuncs->glGenRenderbuffers(1, &backbufferRenderbuffer);
}

CanvasRender::~CanvasRender()
{
    clearTiles();

    if (vertexBuffer)
        glFuncs->glDeleteBuffers(1, &vertexBuffer);

    if (vertexArray)
        glFuncs->glDeleteVertexArrays(1, &vertexArray);

    if (program)
        glFuncs->glDeleteProgram(program);

    if (cursorProgram)
        glFuncs->glDeleteProgram(cursorProgram);

    if (cursorVertexBuffer)
        glFuncs->glDeleteBuffers(1, &cursorVertexBuffer);

    if (cursorVertexArray)
        glFuncs->glDeleteVertexArrays(1, &cursorVertexArray);

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

    for (auto iter: tiles)
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

    for (auto iter: tiles)
    {
        renderTile(iter.first.x(), iter.first.y(), iter.second);
        if (iter.second)
            delete iter.second;
    }
    tiles.clear();

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
        clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);
}
