#ifndef PATHLISTWIDGET_H
#define PATHLISTWIDGET_H

#include <QWidget>

class PathListWidgetPrivate;
class PathListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PathListWidget(QWidget *parent = nullptr);
    ~PathListWidget();

    void setPaths(QStringList staticPaths, QStringList paths);
    QStringList getPaths();

protected:
    QScopedPointer<PathListWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(PathListWidget)

signals:
    void changed();

public slots:
};

#endif // PATHLISTWIDGET_H
