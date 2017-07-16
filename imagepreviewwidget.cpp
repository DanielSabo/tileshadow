#include "imagepreviewwidget.h"
#include <QPaintEvent>
#include <QPainter>

ImagePreviewWidget::ImagePreviewWidget(QWidget *parent) :
    QWidget(parent)
{
}

QSize ImagePreviewWidget::sizeHint() const
{
    return image.size();
}

void ImagePreviewWidget::setImage(const QImage &image, const QColor &background)
{
    this->image = image;
    this->background = background;

    update();
}

void ImagePreviewWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    QSize widgetArea = size();
    QSize imageArea = image.size();
    QPoint origin = {
        widgetArea.width() / 2 - imageArea.width() / 2,
        widgetArea.height() / 2 - imageArea.height() / 2,
    };
    origin += image.offset();
    painter.fillRect(event->rect(), background);
    painter.drawImage(origin, image);
}
