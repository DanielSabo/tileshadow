#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QGLWidget>

class CanvasWidget : public QGLWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = 0);
    virtual  ~CanvasWidget();

protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

private:
    class CanvasContext;
    CanvasContext *ctx;

signals:

public slots:

};

#endif // CANVASWIDGET_H
