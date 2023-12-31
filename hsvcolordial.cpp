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

    float h;
    float s;
    float v;

    bool inDialDrag;
    bool inBoxDrag;

    QRect boxRect;

    QImage renderedInnerBox;
    QImage renderedDial;
};

namespace {
struct ColorDialMetrics {
    ColorDialMetrics(int width, int height) {
        size = (qMin(width, height) / 2) * 2;
        offsetX = size < width ? (width - size) / 2 : 0;
        outerRadius = size / 2 - size * .03;
        innerRadius = outerRadius - size * .1;
    }

    int size;
    int offsetX;
    float outerRadius;
    float innerRadius;
};

// Hue rotates counter-clockwise with 0 (red) on the right hand side
float hueFromXY(float x, float y)
{
    return atan2(y, -x) / (2 * M_PI) + 0.5;
}
}

HSVColorDial::HSVColorDial(QWidget *parent) :
    QWidget(parent),
    d_ptr(new HSVColorDialPrivate)
{
}

HSVColorDial::~HSVColorDial()
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

void HSVColorDial::paintEvent(QPaintEvent *event)
{
    Q_D(HSVColorDial);

    const auto metrics = ColorDialMetrics(width(), height());
    const int pixelRatio = devicePixelRatio();

//    QRgb widgetBackgroundPixel = QWidget::palette().color(QWidget::backgroundRole()).rgba();
    QRgb widgetBackgroundPixel = qRgba(0, 0, 0, 0);

    if (d->renderedDial.isNull())
    {
        int dialSize = metrics.size;
        float rFactor = 1.0f;
        if (pixelRatio > 1)
        {
            dialSize *= pixelRatio;
            rFactor = 1.0f / pixelRatio;
            d->renderedDial = QImage(dialSize, dialSize, QImage::Format_ARGB32);
            d->renderedDial.setDevicePixelRatio(pixelRatio);
        }
        else
        {
            d->renderedDial = QImage(dialSize, dialSize, QImage::Format_ARGB32);
        }

        for (int row = 0; row < dialSize; row++)
        {
            QRgb *pixel = (QRgb *)d->renderedDial.scanLine(row);
            for (int col = 0; col < dialSize; col++)
            {
                float dx = col - dialSize / 2.0f;
                float dy = row - dialSize / 2.0f;

                float r = sqrtf(dx*dx + dy*dy) * rFactor;
                if (r <= metrics.outerRadius && r >= metrics.innerRadius)
                {
                    float alpha;

                    if (metrics.outerRadius - 1 < r)
                        alpha = metrics.outerRadius - r;
                    else if (metrics.innerRadius + 1 > r)
                        alpha = r - metrics.innerRadius;
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
        int boxInnerRadius = (metrics.innerRadius - 2) * 0.7071;
        int boxOrigin = metrics.size / 2 - boxInnerRadius;
        int boxSize = boxInnerRadius * 2;
        d->boxRect = QRect(boxOrigin, boxOrigin, boxSize, boxSize);

        if (pixelRatio > 1)
        {
            boxSize *= pixelRatio;
            d->renderedInnerBox = QImage(boxSize, boxSize, QImage::Format_ARGB32);
            d->renderedInnerBox.setDevicePixelRatio(pixelRatio);
        }
        else
        {
            d->renderedInnerBox = QImage(boxSize, boxSize, QImage::Format_ARGB32);
        }

        for (int row = 0; row < boxSize; row++)
        {
            QRgb *pixel = (QRgb *)d->renderedInnerBox.scanLine(row);
            float v = float(boxSize - row) / boxSize;
            v = powf(v, BOX_VALUE_FACTOR);
            for (int col = 0; col < boxSize; col++)
            {
                float s = float(boxSize - col) / boxSize;
                s = powf(s, BOX_VALUE_FACTOR);

                QColor pixelColor = QColor::fromHsvF(d->h, s, v);
                *pixel++ = pixelColor.rgba();
            }
        }
    }

    QPainter painter(this);
    painter.drawImage(QPoint(metrics.offsetX,0), d->renderedDial);
    painter.drawImage(d->boxRect.topLeft() + QPoint(metrics.offsetX, 0), d->renderedInnerBox);

    { /* Selected hue */
        QPointF center = QPointF(metrics.size / 2 + metrics.offsetX, metrics.size / 2) + QPointF(0.5f, 0.5f);
        QPointF vector = QPointF(cos(d->h * -2.0 * M_PI), sin(d->h * -2.0 * M_PI));
        QPointF p1 = vector * (metrics.innerRadius + 2) + center;
        QPointF p2 = vector * (metrics.outerRadius - 2) + center;

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
        QRect selectedBox = QRect(selectedPoint.x() - SELECT_CIRCLE_RADIUS - 1 + metrics.offsetX,
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

    const auto metrics = ColorDialMetrics(width(), height());
    float dx = event->x() - metrics.offsetX - metrics.size / 2.0f;
    float dy = event->y() - metrics.size / 2.0f;

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
        float s = event->x() - metrics.offsetX - d->boxRect.x();
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

    const auto metrics = ColorDialMetrics(width(), height());
    float dx = event->x() - metrics.offsetX - metrics.size / 2.0f;
    float dy = event->y() - metrics.size / 2.0f;

    float r = sqrtf(dx*dx + dy*dy);
    if (r <= metrics.outerRadius && r >= metrics.innerRadius)
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
        float s = event->x() - metrics.offsetX - d->boxRect.x();
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

    const auto metrics = ColorDialMetrics(width(), height());
    float dx = event->x() - metrics.offsetX - metrics.size / 2.0f;
    float dy = event->y() - metrics.size / 2.0f;

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
