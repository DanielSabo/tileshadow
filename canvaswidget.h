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
#include "layershuffletype.h"
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
        EditFrame,
        RotateLayer,
        ScaleLayer
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

    struct ViewStateInfo
    {
        ViewStateInfo() {}
        ViewStateInfo(float s, float a, bool h, bool v) : scale(s), angle(a), mirrorHorizontal(h), mirrorVertical(v) {}
        ViewStateInfo(ViewStateInfo const &other) = default;
        ViewStateInfo & operator =(ViewStateInfo const &other) = default;

        float scale = 1.0f;
        float angle = 0.0f;
        bool mirrorHorizontal = false;
        bool mirrorVertical = false;

        bool operator ==(ViewStateInfo const &o) {
            return (scale == o.scale) && (angle == o.angle) &&
                   (mirrorHorizontal == o.mirrorHorizontal) && (mirrorVertical == o.mirrorVertical);
        }
        bool operator !=(ViewStateInfo const &o) {
            return !(operator ==(o));
        }
    };

    void setToolColor(const QColor &color);
    QColor getToolColor();
    void setActiveTool(const QString &toolName);
    QString getActiveTool();
    QList<ToolSettingInfo> getToolSettings();
    QList<ToolSettingInfo> getAdvancedToolSettings();
    void setToolSetting(const QString &settingName, const QVariant &value, bool quiet = false);
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
    ViewStateInfo getViewTransform();
    void setViewTransform(ViewStateInfo vt);

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
    void shuffleLayer(int layerIndex, int targetIndex, LayerShuffle::Type op);
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
    void rotateMode();
    void scaleMode();

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

    std::unique_ptr<CanvasRender> render;
    std::unique_ptr<CanvasContext> context;

    CanvasAction::Action action;
    QColor toolColor;
    int lastNewLayerNumber;
    bool modified;
    QPoint canvasOrigin;
    QPoint actionOrigin;
    Qt::MouseButton actionButton;

    QPoint mapToCanvas(QPoint p) ;
    QPointF mapToCanvas(QPointF p);
    QPoint mapFromCanvas(QPoint p) ;
    QPointF mapFromCanvas(QPointF p);

    void updateModifiers(QInputEvent *event);
    void updateCursor();

    void startLine();
    void lineTo(QPointF start, QPointF end);
    void endLine();

    void toggleTransformMode(CanvasAction::Action const mode);
    void commitTransformMode();

    void quickmaskApply(bool copyTo, bool eraseFrom);

    void pickColorAt(QPoint pos, bool merged = false);
    void updateLayerTranslate(int x, int y);
    void translateCurrentLayer(int x, int y);
    void updateLayerTransform(QMatrix matrix);
    void transformCurrentLayer(QMatrix const &matrix);

    void insertLayerAbove(int layerIndex, std::unique_ptr<CanvasLayer> newLayer);
    void resetCurrentLayer(CanvasContext *ctx, int index);

    void acceptCanvasAction();
    void cancelCanvasAction();

    CanvasContext *getContext();
    CanvasContext *getContextMaybe();

    QCursor colorPickCursor;
    QCursor moveViewCursor;
    QCursor moveLayerCursor;
    QCursor transformMoveOrigin;
    QCursor transformRotateCursor;
    QCursor transformScaleAllCursor;
    QCursor transformScaleXCursor;
    QCursor transformScaleYCursor;

signals:
    void updateStats();
    void updateLayers();
    void updateTool(bool pathChangeOrReset = false);
    void canvasModified();

public slots:
    void undo();
    void redo();

private slots:
    void endLayerFlash();
};

#endif // CANVASWIDGET_H
