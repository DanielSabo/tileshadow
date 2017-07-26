#ifndef SIDEBARPOPUP_H
#define SIDEBARPOPUP_H

#include <QWidget>

class SidebarPopup : public QWidget
{
    Q_OBJECT
public:
    explicit SidebarPopup(QWidget *parent = 0);
    ~SidebarPopup();

    void reposition(QWidget *sidebar, QWidget *source);
    void reposition(const QRect &globalBounds, const QPoint &globalOrigin);
};

#endif // SIDEBARPOPUP_H
