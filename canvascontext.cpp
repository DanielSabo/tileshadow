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

CanvasContext::~CanvasContext()
{
    std::map<uint64_t, GLuint>::iterator iter;

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

CanvasTile *CanvasContext::getTile(int x, int y)
{
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;

    std::map<uint64_t, CanvasTile *>::iterator found = tiles.find(key);

    if (found != tiles.end())
        return found->second;

    CanvasTile *tile = new CanvasTile(x, y);

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

    tiles[key] = tile;

    return tile;
}

GLuint CanvasContext::getGLBuf(int x, int y)
{
    uint64_t key = (uint64_t)(x & 0xFFFFFFFF) | (uint64_t)(y & 0xFFFFFFFF) << 32;
    std::map<uint64_t, GLuint>::iterator found = glTiles.find(key);

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
    glTiles[key] = glTile;

    closeTile(getTile(x, y));

    // Call clFinish here because we closed a tile outside of closeTiles()
    clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);

    return glTile;
}

cl_mem CanvasContext::clOpenTile(CanvasTile *tile)
{
    tile->unmapHost();

    return tile->tileMem;
}

float *CanvasContext::openTile(CanvasTile *tile)
{
    tile->mapHost();

    return tile->tileData;
}

cl_mem CanvasContext::clOpenTileAt(int x, int y)
{
    return clOpenTile(getTile(x, y));
}

void CanvasContext::closeTile(CanvasTile *tile)
{
    uint64_t key = (uint64_t)(tile->x & 0xFFFFFFFF) | (uint64_t)(tile->y & 0xFFFFFFFF) << 32;

    GLuint tileBuffer;
    std::map<uint64_t, GLuint>::iterator found = glTiles.find(key);

    if (found != glTiles.end())
        tileBuffer = found->second;
    else
        return;

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
    {
        /* This function assumes cl_khr_gl_event is present; which allows
         * the Acquire and Release calls to be used without explicitly syncing.
         */
        tile->unmapHost();

        cl_int err = CL_SUCCESS;
        cl_command_queue cmdQueue = SharedOpenCL::getSharedOpenCL()->cmdQueue;
        cl_context context = SharedOpenCL::getSharedOpenCL()->ctx;

        cl_mem output = clCreateFromGLBuffer(context,
                                             CL_MEM_WRITE_ONLY,
                                             tileBuffer,
                                             &err);

        err = clEnqueueAcquireGLObjects(cmdQueue, 1, &output, 0, NULL, NULL);

        err = clEnqueueCopyBuffer(cmdQueue, tile->tileMem, output,
                                  0, 0, sizeof(float) * TILE_COMP_TOTAL,
                                  0, NULL, NULL);

        err = clEnqueueReleaseGLObjects(cmdQueue, 1, &output, 0, NULL, NULL);

        err = clReleaseMemObject(output);
    }
    else
    {
        tile->mapHost();

        /* Push the new data to OpenGL */
        glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, tileBuffer);
        glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                              sizeof(float) * TILE_COMP_TOTAL,
                              tile->tileData,
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
        CanvasTile *tile = getTile(dirtyIter->x(), dirtyIter->y());
        closeTile(tile);
    }

    dirtyTiles.clear();


    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
        clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);
}
