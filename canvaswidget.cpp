#include "canvaswidget.h"
#include <iostream>

using namespace std;

CanvasWidget::CanvasWidget(QWidget *parent) :
    QGLWidget(parent)
{
}

void CanvasWidget::initializeGL()
{
//    cout << "GL Initialize" << endl;
    glClearColor(0.2, 0.2, 0.4, 1.0);
}

void CanvasWidget::resizeGL(int w, int h)
{
    (void)w; (void)h;
}

void CanvasWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
}
