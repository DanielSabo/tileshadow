#ifndef HSVCOLORDIAL_H
#define HSVCOLORDIAL_H

#include <QWidget>

class HSVColorDialPrivate;
class HSVColorDial : public QWidget
{
    Q_OBJECT
public:
    explicit HSVColorDial(QWidget *parent = 0);

    bool hasHeightForWidth() const;
    int heightForWidth(int w) const;
    QSizePolicy sizePolicy() const;

    void setColor(const QColor &color);
    QColor getColor() const;

protected:
    void resizeEvent(QResizeEvent * event);
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    HSVColorDialPrivate * const d_ptr;

private:
    Q_DECLARE_PRIVATE(HSVColorDial)

signals:
    void dragColor(QColor const &color);
    void releaseColor(QColor const &color);
    void updateColor(QColor const &color);

public slots:

};

#endif // HSVCOLORDIAL_H
