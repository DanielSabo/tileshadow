#include "canvascontext.h"
#include "canvastile.h"

#include <QColor>
#include <QDebug>

#ifdef __APPLE__
#include <OpenCL/cl_gl.h>
#else
#include <CL/cl_gl.h>
#endif

void StrokeContext::multiplySize(float mult) {}
void StrokeContext::setColor(const QColor &color) {}

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

GLuint CanvasContext::getGLBuf(int x, int y)
{
    GLTileMap::iterator found = glTiles.find(QPoint(x, y));

    if (found != glTiles.end())
        return found->second;

    // Generate the tile
    GLuint glTile = 0;
    glFuncs->glGenBuffers(1, &glTile);
    glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, glTile);
    glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                          sizeof(float) * TILE_COMP_TOTAL,
                          NULL,
                          GL_DYNAMIC_DRAW);
    glTiles[QPoint(x, y)] = glTile;

    closeTileAt(x, y);

    // Call clFinish here because we closed a tile outside of closeTiles()
    clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);

    return glTile;
}

void CanvasContext::closeTileAt(int x, int y)
{
    GLuint tileBuffer = getGLBuf(x, y);

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

        err = clEnqueueCopyBuffer(cmdQueue, layers.clOpenTileAt(x, y), output,
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
                              layers.openTileAt(x, y),
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
