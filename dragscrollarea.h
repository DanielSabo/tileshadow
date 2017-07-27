#ifndef DRAGSCROLLAREA_H
#define DRAGSCROLLAREA_H
#include <QAbstractScrollArea>

class DragScrollAreaPrivate;
class DragScrollArea : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit DragScrollArea(QWidget *parent = 0);
    virtual ~DragScrollArea() override;

    virtual QSize rowSize() const = 0;
    virtual int rowCount() const = 0;
    QSize viewportSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void viewportClicked(QMouseEvent *event);

private:
    QScopedPointer<DragScrollAreaPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(DragScrollArea)
};

#endif // DRAGSCROLLAREA_H
