#ifndef TOOLEXTENDEDSETTINGSWINDOW_H
#define TOOLEXTENDEDSETTINGSWINDOW_H

#include <QWidget>
#include <QImage>

class CanvasWidget;
class ToolExtendedSettingsWindowPrivate;
class ToolExtendedSettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ToolExtendedSettingsWindow(CanvasWidget *canvas, QWidget *parent = 0);
    ~ToolExtendedSettingsWindow();

protected:
    void closeEvent(QCloseEvent *);
    void keyPressEvent(QKeyEvent *);
    bool event(QEvent *);

private:
    QScopedPointer<ToolExtendedSettingsWindowPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(ToolExtendedSettingsWindow)

signals:

public slots:
    void updateTool();
};

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    PreviewWidget(QWidget *parent = 0);
    QSize sizeHint() const;
    void setImage(QImage const &image, QColor const &background);

protected:
    void paintEvent(QPaintEvent *event);

    QColor background;
    QImage image;
};

#endif // TOOLEXTENDEDSETTINGSWINDOW_H
