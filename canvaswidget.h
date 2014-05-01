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

    void setToolSizeFactor(float multipler);
    void setActiveTool(const QString &toolName);
    void startStroke(QPointF pos, float pressure);
    void strokeTo(QPointF pos, float pressure);
    void endStroke();


protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    void mousePressEvent(QMouseEvent * event);
    void mouseReleaseEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void tabletEvent(QTabletEvent *event);

private:
    CanvasContext *ctx;

    QString activeBrush;
    float toolSizeFactor;
signals:

public slots:

};

#endif // CANVASWIDGET_H
