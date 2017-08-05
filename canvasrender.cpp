#include "canvasrender.h"
#include "canvascontext.h"
#include "canvastile.h"
#include "glhelper.h"
#include <qmath.h>
#include <QColor>
#include <QRegion>
#include <QMatrix>
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

static void dynamicVertexData2f(QOpenGLFunctions_3_2_Core *glFuncs,
                               GLShaderProgram &shader,
                               std::vector<float> const &data)
{
    if (!shader.vertexBuffer && !shader.vertexArray)
    {
        glFuncs->glGenBuffers(1, &shader.vertexBuffer);
        glFuncs->glGenVertexArrays(1, &shader.vertexArray);
    }
    glFuncs->glBindVertexArray(shader.vertexArray);
    glFuncs->glBindBuffer(GL_ARRAY_BUFFER, shader.vertexBuffer);

    glFuncs->glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);

    glFuncs->glEnableVertexAttribArray(0);
    glFuncs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
}

CanvasRender::CanvasRender() :
    glFuncs(new QOpenGLFunctions_3_2_Core())
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
        tileShader.tileMatrix = glFuncs->glGetUniformLocation(tileShader.program, "tileMatrix");
        tileShader.tileImage  = glFuncs->glGetUniformLocation(tileShader.program, "tileImage");
        tileShader.tilePixels = glFuncs->glGetUniformLocation(tileShader.program, "tilePixels");
        tileShader.binSize    = glFuncs->glGetUniformLocation(tileShader.program, "binSize");
    }

    std::vector<float> shaderVerts({
        0.0f, 0.0f,
        TILE_PIXEL_WIDTH, 0.0f,
        TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT,
        0.0f, TILE_PIXEL_HEIGHT,
    });
    uploadVertexData2f(glFuncs, tileShader, shaderVerts);

    /* Set up cursor data & shaders */
    buildProgram(glFuncs, cursorShader, ":/CursorCircle.vert", ":/CursorCircle.frag");

    if (cursorShader.program)
    {
        cursorShader.dimensions = glFuncs->glGetUniformLocation(cursorShader.program, "dimensions");
        cursorShader.pixelRadius = glFuncs->glGetUniformLocation(cursorShader.program, "pixelRadius");
        cursorShader.innerColor = glFuncs->glGetUniformLocation(cursorShader.program, "innerColor");
        cursorShader.outerColor = glFuncs->glGetUniformLocation(cursorShader.program, "outerColor");
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

    /* Frame fill shaders */
    buildProgram(glFuncs, canvasFrameShader, ":/ColorFill.vert", ":/ColorFill.frag");

    if (canvasFrameShader.program)
    {
        canvasFrameShader.fillColor = glFuncs->glGetUniformLocation(canvasFrameShader.program, "fillColor");
    }

    /* Framebuffers */
    glFuncs->glGenFramebuffers(1, &backbufferFramebuffer);
    glFuncs->glGenRenderbuffers(1, &backbufferRenderbuffer);
    glFuncs->glGenRenderbuffers(1, &backbufferStencilbuffer);
}

CanvasRender::~CanvasRender()
{
    clearTiles();

    tileShader.cleanup(glFuncs);
    cursorShader.cleanup(glFuncs);
    colorDotShader.cleanup(glFuncs);
    canvasFrameShader.cleanup(glFuncs);

    if (backgroundGLTile)
        glFuncs->glDeleteBuffers(1, &backgroundGLTile);

    if (backbufferFramebuffer)
        glFuncs->glDeleteFramebuffers(1, &backbufferFramebuffer);

    if (backbufferRenderbuffer)
        glFuncs->glDeleteRenderbuffers(1, &backbufferRenderbuffer);

    if (backbufferStencilbuffer)
        glFuncs->glDeleteRenderbuffers(1, &backbufferStencilbuffer);

    delete glFuncs;
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
        glFuncs->glBindRenderbuffer(GL_RENDERBUFFER, backbufferStencilbuffer);
        glFuncs->glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, viewSize.width(), viewSize.height());

        glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, backbufferFramebuffer);
        glFuncs->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, backbufferRenderbuffer);
        glFuncs->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, backbufferStencilbuffer);

        // If creating the framebuffer with STENCIL_INDEX8 failed fall back to DEPTH24_STENCIL8
        if (glFuncs->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            glFuncs->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewSize.width(), viewSize.height());

        glFuncs->glClearColor(1.0, 0.0, 0.0, 1.0);
        glFuncs->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glFuncs->glBindRenderbuffer(GL_RENDERBUFFER, GL_NONE);
        glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    }
}

void CanvasRender::shiftFramebuffer(int xOffset, int yOffset)
{
    GLuint newFramebuffer = 0;
    GLuint newRenderbuffer = 0;
    QSize shiftArea = viewSize;
    // Y offset is flipped because GL's origin is lower right vs top right for Qt
    yOffset = -yOffset;
    shiftArea.rwidth() -= abs(xOffset);
    shiftArea.rheight() -= abs(yOffset);
    int readX = xOffset > 0 ? xOffset : 0;
    int writeX = xOffset > 0 ? 0 : -xOffset;
    int readY = yOffset > 0 ? yOffset : 0;
    int writeY = yOffset > 0 ? 0 : -yOffset;

    glFuncs->glGenFramebuffers(1, &newFramebuffer);
    glFuncs->glGenRenderbuffers(1, &newRenderbuffer);

    glFuncs->glBindRenderbuffer(GL_RENDERBUFFER, newRenderbuffer);
    glFuncs->glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, viewSize.width(), viewSize.height());
    glFuncs->glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glFuncs->glBindFramebuffer(GL_READ_FRAMEBUFFER, backbufferFramebuffer);
    glFuncs->glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    glFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, newFramebuffer);
    glFuncs->glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, newRenderbuffer);

    glFuncs->glBlitFramebuffer(readX, readY, shiftArea.width() + readX, shiftArea.height() + readY,
                               writeX, writeY, shiftArea.width() + writeX, shiftArea.height() + writeY,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glFuncs->glDeleteRenderbuffers(1, &backbufferRenderbuffer);
    glFuncs->glDeleteFramebuffers(1, &backbufferFramebuffer);
    glFuncs->glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, backbufferStencilbuffer);
    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);

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

namespace {
void insertRectTiles(TileSet &tileSet, QRect const &pixelRect)
{
    QRect tileBounds = boundingTiles(pixelRect);

    for (int y = tileBounds.top(); y <= tileBounds.bottom(); ++y)
        for (int x = tileBounds.left(); x <= tileBounds.right(); ++x)
            tileSet.insert({x, y});
}
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

void CanvasRender::renderView(QPoint newOrigin, QSize newSize, float newScale, float newAngle, bool newMirrorHoriz, bool newMirrorVert, QRect newFrame, bool fullRedraw)
{
    fullRedraw |= (viewScale != newScale);
    fullRedraw |= (viewAngle != newAngle);
    fullRedraw |= (mirrorHoriz != newMirrorHoriz);
    fullRedraw |= (mirrorVert != newMirrorVert);
    fullRedraw |= (viewFrame != newFrame);

    viewScale = newScale;
    viewAngle = newAngle;
    mirrorHoriz = newMirrorHoriz;
    mirrorVert = newMirrorVert;
    viewFrame = newFrame;

    QVector<QRect> dirtyRects;

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

        QRegion dirtyRegion = QRegion{QRect{{0,0}, viewSize}};
        dirtyRegion -= QRect{viewOrigin - newOrigin, viewSize};
        dirtyRects = dirtyRegion.rects();
    }

    viewSize = newSize;
    viewOrigin = newOrigin;

    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, backbufferFramebuffer);
    glFuncs->glViewport(0, 0, viewSize.width(), viewSize.height());

    QMatrix widgetToCanvas;
    widgetToCanvas.scale(1.0 / viewScale, 1.0 / viewScale);
    widgetToCanvas.rotate(-viewAngle);
    widgetToCanvas.scale(mirrorHoriz ? -1.0 : 1.0, mirrorVert ? -1.0 : 1.0);
    widgetToCanvas.translate(viewOrigin.x(), viewOrigin.y());
    QMatrix viewportToCanvas = widgetToCanvas;
    viewportToCanvas.scale(viewSize.width(), viewSize.height());
    viewportToCanvas.translate(0.5, 0.5);
    viewportToCanvas.scale(0.5, -0.5);
    QMatrix canvasToViewport = viewportToCanvas.inverted();

    int zoomFactor = viewScale >= 1.0f ? 1 : 1 / viewScale;

    glFuncs->glUseProgram(tileShader.program);

    glFuncs->glActiveTexture(GL_TEXTURE0);
    glFuncs->glUniform1i(tileShader.tileImage, 0);
    // vertex.xx * tileMatrix.xy + vertex.yy * tileMatrix.zw
    // x = vertex.x * tileMatrix.x + vertex.y * tileMatrix.z
    // y = vertex.x * tileMatrix.y + vertex.y * tileMatrix.w
    glFuncs->glUniform4f(tileShader.tileMatrix, canvasToViewport.m11(), canvasToViewport.m12(),
                                                canvasToViewport.m21(), canvasToViewport.m22());
    glFuncs->glUniform2f(tileShader.tilePixels, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);
    glFuncs->glUniform1i(tileShader.binSize, zoomFactor);
    glFuncs->glBindVertexArray(tileShader.vertexArray);

    auto drawOneTile = [&](int ix, int iy) {
        QPointF offset = canvasToViewport.map(QPointF(ix * TILE_PIXEL_WIDTH, iy * TILE_PIXEL_HEIGHT));

        GLuint tileBuffer = getGLBuf(ix, iy);
        glFuncs->glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, tileBuffer);
        glFuncs->glUniform2f(tileShader.tileOrigin, offset.x(), offset.y());
        glFuncs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    };

    bool stencilTiles = !(viewFrame.isEmpty() || fullRedraw);

    if (stencilTiles)
    {
        glFuncs->glEnable(GL_STENCIL_TEST);
        glFuncs->glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glFuncs->glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glFuncs->glStencilMask(0xFF);
        glFuncs->glClearStencil(0);
        glFuncs->glClear(GL_STENCIL_BUFFER_BIT);
    }

    if (fullRedraw || dirtyBackground)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        QRect tileBounds = boundingTiles(viewportToCanvas.mapRect(QRectF(-1.0, -1.0, 2.0, 2.0)).toAlignedRect());

        for (int ix = tileBounds.left(); ix <= tileBounds.right(); ++ix)
            for (int iy = tileBounds.top(); iy <= tileBounds.bottom(); ++iy)
                drawOneTile(ix, iy);
    }
    else
    {
        for (QRectF rect: dirtyRects)
            insertRectTiles(dirtyTiles, widgetToCanvas.mapRect(rect).toAlignedRect());

        for (QPoint const &iter: dirtyTiles)
            drawOneTile(iter.x(), iter.y());
    }

    if (stencilTiles)
    {
        glFuncs->glStencilFunc(GL_EQUAL, 1, 0xFF);
        glFuncs->glStencilMask(0x00);
    }

    drawFrame(viewFrame, canvasToViewport);

    if (stencilTiles)
    {
        glFuncs->glDisable(GL_STENCIL_TEST);
        glFuncs->glClear(GL_STENCIL_BUFFER_BIT);
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

void CanvasRender::drawToolCursor(QPoint cursorPos, float cursorRadius, QColor outerColor, QColor innerColor)
{
    if (cursorRadius < 1.0f)
        return;

    float widgetHeight = viewSize.height();
    float widgetWidth  = viewSize.width();

    glFuncs->glEnable(GL_BLEND);
    glFuncs->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glFuncs->glUseProgram(cursorShader.program);
    glFuncs->glUniform4f(cursorShader.innerColor,
                         innerColor.redF(),
                         innerColor.greenF(),
                         innerColor.blueF(),
                         innerColor.alphaF());
    glFuncs->glUniform4f(cursorShader.outerColor,
                         outerColor.redF(),
                         outerColor.greenF(),
                         outerColor.blueF(),
                         outerColor.alphaF());
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

namespace {
    void appendRectVerts(std::vector<float> &shaderVerts, QRectF const &rect)
    {
        shaderVerts.push_back(rect.x()); shaderVerts.push_back(rect.y());
        shaderVerts.push_back(rect.x() + rect.width()); shaderVerts.push_back(rect.y());
        shaderVerts.push_back(rect.x() + rect.width()); shaderVerts.push_back(rect.y() + rect.height());

        shaderVerts.push_back(rect.x()); shaderVerts.push_back(rect.y());
        shaderVerts.push_back(rect.x() + rect.width()); shaderVerts.push_back(rect.y() + rect.height());
        shaderVerts.push_back(rect.x()); shaderVerts.push_back(rect.y() + rect.height());
    }
}

void CanvasRender::drawFrame(QRectF frame, QMatrix const &canvasToViewport)
{
    if (frame.isEmpty())
        return;

    QColor fillColor(Qt::black);
    fillColor.setAlphaF(0.35);

    frame = canvasToViewport.mapRect(frame);

    std::vector<float> shaderVerts;

    if (frame.right() < 1.0f) // Right
        appendRectVerts(shaderVerts, {frame.topRight(), QPointF{1.0f, 1.0f}});
    if (frame.bottom() < 1.0f) // Top
        appendRectVerts(shaderVerts, {frame.bottomRight(), QPointF{-1.0f, 1.0f}});
    if (frame.left() > -1.0f) // Left
        appendRectVerts(shaderVerts, {frame.bottomLeft(), QPointF{-1.0f, -1.0f}});
    if (frame.top() > -1.0f) // Bottom
        appendRectVerts(shaderVerts, {frame.topLeft(), QPointF{1.0f, -1.0f}});

    if(!shaderVerts.empty())
    {
        float extraX = (2.0f * 2.0f) / viewSize.width();
        float extraY = (2.0f * 2.0f) / viewSize.height();
        appendRectVerts(shaderVerts, {frame.topRight(), QSizeF(extraX, frame.height() + extraY)}); // Right
        appendRectVerts(shaderVerts, {frame.bottomRight(), QSizeF(-(frame.width() + extraX), extraY)}); // Top
        appendRectVerts(shaderVerts, {frame.bottomLeft(), QSizeF(-extraX, -(frame.height() + extraY))}); // Left
        appendRectVerts(shaderVerts, {frame.topLeft(), QSizeF(frame.width() + extraX, -extraY)}); // Bottom

        glFuncs->glEnable(GL_BLEND);
        glFuncs->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glFuncs->glUseProgram(canvasFrameShader.program);
        glFuncs->glUniform4f(canvasFrameShader.fillColor,
                             fillColor.redF(),
                             fillColor.greenF(),
                             fillColor.blueF(),
                             fillColor.alphaF());
        // Calling dynamicVertexData2f binds the vertex array
        //glFuncs->glBindVertexArray(canvasFrameShader.vertexArray);
        dynamicVertexData2f(glFuncs, canvasFrameShader, shaderVerts);
        glFuncs->glDrawArrays(GL_TRIANGLES, 0, shaderVerts.size() / 2);
        glFuncs->glDisable(GL_BLEND);
    }
}
