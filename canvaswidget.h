#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <memory>
#include <QGLWidget>
#include <QInputEvent>
#include <QImage>
#include <QColor>
#include "boxcartimer.h"
#include "blendmodes.h"
#include "layertype.h"
#include "toolsettinginfo.h"
#include "canvasstrokepoint.h"

namespace CanvasAction
{
    typedef enum
    {
        None,
        MouseStroke,
        TabletStroke,
        ColorPick,
        ColorPickMerged,
        MoveView,
        MoveLayer,
        DrawLine,
        EditFrame
    } Action;
}

class BaseTool;
class CanvasLayer;
class CanvasContext;
class CanvasRender;
class CanvasWidgetPrivate;
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
        bool editable;
        float opacity;
        BlendMode::Mode mode;
        LayerType::Type type;
        QList<LayerInfo> children;
        int index;
    };

    struct ToolSaveInfo
    {
        QString fileExtension;
        QString saveDirectory;
        QByteArray serialzedTool;
    };

    void setToolColor(const QColor &color);
    QColor getToolColor();
    void setActiveTool(const QString &toolName);
    QString getActiveTool();
    QList<ToolSettingInfo> getToolSettings();
    QList<ToolSettingInfo> getAdvancedToolSettings();
    void setToolSetting(const QString &settingName, const QVariant &value);
    QVariant getToolSetting(const QString &settingName);
    bool getToolSaveable();
    ToolSaveInfo serializeTool();
    void resetToolSettings();
    QImage previewTool(std::vector<CanvasStrokePoint> const &stroke);

    void startStroke(QPointF pos, float pressure);
    void strokeTo(QPointF pos, float pressure, float dt);
    void endStroke();

    float getScale();
    void  setScale(float newScale);

    bool getModified();
    void setModified(bool state);

    bool getSynchronous();
    void setSynchronous(bool synced);

    std::vector<CanvasStrokePoint> getLastStrokeData();

    int getActiveLayer();
    void setActiveLayer(int layerIndex);
    void addLayerAbove(int layerIndex);
    void addLayerAbove(int layerIndex, QImage image, QString name);
    void addGroupAbove(int layerIndex);
    void removeLayer(int layerIndex);
    void duplicateLayer(int layerIndex);
    void moveLayerUp(int layerIndex);
    void moveLayerDown(int layerIndex);
    void renameLayer(int layerIndex, QString name);
    void mergeLayerDown(int layerIndex);
    void setLayerVisible(int layerIndex, bool visible);
    bool getLayerVisible(int layerIndex);
    void setLayerEditable(int layerIndex, bool editable);
    bool getLayerEditable(int layerIndex);
    void setLayerTransientOpacity(int layerIndex, float opacity);
    void setLayerOpacity(int layerIndex, float opacity);
    float getLayerOpacity(int layerIndex);
    void setLayerMode(int layerIndex, BlendMode::Mode mode);
    BlendMode::Mode getLayerMode(int layerIndex);
    QList<LayerInfo> getLayerList();

    void toggleEditFrame();
    void toggleFrame();

    void toggleQuickmask();
    void clearQuickmask();
    void quickmaskCut();
    void quickmaskCopy();
    void quickmaskErase();

    void flashCurrentLayer();
    void showColorPreview(QColor const &color);
    void hideColorPreview();

    void setBackgroundColor(QColor const &color);
    void lineDrawMode();

    void newDrawing();
    void openORA(QString path);
    void openImage(QImage image);
    void saveAsORA(QString path);
    QImage asImage();

    BoxcarTimer mouseEventRate;
    BoxcarTimer frameRate;

    bool eventFilter(QObject *obj, QEvent *event);

protected:
    void initializeGL();
    void paintGL();

    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void tabletEvent(QTabletEvent *event);
    void wheelEvent(QWheelEvent *event);
    void leaveEvent(QEvent *event);
    void enterEvent(QEvent *event);

private:
    QScopedPointer<CanvasWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(CanvasWidget)

    CanvasRender *render;
    CanvasContext *context;

    CanvasAction::Action action;
    QColor toolColor;
    float viewScale;
    int lastNewLayerNumber;
    bool modified;
    QPoint canvasOrigin;
    QPoint actionOrigin;
    Qt::MouseButton actionButton;

    void updateModifiers(QInputEvent *event);
    void updateCursor();

    void startLine();
    void lineTo(QPointF start, QPointF end);
    void endLine();

    void quickmaskApply(bool copyTo, bool eraseFrom);

    void pickColorAt(QPoint pos, bool merged = false);
    void updateLayerTranslate(int x, int y);
    void translateCurrentLayer(int x, int y);

    void insertLayerAbove(int layerIndex, std::unique_ptr<CanvasLayer> newLayer);
    void resetCurrentLayer(CanvasContext *ctx, int index);

    void cancelCanvasAction();

    CanvasContext *getContext();
    CanvasContext *getContextMaybe();

    QCursor colorPickCursor;
    QCursor moveViewCursor;
    QCursor moveLayerCursor;

signals:
    void updateStats();
    void updateLayers();
    void updateTool();
    void canvasModified();

public slots:
    void undo();
    void redo();

private slots:
    void endLayerFlash();
};

#endif // CANVASWIDGET_H
