#include "canvascontext.h"

#include <QDebug>

#ifdef __APPLE__
#include <OpenCL/cl_gl.h>
#else
#include <CL/cl_gl.h>
#endif

bool _TilePointCompare::operator ()(const QPoint &a, const QPoint &b)
{
    if (a.y() < b.y())
        return true;
    else if (a.y() > b.y())
        return false;
    else if (a.x() < b.x())
        return true;
    return false;
}

CanvasTile::CanvasTile(int x, int y) : x(x), y(y)
{
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
        cl_int err = clEnqueueUnmapMemObject(SharedOpenCL::getSharedOpenCL()->cmdQueue, tileMem, tileData, 0, NULL, NULL);
        tileData = NULL;
    }
}

void CanvasTile::fill(float r, float g, float b, float a)
{
    unmapHost();

    cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->fillKernel;
    const size_t global_work_size[1] = {TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT};

    float color[4] = {r, g, b, a};

    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&tileMem);
    clSetKernelArg(kernel, 1, sizeof(cl_float4), (void *)&color);
    clEnqueueNDRangeKernel(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                           kernel, 1,
                           NULL, global_work_size, NULL,
                           0, NULL, NULL);
}

CanvasTile *CanvasTile::copy()
{
    CanvasTile *result = new CanvasTile(x, y);

    unmapHost();
    result->unmapHost();

    clEnqueueCopyBuffer(SharedOpenCL::getSharedOpenCL()->cmdQueue,
                        tileMem, result->tileMem, 0, 0,
                        TILE_COMP_TOTAL * sizeof(float),
                        0, NULL, NULL);

    return result;
}

CanvasContext::~CanvasContext()
{
    GLTileMap::iterator iter;

    for (iter = glTiles.begin(); iter != glTiles.end(); ++iter)
    {
        GLuint tileBuffer = iter->second;

        if (tileBuffer)
            glFuncs->glDeleteBuffers(1, &tileBuffer);
    }

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
    GLuint tileBuffer;
    GLTileMap::iterator found = glTiles.find(QPoint(x, y));

    if (found != glTiles.end())
        tileBuffer = found->second;
    else
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

void CanvasContext::closeTiles(void)
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
