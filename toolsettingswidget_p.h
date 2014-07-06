#ifndef TOOLSETTINGSWIDGET_P_H
#define TOOLSETTINGSWIDGET_P_H

#include "canvaswidget.h"
#include "hsvcolordial.h"
#include <QSlider>

class ToolSettingsWidgetPrivate : public QObject
{
    Q_OBJECT
public:
    ToolSettingsWidgetPrivate();

    CanvasWidget *canvas;
    HSVColorDial *toolColorDial;
    QSlider *toolSizeSlider;

    bool freezeUpdates;

public slots:
    void colorDialChanged(QColor const &color);
    void sizeSliderMoved(int value);
};

#endif // TOOLSETTINGSWIDGET_P_H
