#ifndef TOOLLISTPOPUP_H
#define TOOLLISTPOPUP_H

#include <QWidget>
#include "toollistwidget.h"

class ToolListPopupPrivate;
class ToolListPopup : public QWidget
{
    Q_OBJECT
public:
    explicit ToolListPopup(QWidget *parent = 0);
    void setToolList(ToolList const &list);
    void setActiveTool(const QString &toolPath);
    void reposition(const QRect &globalBounds, const QPoint &globalOrigin);

protected:
    ToolList toolList;

private:
    ToolListPopupPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(ToolListPopup)

signals:

public slots:
    void toolButtonClicked();
};

#endif // TOOLLISTPOPUP_H
