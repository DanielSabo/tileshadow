#include "hsvcolordial.h"
#include <QDebug>
#include <QPainter>
#include <QImage>
#include <QRgb>
#include <qmath.h>
#include <QMouseEvent>

static const double BOX_VALUE_FACTOR = 0.75;

class HSVColorDialPrivate
{
public:
    HSVColorDialPrivate() :
        h(0), s(1), v(1),
        inDialDrag(false),
        inBoxDrag(false)
    {
    }

    ~HSVColorDialPrivate()
    {
    }

    bool inDialDrag;
    bool inBoxDrag;

    float h;
    float s;
    float v;

    QRect boxRect;

    QImage renderedInnerBox;
    QImage renderedDial;
};

HSVColorDial::HSVColorDial(QWidget *parent) :
    QWidget(parent),
    d_ptr(new HSVColorDialPrivate)
{
}

bool HSVColorDial::hasHeightForWidth() const
{
    return true;
}

int HSVColorDial::heightForWidth(int w) const
{
    return w;
}

QSizePolicy HSVColorDial::sizePolicy() const
{
    QSizePolicy result;
    result.setHeightForWidth(true);

    return result;
}

void HSVColorDial::setColor(const QColor &color)
{
    Q_D(HSVColorDial);
    d->h = color.hueF();
    d->s = color.saturationF();
    d->v = color.valueF();

    while (d->h < 0.0f)
        d->h += 1.0f;
    while (d->h > 1.0f)
        d->h -= 1.0f;

    emit updateColor(QColor::fromHsvF(d->h, d->s, d->v));

    d->renderedInnerBox = QImage();
    update();
}

QColor HSVColorDial::getColor() const
{
    return QColor::fromHsvF(d_ptr->h, d_ptr->s, d_ptr->v);
}

void HSVColorDial::resizeEvent(QResizeEvent *event)
{
    Q_D(HSVColorDial);
    d->renderedDial = QImage();
    d->renderedInnerBox = QImage();
}

static float hueFromXY(float x, float y)
{
    float theta;

    if (y >= 0.0)
    {
        if (x * x < 0.001)
            theta = M_PI / 2;
        else if (x >= 0.0)
            theta = atan(y / x);
        else
            theta = M_PI + atan(y / x);
    }
    else
    {
        if (x * x < 0.001)
            theta = M_PI / 2 + M_PI;
        else if (x >= 0.0)
            theta = 2 * M_PI + atan(y / x);
        else
            theta = M_PI + atan(y / x);
    }

    return 1.0 - theta / (2 * M_PI);
}

void HSVColorDial::paintEvent(QPaintEvent *event)
{
    Q_D(HSVColorDial);

    const int dialSize = (qMin(width(), height()) / 2) * 2;
    const int dialOffsetX = dialSize < width() ? (width() - dialSize) / 2 : 0;
    const float outerRadius = dialSize / 2 - dialSize * .03;
    const float innerRadius = outerRadius - dialSize * .1;

//    QRgb widgetBackgroundPixel = QWidget::palette().color(QWidget::backgroundRole()).rgba();
    QRgb widgetBackgroundPixel = qRgba(0, 0, 0, 0);

    if (d->renderedDial.isNull())
    {
        d->renderedDial = QImage(dialSize, dialSize, QImage::Format_ARGB32);

        for (int row = 0; row < dialSize; row++)
        {
            QRgb *pixel = (QRgb *)d->renderedDial.scanLine(row);
            for (int col = 0; col < dialSize; col++)
            {
                float dx = col - dialSize / 2.0f;
                float dy = row - dialSize / 2.0f;

                float r = sqrtf(dx*dx + dy*dy);
                if (r <= outerRadius && r >= innerRadius)
                {
                    float alpha;

                    if (outerRadius - 1 < r)
                        alpha = outerRadius - r;
                    else if (innerRadius + 1 > r)
                        alpha = r - innerRadius;
                    else
                        alpha = 1.0f;

                    float angle = hueFromXY(dx, dy);

                    QColor pixelColor = QColor::fromHsvF(angle, 1.0, 1.0, alpha);
                    *pixel++ = pixelColor.rgba();
                }
                else
                    *pixel++ = widgetBackgroundPixel;
            }
        }
    }

    if (d->renderedInnerBox.isNull())
    {
        QPoint center = QPoint(dialSize / 2, dialSize / 2);
        float boxInnerRadius = innerRadius - 2;
        QPoint p1 = QPoint(cos(3.0 / 4.0 * M_PI) * boxInnerRadius, sin(-1.0 / 4.0 * M_PI) * boxInnerRadius) + center;
        QPoint p2 = QPoint(cos(-1.0 / 4.0 * M_PI) * boxInnerRadius, sin(3.0 / 4.0 * M_PI) * boxInnerRadius) + center;
        d->boxRect = QRect(p1.x(), p1.y(), abs(p2.x() - p1.x()), abs(p2.y() - p1.y()));

        d->renderedInnerBox = QImage(d->boxRect.size(), QImage::Format_ARGB32);

        for (int row = 0; row < d->boxRect.height(); row++)
        {
            QRgb *pixel = (QRgb *)d->renderedInnerBox.scanLine(row);
            float v = float(d->boxRect.height() - row) / d->boxRect.height();
            v = powf(v, BOX_VALUE_FACTOR);
            for (int col = 0; col < d->boxRect.width(); col++)
            {
                float s = float(d->boxRect.width() - col) / d->boxRect.width();
                s = powf(s, BOX_VALUE_FACTOR);

                QColor pixelColor = QColor::fromHsvF(d->h, s, v);
                *pixel++ = pixelColor.rgba();
            }
        }
    }

    QPainter painter(this);
    painter.drawImage(QPoint(dialOffsetX,0), d->renderedDial);
    painter.drawImage(d->boxRect.topLeft() + QPoint(dialOffsetX, 0), d->renderedInnerBox);

    { /* Selected hue */
        QPointF center = QPointF(dialSize / 2 + dialOffsetX, dialSize / 2) + QPointF(0.5f, 0.5f);
        QPointF p1 = QPointF(cos(d->h * -2.0 * M_PI) * (innerRadius + 2), sin(d->h * -2.0 * M_PI) * (innerRadius + 2)) + center;
        QPointF p2 = QPointF(cos(d->h * -2.0 * M_PI) * (outerRadius - 2), sin(d->h * -2.0 * M_PI) * (outerRadius - 2)) + center;

        QPen pen(QColor::fromRgb(0xFF, 0xFF, 0xFF));
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawLine(p1, p2);
    }
    { /* Selected saturation, value */
        float yFactor = 1.0f - powf(d->v, 1.0 / BOX_VALUE_FACTOR);
        float xFactor = 1.0f - powf(d->s, 1.0 / BOX_VALUE_FACTOR);
        const int SELECT_CIRCLE_RADIUS = 3;

        QPoint selectedPoint = QPoint(round(xFactor * d->boxRect.width()), round(yFactor * d->boxRect.height())) + d->boxRect.topLeft();
        QRect selectedBox = QRect(selectedPoint.x() - SELECT_CIRCLE_RADIUS - 1 + dialOffsetX,
                                  selectedPoint.y() - SELECT_CIRCLE_RADIUS - 1,
                                  SELECT_CIRCLE_RADIUS * 2 + 1,
                                  SELECT_CIRCLE_RADIUS * 2 + 1);

        QPen pen;

        if (d->s < 0.2 && d->v > 0.5)
            pen.setColor(QColor::fromRgb(0x00, 0x00, 0x00));
        else
            pen.setColor(QColor::fromRgb(0xFF, 0xFF, 0xFF));
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawEllipse(selectedBox);
    }
}

void HSVColorDial::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(HSVColorDial);

    const int dialSize = (qMin(width(), height()) / 2) * 2;
    const int dialOffsetX = dialSize < width() ? (width() - dialSize) / 2 : 0;
    float dx = event->x() - dialOffsetX - dialSize / 2.0f;
    float dy = event->y() - dialSize / 2.0f;

    if (d->inDialDrag)
    {
        float h = hueFromXY(dx, dy);
        d->h = h;
        d->renderedInnerBox = QImage();
        update();
        QColor value = QColor::fromHsvF(d->h, d->s, d->v);
        emit dragColor(value);
        emit updateColor(value);
    }
    else if(d->inBoxDrag)
    {
        float s = event->x() - dialOffsetX - d->boxRect.x();
        s = 1.0f - qBound(0.0f, s, float(d->boxRect.width())) / d->boxRect.width();
        s = powf(s, BOX_VALUE_FACTOR);
        float v = event->y() - d->boxRect.y();
        v = 1.0f - qBound(0.0f, v, float(d->boxRect.height())) / d->boxRect.height();
        v = powf(v, BOX_VALUE_FACTOR);

        d->s = s;
        d->v = v;
        update();
        QColor value = QColor::fromHsvF(d->h, d->s, d->v);
        emit dragColor(value);
        emit updateColor(value);
    }
}

void HSVColorDial::mousePressEvent(QMouseEvent *event)
{
    Q_D(HSVColorDial);

    const int dialSize = (qMin(width(), height()) / 2) * 2;
    const int dialOffsetX = dialSize < width() ? (width() - dialSize) / 2 : 0;
    const float outerRadius = dialSize / 2 - dialSize * .03;
    const float innerRadius = outerRadius - dialSize * .1;

    float dx = event->x() - dialOffsetX - dialSize / 2.0f;
    float dy = event->y() - dialSize / 2.0f;

    float r = sqrtf(dx*dx + dy*dy);
    if (r <= outerRadius && r >= innerRadius)
    {
        float h = hueFromXY(dx, dy);
        d->h = h;
        d->renderedInnerBox = QImage();
        update();
        d->inDialDrag = true;
        d->inBoxDrag = false;
        QColor value = QColor::fromHsvF(d->h, d->s, d->v);
        emit dragColor(value);
        emit updateColor(value);
    }
    else if (d->boxRect.contains(event->pos()))
    {
        float s = event->x() - dialOffsetX - d->boxRect.x();
        s = 1.0f - qBound(0.0f, s, float(d->boxRect.width())) / d->boxRect.width();
        s = powf(s, BOX_VALUE_FACTOR);
        float v = event->y() - d->boxRect.y();
        v = 1.0f - qBound(0.0f, v, float(d->boxRect.height())) / d->boxRect.height();
        v = powf(v, BOX_VALUE_FACTOR);

        d->s = s;
        d->v = v;
        update();
        d->inDialDrag = false;
        d->inBoxDrag = true;
        QColor value = QColor::fromHsvF(d->h, d->s, d->v);
        emit dragColor(value);
        emit updateColor(value);
    }
}

void HSVColorDial::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(HSVColorDial);

    const int dialSize = (qMin(width(), height()) / 2) * 2;
    const int dialOffsetX = dialSize < width() ? (width() - dialSize) / 2 : 0;

    float dx = event->x() - dialOffsetX - dialSize / 2.0f;
    float dy = event->y() - dialSize / 2.0f;

    if (d->inDialDrag)
    {
        float h = hueFromXY(dx, dy);
//        qDebug() << h << QColor::fromHsvF(h, 1, 1);
        d->h = h;
        d->renderedInnerBox = QImage();
        update();
        QColor value = QColor::fromHsvF(d->h, d->s, d->v);
        emit releaseColor(value);
        emit updateColor(value);
    }
    else if (d->inBoxDrag)
    {
        QColor value = QColor::fromHsvF(d->h, d->s, d->v);
        emit releaseColor(value);
        emit updateColor(value);
    }

    d->inDialDrag = false;
    d->inBoxDrag = false;
}
