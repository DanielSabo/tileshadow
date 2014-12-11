#-------------------------------------------------
#
# Project created by QtCreator 2014-01-13T00:07:55
#
#-------------------------------------------------
mac {
cache()
}

QT += core gui widgets opengl

TARGET = TileShadow
TEMPLATE = app

!win32-msvc* {
    QMAKE_CFLAGS += -std=gnu99
    QMAKE_CXXFLAGS_WARN_ON += -Wno-missing-braces -Wno-missing-field-initializers -Wno-unused-parameter
    QMAKE_CFLAGS_WARN_ON += -Wno-missing-field-initializers -Wno-unused-parameter
    CONFIG += warn_on
}
else {
    # 4100 = Unused parameter
    # 4267 = Truncation of size_t
    # 4244, 4305 = Truncation of double to float
    # 4701, 4703 = Use of uninitialized variable
    QMAKE_CXXFLAGS_WARN_ON = -W3 -wd4100 -wd4267 -wd4305 -w14701 -w14703
    QMAKE_CFLAGS_WARN_ON = -W3 -wd4244 -wd4305 -w14701 -w14703
    CONFIG += warn_on
}

QMAKE_CXXFLAGS += -O2
QMAKE_CFLAGS += -O2

CONFIG += c++11

DEFINES += CL_USE_DEPRECATED_OPENCL_1_1_APIS
# Disable lodepng's file operations, which have deprecation warnings on Win32
DEFINES += LODEPNG_NO_COMPILE_DISK

SOURCES += main.cpp\
        mainwindow.cpp \
    systeminfodialog.cpp \
    canvaswidget.cpp \
    canvaswidget-opencl.cpp \
    glhelper.cpp \
    mypaintstrokecontext.cpp \
    canvascontext.cpp \
    benchmarkdialog.cpp \
    boxcartimer.cpp \
    canvastile.cpp \
    canvaslayer.cpp \
    canvasstack.cpp \
    tileset.cpp \
    ora.cpp \
    hsvcolordial.cpp \
    canvasundoevent.cpp \
    basetool.cpp \
    mypainttool.cpp \
    tiledebugtool.cpp \
    roundbrushtool.cpp \
    toollistwidget.cpp \
    toolfactory.cpp \
    layerlistwidget.cpp \
    layerlistview.cpp \
    toolsettingswidget.cpp \
    toollistpopup.cpp \
    imagefiles.cpp \
    toolsettinginfo.cpp \
    canvasrender.cpp \
    canvaseventthread.cpp \
    paintutils.cpp \
    deviceselectdialog.cpp \
    maskbuffer.cpp \
    batchprocessor.cpp

HEADERS  += mainwindow.h \
    systeminfodialog.h \
    canvaswidget.h \
    canvaswidget-opencl.h \
    glhelper.h \
    mypaintstrokecontext.h \
    canvascontext.h \
    benchmarkdialog.h \
    boxcartimer.h \
    canvastile.h \
    canvaslayer.h \
    canvasstack.h \
    tileset.h \
    ora.h \
    hsvcolordial.h \
    canvasundoevent.h \
    basetool.h \
    mypainttool.h \
    tiledebugtool.h \
    roundbrushtool.h \
    toollistwidget.h \
    toolfactory.h \
    layerlistwidget.h \
    layerlistview.h \
    toolsettingswidget.h \
    blendmodes.h \
    toollistpopup.h \
    imagefiles.h \
    strokecontext.h \
    toolsettinginfo.h \
    canvasrender.h \
    canvaseventthread.h \
    paintutils.h \
    deviceselectdialog.h \
    maskbuffer.h \
    batchprocessor.h

FORMS    += mainwindow.ui \
    systeminfodialog.ui \
    benchmarkdialog.ui \
    layerlistwidget.ui

win32 {
    win32-msvc* {
        INCLUDEPATH += "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\include"
        LIBS += "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\lib\x64\OpenCL.lib"
        LIBS += "OpenGL32.lib"
    } else {
        INCLUDEPATH += "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\include"
        LIBS += -L"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\lib\Win32" -lOpenCL
        LIBS += -lopengl32
    }
    RC_FILE = tileshadow.rc
} mac {
    QMAKE_CXXFLAGS += -framework OpenCL -Qunused-arguments
    LIBS += -framework OpenCL
    ICON = TileShadow.icns
} unix:!mac {
    LIBS += -lOpenCL
}

OTHER_FILES += \
    CanvasShader.frag \
    CanvasShader.vert \
    BaseKernels.cl \
    MyPaintKernels.cl \
    PaintKernels.cl \
    CursorCircle.frag \
    CursorCircle.vert \
    ColorDot.frag

RESOURCES += \
    Resources.qrc

include(brushlib/brushlib.pri)
include(lodepng/lodepng.pri)
include(zlib/zlib.pri)
include(qzip/qzip.pri)
