#include "toollistwidget.h"

#include <QVBoxLayout>
#include <QDir>
#include <QPushButton>

#include <QDebug>

#include "canvaswidget.h"
#include "toollistpopup.h"

static void asciiTitleCase(QString &instr)
{
    bool lastspace = true;
    for (int i = 0; i < instr.size(); i++)
    {
        if (instr[i].isSpace())
        {
            lastspace = true;
            continue;
        }

        if (lastspace)
        {
            instr[i] = instr[i].toUpper();
            lastspace = false;
        }
    }
}

class ToolListWidgetPrivate
{
public:
    QPushButton *toolPopupButton;
};

ToolListWidget::ToolListWidget(QWidget *parent) :
    QWidget(parent),
    canvas(NULL),
    popup(NULL),
    d_ptr(new ToolListWidgetPrivate)
{
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(3);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);
}

ToolListWidget::~ToolListWidget()
{
    if (canvas)
        disconnect(canvas, 0, this, 0);
    canvas = NULL;
}

static QStringList findBrushFiles(QDir const &path)
{
    QFileInfoList infoList = path.entryInfoList();
    QStringList result;

    QFileInfoList directories;

    foreach(QFileInfo info, infoList)
    {
        if (info.isDir())
            directories << info;
        else if (info.fileName().endsWith(".myb"))
            result << info.filePath();
    }

    foreach(QFileInfo info, directories)
    {
        result << findBrushFiles(info.filePath());
    }

    return result;
}

void ToolListWidget::reloadTools()
{
    Q_D(ToolListWidget);

    QBoxLayout *toolboxLayout = qobject_cast<QBoxLayout *>(layout());
    Q_ASSERT(toolboxLayout);

    QList<QWidget *>toolButtons = findChildren<QWidget *>();
    for (int i = 0; i < toolButtons.size(); ++i) {
        delete toolButtons[i];
    }

    toolList.clear();
    toolList.append(QPair<QString, QString>("debug", "Debug"));

    QStringList brushFiles = findBrushFiles(QDir(":/mypaint-tools/"));

    for (int i = 0; i < brushFiles.size(); ++i)
    {
        QString brushFile = brushFiles.at(i);

        QStringList splitName = brushFile.split("/");
        splitName.removeFirst();
        splitName.removeFirst();

        //FIXME: This should use the full path but canvas expects the truncated one
        brushFile = splitName.join("/");
        QString buttonName = splitName.join(" - ");
        buttonName.truncate(buttonName.length() - 4);
        buttonName.replace(QChar('_'), " ");
        asciiTitleCase(buttonName);
        toolList.append(QPair<QString, QString>(brushFile, buttonName));
    }

    {
        QPushButton *button = new QPushButton("Tools...");
        connect(button, SIGNAL(clicked()), this, SLOT(showPopup()));
        toolboxLayout->addWidget(button);
        d->toolPopupButton = button;
    }

#if 0
    for (int i = 0; i < toolList.size(); ++i)
    {
        QString toolPath = toolList.at(i).first;
        QString name = toolList.at(i).second;

        QPushButton *button = new QPushButton(name);
        button->setProperty("toolName", QString(toolPath));
        button->setCheckable(true);
        connect(button, SIGNAL(clicked()), this, SLOT(setActiveTool()));
        toolboxLayout->addWidget(button);
    }
#endif
}

void ToolListWidget::setCanvas(CanvasWidget *newCanvas)
{
    if (canvas)
        disconnect(canvas, 0, this, 0);

    canvas = newCanvas;

    if (canvas)
    {
        connect(canvas, SIGNAL(updateTool()), this, SLOT(updateTool()));
        connect(canvas, SIGNAL(destroyed(QObject *)), this, SLOT(canvasDestroyed(QObject *)));
    }

    updateTool();
}

void ToolListWidget::canvasDestroyed(QObject *obj)
{
    canvas = NULL;
}

void ToolListWidget::updateTool()
{
    if (!canvas)
        return;

    QList<QPushButton *>toolButtons = findChildren<QPushButton *>();
    QString activeTool = canvas->getActiveTool();
    for (int i = 0; i < toolButtons.size(); ++i)
    {
        QPushButton *button = toolButtons[i];
        if (button->property("toolName").toString() == activeTool)
            button->setChecked(true);
        else
            button->setChecked(false);
    }

    if (popup)
        popup->setActiveTool(activeTool);
}

void ToolListWidget::setActiveTool()
{
    QVariant toolNameProp = sender()->property("toolName");
    if (toolNameProp.type() == QVariant::String)
        pickTool(toolNameProp.toString());
    else
        qWarning() << sender()->objectName() << " has no toolName property.";
}

void ToolListWidget::pickTool(QString const &toolPath)
{
    if (canvas)
        canvas->setActiveTool(toolPath);
}

void ToolListWidget::showPopup()
{
    Q_D(ToolListWidget);

    if (!popup)
        popup = new ToolListPopup(this);
    popup->setToolList(toolList);
    if (canvas)
        popup->setActiveTool(canvas->getActiveTool());

    QPoint buttonPos = d->toolPopupButton->mapToGlobal(d->toolPopupButton->pos());
    int originY = buttonPos.y() + d->toolPopupButton->height() / 2;
    int originX = mapToGlobal(pos()).x();

    popup->reposition(window()->frameGeometry(), QPoint(originX, originY));
}
