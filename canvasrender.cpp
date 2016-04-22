#include "canvasrender.h"
#include "canvascontext.h"
#include "canvastile.h"
#include "glhelper.h"
#include <qmath.h>
#include <QColor>
#include <QRegion>
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
    backgroundGLTile(0),
    dirtyBackground(true),
    viewScale(0.0f),
    viewPixelRatio(1)
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
    for (auto &iter: glTiles)
    {
        auto &ref = iter.second;

        if (ref.clBuf)
        {
            cl_int err = clReleaseMemObject(ref.clBuf);
            check_cl_error(err);
        }

        if (ref.glBuf)
            glFuncs->glDeleteBuffers(1, &ref.glBuf);

        dirtyTiles.insert(iter.first);
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

    dirtyBackground = true;
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

static void insertRectTiles(TileSet &tileSet, QRect pixelRect)
{
    int x0 = tile_indice(pixelRect.left(), TILE_PIXEL_WIDTH);
    int x1 = tile_indice(pixelRect.right(), TILE_PIXEL_WIDTH);
    int y0 = tile_indice(pixelRect.top(), TILE_PIXEL_HEIGHT);
    int y1 = tile_indice(pixelRect.bottom(), TILE_PIXEL_HEIGHT);

    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            tileSet.insert({x, y});
}

void CanvasRender::renderTileMap(TileMap &tiles)
{
    if (tiles.empty())
        return;

    ensureTiles(tiles);

    for (auto &iter: tiles)
    {
        renderTile(iter.first.x(), iter.first.y(), iter.second.get());
        dirtyTiles.insert(iter.first);
    }

    tiles.clear();

    if (SharedOpenCL::getSharedOpenCL()->gl_sharing)
        clFinish(SharedOpenCL::getSharedOpenCL()->cmdQueue);
}

void CanvasRender::renderView(QPoint newOrigin, QSize newSize, float newScale, bool fullRedraw)
{
    if (viewScale != newScale)
        fullRedraw = true;
    viewScale = newScale;

    if (viewSize != newSize)
    {
        //FIXME: This should also deal with the device pixel ratio for high dpi displays
        // Note: resizeFramebuffer() unbinds the framebuffer
        resizeFramebuffer(newSize.width(), newSize.height());
        fullRedraw = true;
    }
    else if (viewOrigin != newOrigin && !fullRedraw)
    {
        // Note: shiftFramebuffer() unbinds the framebuffer
        shiftFramebuffer(newOrigin.x() - viewOrigin.x(),
                         newOrigin.y() - viewOrigin.y());

        QRegion dirtyRegion = QRegion{QRect{newOrigin, viewSize}};
        dirtyRegion -= QRect{viewOrigin, viewSize};
        for (QRect &dirtyRect: dirtyRegion.rects())
        {
            int pad = viewScale > 1.0f ? std::ceil(viewScale) : 0;
            int x = std::floor(dirtyRect.x() / viewScale);
            int y = std::floor(dirtyRect.y() / viewScale);
            int w = std::ceil((dirtyRect.width() + pad) / viewScale);
            int h = std::ceil((dirtyRect.height() + pad) / viewScale);

            insertRectTiles(dirtyTiles, {x, y, w, h});
        }
    }

    viewSize = newSize;
    viewOrigin = newOrigin;

    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, backbufferFramebuffer);
    glFuncs->glViewport(0, 0, viewSize.width(), viewSize.height());

    int widgetWidth  = viewSize.width();
    int widgetHeight = viewSize.height();

    int originTileX = tile_indice(viewOrigin.x() / viewScale, TILE_PIXEL_WIDTH);
    int originTileY = tile_indice(viewOrigin.y() / viewScale, TILE_PIXEL_HEIGHT);

    float worldTileWidth = (2.0f / widgetWidth) * viewScale * TILE_PIXEL_WIDTH;
    float worldOriginX = -viewOrigin.x() * (2.0f / widgetWidth) - 1.0f;

    float worldTileHeight = (2.0f / widgetHeight) * viewScale * TILE_PIXEL_HEIGHT;
    float worldOriginY = viewOrigin.y() * (2.0f / widgetHeight) - worldTileHeight + 1.0f;

    int tileCountX = ceil(widgetWidth / (TILE_PIXEL_WIDTH * viewScale)) + 1;
    int tileCountY = ceil(widgetHeight / (TILE_PIXEL_HEIGHT * viewScale)) + 1;

    int zoomFactor = viewScale >= 1.0f ? 1 : 1 / viewScale;

    glFuncs->glUseProgram(tileShader.program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glUniform1i(tileShader.tileImage, 0);
    glFuncs->glUniform2f(tileShader.tileSize, worldTileWidth, worldTileHeight);
    glFuncs->glUniform2f(tileShader.tilePixels, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
    glFuncs->glUniform1i(tileShader.binSize, zoomFactor);
    glFuncs->glBindVertexArray(tileShader.vertexArray);

    auto drawOneTile = [&](int ix, int iy) {
        float offsetX = ix * worldTileWidth + worldOriginX;
        float offsetY = worldOriginY - iy * worldTileHeight;

        GLuint tileBuffer = getGLBuf(ix, iy);
        glFuncs->glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, tileBuffer);
        glFuncs->glUniform2f(tileShader.tileOrigin, offsetX, offsetY);
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    };

    if (fullRedraw || dirtyBackground)
    {
        for (int ix = originTileX; ix < tileCountX + originTileX; ++ix)
            for (int iy = originTileY; iy < tileCountY + originTileY; ++iy)
                drawOneTile(ix, iy);
    }
    else
    {
        for (QPoint const &iter: dirtyTiles)
            drawOneTile(iter.x(), iter.y());
    }

    glFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GL_NONE);
     /* FIXME: This should track the viewport size to make sure the scaled ratio is set
      * for drawToolCursor() and drawColorDots() as well. */
    if (viewPixelRatio != 1)
        glFuncs->glViewport(0, 0, viewSize.width() * viewPixelRatio, viewSize.height() * viewPixelRatio);
    glFuncs->glBlitFramebuffer(0, 0, viewSize.width(), viewSize.height(),
                               0, 0, viewSize.width() * viewPixelRatio, viewSize.height() * viewPixelRatio,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);

    dirtyBackground = false;
    dirtyTiles.clear();
}

void CanvasRender::drawToolCursor(QPoint cursorPos, float cursorRadius)
{
    if (cursorRadius < 1.0f)
        return;

    float widgetHeight = viewSize.height();
    float widgetWidth  = viewSize.width();

    glFuncs->glEnable(GL_BLEND);
    glFuncs->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glFuncs->glUseProgram(cursorShader.program);
    glFuncs->glUniform4f(cursorShader.dimensions,
                         (float(cursorPos.x()) / widgetWidth * 2.0f) - 1.0f,
                         1.0f - (float(cursorPos.y()) / widgetHeight * 2.0f),
                         cursorRadius / widgetWidth, cursorRadius / widgetHeight);
    glFuncs->glUniform1f(cursorShader.pixelRadius, cursorRadius / 2.0f);
    glFuncs->glBindVertexArray(cursorShader.vertexArray);
    glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glFuncs->glDisable(GL_BLEND);
}

void CanvasRender::drawColorDots(QColor dotPreviewColor)
{
    if (!dotPreviewColor.isValid())
        return;

    float dotRadius = 10.5f;

    glFuncs->glEnable(GL_BLEND);
    glFuncs->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glFuncs->glUseProgram(colorDotShader.program);
    glFuncs->glUniform4f(colorDotShader.previewColor,
                         dotPreviewColor.redF(),
                         dotPreviewColor.greenF(),
                         dotPreviewColor.blueF(),
                         dotPreviewColor.alphaF());
    glFuncs->glUniform1f(colorDotShader.pixelRadius, dotRadius);
    glFuncs->glBindVertexArray(colorDotShader.vertexArray);

    float dotHeight = dotRadius * 2.0f / viewSize.height();
    float dotWidth  = dotRadius * 2.0f / viewSize.width();

    float xShift = dotWidth * 6.0f;
    float yShift = dotHeight * 6.0f;

    glFuncs->glUniform4f(colorDotShader.dimensions,
                         0.0f, 0.0f, dotWidth, dotHeight);
    glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glFuncs->glUniform4f(colorDotShader.dimensions,
                         -xShift, 0.0f, dotWidth, dotHeight);
    glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glFuncs->glUniform4f(colorDotShader.dimensions,
                         xShift, 0.0f, dotWidth, dotHeight);
    glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glFuncs->glUniform4f(colorDotShader.dimensions,
                         0.0f, -yShift, dotWidth, dotHeight);
    glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glFuncs->glUniform4f(colorDotShader.dimensions,
                         0.0f, yShift, dotWidth, dotHeight);
    glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glFuncs->glDisable(GL_BLEND);
}
