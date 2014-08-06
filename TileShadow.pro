#-------------------------------------------------
#
# Project created by QtCreator 2014-01-13T00:07:55
#
#-------------------------------------------------
mac {
cache()
}

QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TileShadow
TEMPLATE = app

# For unique_ptr
#QMAKE_CXXFLAGS += -std=gnu++0x
QMAKE_CFLAGS += -std=gnu99
QMAKE_CXXFLAGS += -O2
QMAKE_CFLAGS += -O2

CONFIG -= warn_on
CONFIG += c++11

SOURCES += main.cpp\
        mainwindow.cpp \
    systeminfodialog.cpp \
    canvaswidget.cpp \
    canvaswidget-opencl.cpp \
    glhelper.cpp \
    mypaintstrokecontext.cpp \
    canvascontext.cpp \
    basicstrokecontext.cpp \
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
    toollistwidget.cpp \
    layerlistwidget.cpp \
    toolsettingswidget.cpp \
    toollistpopup.cpp \
    imageexport.cpp \
    toolsettinginfo.cpp \
    canvasrender.cpp \
    canvaseventthread.cpp

HEADERS  += mainwindow.h \
    systeminfodialog.h \
    canvaswidget.h \
    canvaswidget-opencl.h \
    glhelper.h \
    mypaintstrokecontext.h \
    canvascontext.h \
    basicstrokecontext.h \
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
    toollistwidget.h \
    layerlistwidget.h \
    toolsettingswidget.h \
    blendmodes.h \
    toollistpopup.h \
    imageexport.h \
    strokecontext.h \
    toolsettinginfo.h \
    canvasrender.h \
    canvaseventthread.h

FORMS    += mainwindow.ui \
    systeminfodialog.ui \
    benchmarkdialog.ui \
    layerlistwidget.ui

win32 {
    INCLUDEPATH += "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\include"
    LIBS += -L"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\lib\Win32" -lOpenCL
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
    CursorCircle.frag \
    CursorCircle.vert

RESOURCES += \
    Resources.qrc

include(brushlib/brushlib.pri)
include(lodepng/lodepng.pri)
include(zlib/zlib.pri)
include(qzip/qzip.pri)
