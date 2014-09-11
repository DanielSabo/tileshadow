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
                          sizeof(float) * TILE_COMP_TOTAL,
                          ctx->layers.backgroundTile->mapHost(),
                          GL_STATIC_DRAW);
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

        err = clEnqueueCopyBuffer(cmdQueue, tile->unmapHost(), output,
                                  0, 0, sizeof(float) * TILE_COMP_TOTAL,
                                  0, nullptr, nullptr);

        err = clEnqueueReleaseGLObjects(cmdQueue, 1, &output, 0, nullptr, nullptr);

        err = clReleaseMemObject(output);
    }
    else
    {
        /* Push the new data to OpenGL */
        glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, tileBuffer);
        glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                              sizeof(float) * TILE_COMP_TOTAL,
                              tile->mapHost(),
                              GL_STREAM_DRAW);
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
                                      sizeof(float) * TILE_COMP_TOTAL,
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
