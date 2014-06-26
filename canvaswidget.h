#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QGLWidget>
#include <QColor>
#include "boxcartimer.h"

class BaseTool;
class CanvasContext;
class CanvasWidget : public QGLWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = 0);
    virtual  ~CanvasWidget();

    void setToolSizeFactor(float multipler);
    void setToolColor(const QColor &color);
    QColor getToolColor();
    void setActiveTool(const QString &toolName);
    QString getActiveTool();
    void startStroke(QPointF pos, float pressure);
    void strokeTo(QPointF pos, float pressure);
    void endStroke();

    float getScale();
    void  setScale(float newScale);

    int getActiveLayer();
    void setActiveLayer(int layerIndex);
    void addLayerAbove(int layerIndex);
    void removeLayer(int layerIndex);
    void moveLayer(int currentIndex, int targetIndex);
    void renameLayer(int layerIndex, QString name);
    QList<QString> getLayerList();

    void openORA(QString path);
    void saveAsORA(QString path);

    BoxcarTimer mouseEventRate;
    BoxcarTimer frameRate;

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

    QString activeToolName;
    QScopedPointer<BaseTool> activeTool;
    float toolSizeFactor;
    QColor toolColor;
    float viewScale;
    int lastNewLayerNumber;

    void pickColorAt(QPoint pos);

signals:
    void updateStats();
    void updateLayers();
    void updateTool();

public slots:
    void undo();
    void redo();
};

#endif // CANVASWIDGET_H
