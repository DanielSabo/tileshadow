#ifndef TOOLLISTVIEW_H
#define TOOLLISTVIEW_H

#include <QWidget>
#include <QAbstractScrollArea>
#include "toolfactory.h"

class ToolListViewPrivate;
class ToolListView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit ToolListView(QWidget *parent = 0);

    void setToolList(ToolList const &list);
    void setActiveTool(const QString &toolPath);

    QSize viewportSizeHint() const;
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    ToolListViewPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(ToolListView)

signals:
    void selectionChanged(QString const &toolPath);
};

#endif // TOOLLISTVIEW_H
