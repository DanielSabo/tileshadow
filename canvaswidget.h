#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QGLWidget>
#include <QInputEvent>
#include <QImage>
#include <QColor>
#include "boxcartimer.h"
#include "blendmodes.h"

namespace CanvasAction
{
    typedef enum
    {
        None,
        MouseStroke,
        TabletStroke,
        ColorPick,
        MoveView
    } Action;
}

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
        BlendMode::Mode mode;
    };

    void setToolSizeFactor(float multipler);
    float getToolSizeFactor();
    void setToolColor(const QColor &color);
    QColor getToolColor();
    void setActiveTool(const QString &toolName);
    QString getActiveTool();
    void startStroke(QPointF pos, float pressure);
    void strokeTo(QPointF pos, float pressure, float dt);
    void endStroke();

    float getScale();
    void  setScale(float newScale);

    bool getModified();
    void setModified(bool state);

    int getActiveLayer();
    void setActiveLayer(int layerIndex);
    void addLayerAbove(int layerIndex);
    void removeLayer(int layerIndex);
    void duplicateLayer(int layerIndex);
    void moveLayer(int currentIndex, int targetIndex);
    void renameLayer(int layerIndex, QString name);
    void setLayerVisible(int layerIndex, bool visible);
    bool getLayerVisible(int layerIndex);
    void setLayerMode(int layerIndex, BlendMode::Mode mode);
    BlendMode::Mode getLayerMode(int layerIndex);
    QList<LayerInfo> getLayerList();

    void newDrawing();
    void openORA(QString path);
    void saveAsORA(QString path);
    QImage asImage();

    BoxcarTimer mouseEventRate;
    BoxcarTimer frameRate;

    // These are public so the parent can fake focus on the canvas
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    void mousePressEvent(QMouseEvent * event);
    void mouseReleaseEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void tabletEvent(QTabletEvent *event);
    void wheelEvent(QWheelEvent *);
    void leaveEvent(QEvent *event);
    void enterEvent(QEvent *event);

private:
    CanvasContext *ctx;

    CanvasAction::Action action;
    QString activeToolName;
    QScopedPointer<BaseTool> activeTool;
    float toolSizeFactor;
    QColor toolColor;
    float viewScale;
    bool showToolCursor;
    int lastNewLayerNumber;
    bool modified;
    QPoint canvasOrigin;
    QPoint actionOrigin;

    void updateModifiers(QInputEvent *event);

    void pickColorAt(QPoint pos);

    QCursor colorPickCursor;
    QCursor moveViewCursor;
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
