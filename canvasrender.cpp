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

void GLShaderProgram::cleanup(QOpenGLFunctions_3_2_Core *glFuncs)
{
    if (program)
        glFuncs->glDeleteProgram(program);

    if (vertexBuffer)
        glFuncs->glDeleteBuffers(1, &vertexBuffer);

    if (vertexArray)
        glFuncs->glDeleteVertexArrays(1, &vertexArray);
}

static void buildProgram(QOpenGLFunctions_3_2_Core *glFuncs,
                         GLShaderProgram &shader,
                         QString const &vertPath,
                         QString const &fragPath)
{
    if (shader.program)
        qWarning() << "Leaking shader program";

    GLuint vertexShader = compileGLShaderFile(glFuncs, vertPath, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileGLShaderFile(glFuncs, fragPath, GL_FRAGMENT_SHADER);
    shader.program = buildGLProgram(glFuncs, vertexShader, fragmentShader);

    if (vertexShader)
        glFuncs->glDeleteShader(vertexShader);
    if (fragmentShader)
        glFuncs->glDeleteShader(fragmentShader);
}

static void uploadVertexData2f(QOpenGLFunctions_3_2_Core *glFuncs,
                               GLShaderProgram &shader,
                               std::vector<float> data)
{
    if (shader.vertexBuffer || shader.vertexArray)
        qWarning() << "Leaking shader data";

    glFuncs->glGenBuffers(1, &shader.vertexBuffer);
    glFuncs->glGenVertexArrays(1, &shader.vertexArray);
    glFuncs->glBindVertexArray(shader.vertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, shader.vertexBuffer);

    glFuncs->glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    glFuncs->glEnableVertexAttribArray(0);
    glFuncs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
}

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
    buildProgram(glFuncs, tileShader, ":/CanvasShader.vert", ":/CanvasShader.frag");

    if (tileShader.program)
    {
        tileShader.tileOrigin = glFuncs->glGetUniformLocation(tileShader.program, "tileOrigin");
        tileShader.tileSize   = glFuncs->glGetUniformLocation(tileShader.program, "tileSize");
        tileShader.tileImage  = glFuncs->glGetUniformLocation(tileShader.program, "tileImage");
        tileShader.tilePixels = glFuncs->glGetUniformLocation(tileShader.program, "tilePixels");
        tileShader.binSize    = glFuncs->glGetUniformLocation(tileShader.program, "binSize");
    }

    std::vector<float> shaderVerts({
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    });
    uploadVertexData2f(glFuncs, tileShader, shaderVerts);

    /* Set up cursor data & shaders */
    buildProgram(glFuncs, cursorShader, ":/CursorCircle.vert", ":/CursorCircle.frag");

    if (cursorShader.program)
    {
        cursorShader.dimensions = glFuncs->glGetUniformLocation(cursorShader.program, "dimensions");
        cursorShader.pixelRadius = glFuncs->glGetUniformLocation(cursorShader.program, "pixelRadius");
    }

    shaderVerts = std::vector<float>({
        -1.0f, -1.0f,
        1.0f,  -1.0f,
        1.0f,  1.0f,
        -1.0f, 1.0f,
    });
    uploadVertexData2f(glFuncs, cursorShader, shaderVerts);

    /* Color dot data & shaders */
    buildProgram(glFuncs, colorDotShader, ":/CursorCircle.vert", ":/ColorDot.frag");

    if (colorDotShader.program)
    {
        colorDotShader.dimensions = glFuncs->glGetUniformLocation(colorDotShader.program, "dimensions");
        colorDotShader.pixelRadius = glFuncs->glGetUniformLocation(colorDotShader.program, "pixelRadius");
        colorDotShader.previewColor =  glFuncs->glGetUniformLocation(colorDotShader.program, "previewColor");
    }

    shaderVerts = std::vector<float>({
        -1.0f, -1.0f,
        1.0f,  -1.0f,
        1.0f,  1.0f,
        -1.0f, 1.0f,
    });
    uploadVertexData2f(glFuncs, colorDotShader, shaderVerts);

    /* Framebuffers */
    glFuncs->glGenFramebuffers(1, &backbufferFramebuffer);
    glFuncs->glGenRenderbuffers(1, &backbufferRenderbuffer);
}

CanvasRender::~CanvasRender()
{
    clearTiles();

    tileShader.cleanup(glFuncs);
    cursorShader.cleanup(glFuncs);
    colorDotShader.cleanup(glFuncs);

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

void CanvasRender::shiftFramebuffer(int xOffset, int yOffset)
{
    GLuint newFramebuffer = 0;
    GLuint newRenderbuffer = 0;
    QSize siftArea = viewSize;
    // Y offset is flipped because GL's origin is lower right vs top right for Qt
    yOffset = -yOffset;
    siftArea.rwidth() -= abs(xOffset);
    siftArea.rheight() -= abs(yOffset);
    int readX = xOffset > 0 ? xOffset : 0;
    int writeX = xOffset > 0 ? 0 : -xOffset;
    int readY = yOffset > 0 ? yOffset : 0;
    int writeY = yOffset > 0 ? 0 : -yOffset;

    glFuncs->glGenFramebuffers(1, &newFramebuffer);
    glFuncs->glGenRenderbuffers(1, &newRenderbuffer);

    glFuncs->glBindRenderbuffer(GL_RENDERBUFFER, newFramebuffer);
    glFuncs->glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, viewSize.width(), viewSize.height());
    glFuncs->glBindFramebuffer(GL_READ_FRAMEBUFFER, backbufferFramebuffer);
    glFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, newFramebuffer);
    glFuncs->glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, newFramebuffer);

    glFuncs->glBlitFramebuffer(readX, readY, siftArea.width() + readX, siftArea.height() + readY,
                               writeX, writeY, siftArea.width() + writeX, siftArea.height() + writeY,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);

    glFuncs->glDeleteFramebuffers(1, &backbufferFramebuffer);
    glFuncs->glDeleteRenderbuffers(1, &backbufferRenderbuffer);
    backbufferFramebuffer = newFramebuffer;
    backbufferRenderbuffer = newRenderbuffer;
}

void CanvasRender::clearTiles()
{
    GLTileMap::iterator iter;
    for (iter = glTiles.begin(); iter != glTiles.end(); ++iter)
    {
        auto &ref = iter->second;

        if (ref.clBuf)
        {
            cl_int err = clReleaseMemObject(ref.clBuf);
            check_cl_error(err);
        }

        if (ref.glBuf)
            glFuncs->glDeleteBuffers(1, &ref.glBuf);
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
        if (found->second.glBuf)
            return found->second.glBuf;
        else
            return backgroundGLTile;
    }

    return backgroundGLTile;
}

void CanvasRender::renderTile(int x, int y, CanvasTile *tile)
{
    auto &ref = glTiles[QPoint(x, y)];

    if (!ref.glBuf && tile)
    {
        qDebug() << "renderTile can't render uninitialized tile" << x << y;
        return;
    }

    if (!tile)
    {
        if (ref.glBuf)
            qDebug() << "renderTile can't delete" << x << y;

        return;
    }

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
    {
        cl_int err = CL_SUCCESS;
        cl_command_queue cmdQueue = SharedOpenCL::getSharedOpenCL()->cmdQueue;

        err = clEnqueueAcquireGLObjects(cmdQueue, 1, &ref.clBuf, 0, nullptr, nullptr);

        cl_mem input = tile->unmapHost();

        cl_kernel kernel = SharedOpenCL::getSharedOpenCL()->floatToU8;

        err = clSetKernelArg<cl_mem>(kernel, 0, input);
        err = clSetKernelArg<cl_mem>(kernel, 1, ref.clBuf);

        size_t workSize = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT;

        err = clEnqueueNDRangeKernel(cmdQueue,
                                     kernel, 1,
                                     nullptr, &workSize, nullptr,
                                     0, nullptr, nullptr);

        err = clEnqueueReleaseGLObjects(cmdQueue, 1, &ref.clBuf, 0, nullptr, nullptr);
        (void)err; /* Ignore the fact that err is unused */
    }
    else
    {
        glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, ref.glBuf);
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

    cl_context context = 0;
    bool gl_sharing = SharedOpenCL::getSharedOpenCL()->gl_sharing;
    if (gl_sharing)
        context = SharedOpenCL::getSharedOpenCL()->ctx;

    for (auto &iter: tiles)
    {
        auto &ref = glTiles[iter.first];

        if (iter.second)
        {
            if (!ref.glBuf)
            {
                glFuncs->glGenBuffers(1, &ref.glBuf);
                glFuncs->glBindBuffer(GL_TEXTURE_BUFFER, ref.glBuf);
                glFuncs->glBufferData(GL_TEXTURE_BUFFER,
                                      sizeof(GLubyte) * TILE_COMP_TOTAL,
                                      nullptr,
                                      GL_DYNAMIC_DRAW);

                if (gl_sharing)
                {
                    cl_int err;
                    ref.clBuf = clCreateFromGLBuffer(context,
                                                     CL_MEM_WRITE_ONLY,
                                                     ref.glBuf,
                                                     &err);
                    check_cl_error(err);
                }
            }
        }
        else
        {
            if (ref.glBuf)
                glFuncs->glDeleteBuffers(1, &ref.glBuf);

            if (ref.clBuf)
            {
                cl_int err = clReleaseMemObject(ref.clBuf);
                check_cl_error(err);
            }

            ref.glBuf = 0;
            ref.clBuf = 0;
        }
    }

    if (gl_sharing)
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
