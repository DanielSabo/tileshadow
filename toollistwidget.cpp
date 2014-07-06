#include "toollistwidget.h"

#include <QVBoxLayout>
#include <QDir>
#include <QPushButton>

#include <QDebug>

#include "canvaswidget.h"

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

ToolListWidget::ToolListWidget(QWidget *parent) :
    QWidget(parent),
    canvas(NULL)
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

void ToolListWidget::reloadTools()
{
    QBoxLayout *toolboxLayout = qobject_cast<QBoxLayout *>(layout());
    Q_ASSERT(toolboxLayout);

    QList<QWidget *>toolButtons = findChildren<QWidget *>();
    for (int i = 0; i < toolButtons.size(); ++i) {
        delete toolButtons[i];
    }

    QStringList brushFiles = QDir(":/mypaint-tools/").entryList();

    for (int i = 0; i < brushFiles.size(); ++i)
    {
        QString brushfile = brushFiles.at(i);
        QString buttonName = brushfile;
        buttonName.truncate(buttonName.length() - 4);
        //buttonName.replace(QChar('-'), " ");
        buttonName.replace(QChar('_'), " ");
        asciiTitleCase(buttonName);

        QPushButton *button = new QPushButton(buttonName);
        button->setProperty("toolName", brushfile);
        button->setCheckable(true);
        connect(button, SIGNAL(clicked()), this, SLOT(setActiveTool()));
        toolboxLayout->insertWidget(i, button);
    }

    QPushButton *button = new QPushButton("Debug");
    button->setProperty("toolName", QString("debug"));
    button->setCheckable(true);
    connect(button, SIGNAL(clicked()), this, SLOT(setActiveTool()));
    toolboxLayout->insertWidget(0, button);
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
}

void ToolListWidget::setActiveTool()
{
    if (!canvas)
        return;

    QVariant toolNameProp = sender()->property("toolName");
    if (toolNameProp.type() == QVariant::String)
        canvas->setActiveTool(toolNameProp.toString());
    else
        qWarning() << sender()->objectName() << " has no toolName property.";
}
