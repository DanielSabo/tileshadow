#include "canvascontext.h"
#include "canvastile.h"

#include <QColor>
#include <QDebug>

#ifdef __APPLE__
#include <OpenCL/cl_gl.h>
#else
#include <CL/cl_gl.h>
#endif

CanvasContext::CanvasContext() :
    glFuncs(new QOpenGLFunctions_3_2_Core()),
    vertexShader(0),
    fragmentShader(0),
    program(0),
    vertexBuffer(0),
    currentLayer(0),
    backgroundGLTile(0)
{
    if (!glFuncs->initializeOpenGLFunctions())
    {
        qWarning() << "Could not initialize OpenGL Core Profile 3.2";
        exit(1);
    }
}

CanvasContext::~CanvasContext()
{
    clearUndoHistory();
    clearRedoHistory();
    clearTiles();

    if (vertexBuffer)
        glFuncs->glDeleteBuffers(1, &vertexBuffer);

    if (vertexArray)
        glFuncs->glDeleteVertexArrays(1, &vertexArray);

    if (vertexShader)
        glFuncs->glDeleteShader(vertexShader);

    if (fragmentShader)
        glFuncs->glDeleteShader(fragmentShader);

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

void CanvasContext::clearTiles()
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

void CanvasContext::updateBackgroundTile()
{
    if (!backgroundGLTile)
    {
        glFuncs->glGenBuffers(1, &backgroundGLTile);
    }

    glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, backgroundGLTile);
    glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                          sizeof(float) * TILE_COMP_TOTAL,
                          layers.backgroundTile->mapHost(),
                          GL_STATIC_DRAW);
}

GLuint CanvasContext::getGLBuf(int x, int y)
{
    GLTileMap::iterator found = glTiles.find(QPoint(x, y));

    if (found != glTiles.end())
    {
        if (found->second)
            return found->second;
        else
            return backgroundGLTile;
    }

    closeTileAt(x, y);

    // Call clFinish here because we closed a tile outside of closeTiles()
    clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);

    GLuint tileBuffer = glTiles[QPoint(x, y)];

    return tileBuffer ? tileBuffer : backgroundGLTile;
}

void CanvasContext::closeTileAt(int x, int y)
{
    CanvasTile *tile = layers.getTileMaybe(x, y);

    GLTileMap::iterator found = glTiles.find(QPoint(x, y));

    GLuint tileBuffer = 0;

    if (found != glTiles.end() && found->second != 0)
    {
        if (tile)
            tileBuffer = found->second;
        else
        {
            glFuncs->glDeleteBuffers(1, &found->second);
            found->second = 0;
        }
    }
    else if (tile)
    {
        glFuncs->glGenBuffers(1, &tileBuffer);
        glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, tileBuffer);
        glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                              sizeof(float) * TILE_COMP_TOTAL,
                              NULL,
                              GL_DYNAMIC_DRAW);
        glTiles[QPoint(x, y)] = tileBuffer;
    }
    else
    {
        glTiles[QPoint(x, y)] = 0;
    }

    if (!tile)
        return;

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
    {
        /* This function assumes cl_khr_gl_event is present; which allows
         * the Acquire and Release calls to be used without explicitly syncing.
         */
        cl_int err = CL_SUCCESS;
        cl_command_queue cmdQueue = SharedOpenCL::getSharedOpenCL()->cmdQueue;
        cl_context context = SharedOpenCL::getSharedOpenCL()->ctx;

        cl_mem output = clCreateFromGLBuffer(context,
                                             CL_MEM_WRITE_ONLY,
                                             tileBuffer,
                                             &err);

        err = clEnqueueAcquireGLObjects(cmdQueue, 1, &output, 0, NULL, NULL);

        err = clEnqueueCopyBuffer(cmdQueue, tile->unmapHost(), output,
                                  0, 0, sizeof(float) * TILE_COMP_TOTAL,
                                  0, NULL, NULL);

        err = clEnqueueReleaseGLObjects(cmdQueue, 1, &output, 0, NULL, NULL);

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

void CanvasContext::closeTiles()
{
    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
        glFinish();

    for (TileSet::iterator dirtyIter = dirtyTiles.begin();
         dirtyIter != dirtyTiles.end();
         dirtyIter++)
    {
        closeTileAt(dirtyIter->x(), dirtyIter->y());
    }

    dirtyTiles.clear();

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
        clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);
}

void CanvasContext::clearUndoHistory()
{
    while (!undoHistory.empty())
    {
        CanvasUndoEvent *event = undoHistory.first();
        delete event;
        undoHistory.removeFirst();
    }
}

void CanvasContext::clearRedoHistory()
{
    while (!redoHistory.empty())
    {
        CanvasUndoEvent *event = redoHistory.first();
        delete event;
        redoHistory.removeFirst();
    }
}
