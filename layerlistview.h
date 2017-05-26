#ifndef LAYERLISTVIEW_H
#define LAYERLISTVIEW_H

#include <QWidget>
#include <QList>
#include <QIcon>
#include <QString>
#include <QVariant>
#include "canvaswidget.h"

class LayerListViewPrivate;
class LayerListView : public QWidget
{
    Q_OBJECT
public:
    explicit LayerListView(QWidget *parent = 0);
    ~LayerListView();

    QSize sizeHint() const;
    int rowHeight();

    void setData(const QList<CanvasWidget::LayerInfo> &data, int selection);
    int getSelectedRow();

    enum {
        VisibleColumn,
        EditableColumn,
        NameColumn
    };

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    bool eventFilter(QObject *watched, QEvent *event);

    QIcon visibleIcon;
    QIcon lockedIcon;

    void recalulateSize();

private:
    QScopedPointer<LayerListViewPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(LayerListView)

signals:
    void edited(int row, int column, QVariant value);
    void selectionChanged(int row);

public slots:

};

#endif // LAYERLISTVIEW_H
