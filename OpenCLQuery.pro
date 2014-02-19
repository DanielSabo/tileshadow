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

TARGET = OpenCLQuery
TEMPLATE = app

# For unique_ptr
#QMAKE_CXXFLAGS += -std=gnu++0x
QMAKE_CFLAGS += -std=gnu99

SOURCES += main.cpp\
        mainwindow.cpp \
    systeminfodialog.cpp \
    canvaswidget.cpp \
    canvaswidget-opencl.cpp \
    mypaintstrokecontext.cpp \
    canvascontext.cpp \
    basicstrokecontext.cpp

HEADERS  += mainwindow.h \
    systeminfodialog.h \
    canvaswidget.h \
    canvaswidget-opencl.h \
    mypaintstrokecontext.h \
    canvascontext.h \
    basicstrokecontext.h

FORMS    += mainwindow.ui \
    systeminfodialog.ui

win32 {
    INCLUDEPATH += "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\include"
    LIBS += -L"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v5.5\lib\Win32" -lOpenCL
} mac {
    QMAKE_CXXFLAGS += -framework OpenCL -Qunused-arguments
    LIBS += -framework OpenCL
} unix:!mac {
    LIBS += -lOpenCL
}

OTHER_FILES += \
    CanvasShader.frag \
    CanvasShader.vert \
    BaseKernels.cl \
    MyPaintKernels.cl

RESOURCES += \
    Resources.qrc

include(brushlib/brushlib.pri)
