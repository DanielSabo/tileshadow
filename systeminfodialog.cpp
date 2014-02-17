#include "systeminfodialog.h"
#include "ui_systeminfodialog.h"

#include <QOpenGLFunctions_3_2_Core>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "canvaswidget-opencl.h"

SystemInfoDialog::SystemInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SystemInfoDialog)
{
    ui->setupUi(this);

    outputLabel = findChild<QLabel*>("queryOutput");
}

void addSectionHeader(QString &str, const char *name)
{
   str.append("<div class=\"sectionHeader\">");
   str.append(name);
   str.append("</div>\n");
}

void addPlatformHeader(QString &str, const char *name)
{
    str.append("<div class=\"platformHeader\">");
    str.append(name);
    str.append("</div>\n");
}

void addPlatformValue(QString &str, const char *name, const char *value)
{
    str.append("<div class=\"platformValue\">");
    if (name && strlen(name))
        str.append(name).append(": ");
    str.append(value);
    str.append("</div>\n");

}

void SystemInfoDialog::showEvent(QShowEvent *event)
{
    (void)event;

    if (!outputLabel)
        return;

    if (queryResultString.isNull())
    {
        queryResultString.reset(new QString ());
        unsigned int num_platforms;
        clGetPlatformIDs (0, NULL, &num_platforms);

        QOpenGLFunctions_3_2_Core gl = QOpenGLFunctions_3_2_Core();
        gl.initializeOpenGLFunctions();

        queryResultString->append("<html><head>\n");

        queryResultString->append("<style>\n");
        queryResultString->append(".queryContent { }\n");
        queryResultString->append(".sectionHeader { font-size: x-large; font-weight: bold;  }\n");
        queryResultString->append(".platformHeader { font-weight: bold; margin-left: 10px; }\n");
        queryResultString->append(".platformValue { margin-left: 20px; white-space: normal; }\n");
        queryResultString->append("</style>\n");

        queryResultString->append("</head><body><div class=\"queryContent\">\n");

        addSectionHeader(*queryResultString, "OpenGL");

        addPlatformHeader(*queryResultString, (const char *)gl.glGetString(GL_VENDOR));
        addPlatformValue(*queryResultString, "Renderer", (const char *)gl.glGetString(GL_RENDERER));
        addPlatformValue(*queryResultString, "GL Version", (const char *)gl.glGetString(GL_VERSION));
        addPlatformValue(*queryResultString, "GLSL Version", (const char *)gl.glGetString(GL_SHADING_LANGUAGE_VERSION));

        addSectionHeader(*queryResultString, "OpenCL");

        std::list<OpenCLDeviceInfo> deviceInfoList = enumerateOpenCLDevices();
        std::list<OpenCLDeviceInfo>::iterator deviceInfoIter;

        for (deviceInfoIter = deviceInfoList.begin(); deviceInfoIter != deviceInfoList.end(); ++deviceInfoIter)
        {
            QString devStr;

            devStr.append(deviceInfoIter->getPlatformName());
            devStr.append(" - ");
            devStr.append(deviceInfoIter->getDeviceName());
            addPlatformHeader(*queryResultString, devStr.toUtf8().data());

            char infoReturnStr[1024];
            clGetDeviceInfo (deviceInfoIter->device, CL_DEVICE_VERSION, sizeof(infoReturnStr), infoReturnStr, NULL);
            addPlatformValue(*queryResultString, NULL, infoReturnStr);
        }

        queryResultString->append("</div>\n");
        queryResultString->append("</body>\n");
    }

    outputLabel->setText(*queryResultString);
}

SystemInfoDialog::~SystemInfoDialog()
{
    delete ui;
}
