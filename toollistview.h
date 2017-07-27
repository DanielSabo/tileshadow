#ifndef TOOLLISTVIEW_H
#define TOOLLISTVIEW_H

#include "dragscrollarea.h"
#include "toolfactory.h"

class ToolListViewPrivate;
class ToolListView : public DragScrollArea
{
    Q_OBJECT
public:
    explicit ToolListView(QWidget *parent = 0);
    ~ToolListView();

    void setToolList(ToolList const &list);
    void setActiveTool(const QString &toolPath);

    QSize rowSize() const override;
    int rowCount() const override;

    void paintEvent(QPaintEvent *event) override;
    void viewportClicked(QMouseEvent *event) override;

private:
    QScopedPointer<ToolListViewPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(ToolListView)

signals:
    void selectionChanged(QString const &toolPath);
};

#endif // TOOLLISTVIEW_H
