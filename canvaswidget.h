#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QGLWidget>
#include <QColor>
#include "boxcartimer.h"

class BaseTool;
class CanvasContext;
class CanvasUndoEvent;
class CanvasWidget : public QGLWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = 0);
    virtual  ~CanvasWidget();

    struct LayerInfo
    {
        QString name;
        bool visible;
    };

    void setToolSizeFactor(float multipler);
    float getToolSizeFactor();
    void setToolColor(const QColor &color);
    QColor getToolColor();
    void setActiveTool(const QString &toolName);
    QString getActiveTool();
    void startStroke(QPointF pos, float pressure);
    void strokeTo(QPointF pos, float pressure);
    void endStroke();

    float getScale();
    void  setScale(float newScale);

    bool getModified();
    void setModified(bool state);

    int getActiveLayer();
    void setActiveLayer(int layerIndex);
    void addLayerAbove(int layerIndex);
    void removeLayer(int layerIndex);
    void moveLayer(int currentIndex, int targetIndex);
    void renameLayer(int layerIndex, QString name);
    void setLayerVisible(int layerIndex, bool visible);
    bool getLayerVisible(int layerIndex);
    QList<LayerInfo> getLayerList();

    void newDrawing();
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
    void leaveEvent(QEvent *event);
    void enterEvent(QEvent *event);

private:
    CanvasContext *ctx;

    QString activeToolName;
    QScopedPointer<BaseTool> activeTool;
    float toolSizeFactor;
    QColor toolColor;
    float viewScale;
    bool showToolCursor;
    int lastNewLayerNumber;
    bool modified;

    void pickColorAt(QPoint pos);

signals:
    void updateStats();
    void updateLayers();
    void updateTool();
    void canvasModified();

public slots:
    void undo();
    void redo();
};

#endif // CANVASWIDGET_H
