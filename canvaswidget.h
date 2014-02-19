#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QGLWidget>

class CanvasContext;
class CanvasWidget : public QGLWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = 0);
    virtual  ~CanvasWidget();

    void setActiveTool(const QString &toolName);

protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    void mousePressEvent(QMouseEvent * event);
    void mouseReleaseEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);

private:
    CanvasContext *ctx;

    QString activeBrush;

signals:

public slots:

};

#endif // CANVASWIDGET_H
