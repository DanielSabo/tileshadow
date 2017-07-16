#ifndef IMAGEPREVIEWWIDGET_H
#define IMAGEPREVIEWWIDGET_H

#include <QWidget>
#include <QImage>

class ImagePreviewWidget : public QWidget
{
    Q_OBJECT

public:
    ImagePreviewWidget(QWidget *parent = 0);
    QSize sizeHint() const;
    void setImage(QImage const &image, QColor const &background);

protected:
    void paintEvent(QPaintEvent *event);

    QColor background;
    QImage image;
};

#endif // IMAGEPREVIEWWIDGET_H
