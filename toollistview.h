#ifndef TOOLLISTVIEW_H
#define TOOLLISTVIEW_H

#include <QWidget>
#include "toolfactory.h"

class ToolListViewPrivate;
class ToolListView : public QWidget
{
    Q_OBJECT
public:
    explicit ToolListView(QWidget *parent = 0);

    void setToolList(ToolList const &list);
    void setActiveTool(const QString &toolPath);

    QSize sizeHint() const;
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    ToolListViewPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(ToolListView)

signals:
    void selectionChanged(QString const &toolPath);
};

#endif // TOOLLISTVIEW_H
