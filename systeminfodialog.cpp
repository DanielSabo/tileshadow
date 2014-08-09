#include "systeminfodialog.h"
#include "ui_systeminfodialog.h"

#include <QOpenGLFunctions_3_2_Core>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#include <OpenCL/cl_ext.h>
#else
#include <CL/cl.h>
#include <CL/cl_ext.h>
#endif

#include "canvaswidget-opencl.h"

SystemInfoDialog::SystemInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SystemInfoDialog)
{
    ui->setupUi(this);
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
    if (queryResultString.isNull())
    {
        unsigned int num_platforms;
        clGetPlatformIDs (0, NULL, &num_platforms);

        QOpenGLFunctions_3_2_Core gl = QOpenGLFunctions_3_2_Core();
        gl.initializeOpenGLFunctions();

        queryResultString.append("<html><head>\n");

        queryResultString.append("<style>\n");
        queryResultString.append(".queryContent { }\n");
        queryResultString.append(".sectionHeader { font-size: x-large; font-weight: bold;  }\n");
        queryResultString.append(".platformHeader { font-weight: bold; margin-left: 10px; }\n");
        queryResultString.append(".platformValue { margin-left: 20px; white-space: normal; }\n");
        queryResultString.append("</style>\n");

        queryResultString.append("</head><body><div class=\"queryContent\">\n");

        addSectionHeader(queryResultString, "OpenGL");

        addPlatformHeader(queryResultString, (const char *)gl.glGetString(GL_VENDOR));
        addPlatformValue(queryResultString, "Renderer", (const char *)gl.glGetString(GL_RENDERER));
        addPlatformValue(queryResultString, "GL Version", (const char *)gl.glGetString(GL_VERSION));
        addPlatformValue(queryResultString, "GLSL Version", (const char *)gl.glGetString(GL_SHADING_LANGUAGE_VERSION));

        addSectionHeader(queryResultString, "OpenCL");

        std::list<OpenCLDeviceInfo> deviceInfoList = enumerateOpenCLDevices();
        std::list<OpenCLDeviceInfo>::iterator deviceInfoIter;

        for (deviceInfoIter = deviceInfoList.begin(); deviceInfoIter != deviceInfoList.end(); ++deviceInfoIter)
        {
            QString devStr;

            devStr.append(deviceInfoIter->getPlatformName());
            devStr.append(" - ");
            devStr.append(deviceInfoIter->getDeviceName());
            addPlatformHeader(queryResultString, devStr.toUtf8().constData());

            char infoReturnStr[1024];
            cl_uint infoReturnInt = 0;
            clGetDeviceInfo (deviceInfoIter->device, CL_DEVICE_VERSION, sizeof(infoReturnStr), infoReturnStr, NULL);
            addPlatformValue(queryResultString, NULL, infoReturnStr);

#ifdef CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV
            {
                cl_uint nvComputeMajor = 0;
                cl_uint nvComputeMinor = 0;
                if (CL_SUCCESS == clGetDeviceInfo (deviceInfoIter->device, CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV, sizeof(nvComputeMajor), &nvComputeMajor, NULL)
                    &&
                    CL_SUCCESS == clGetDeviceInfo (deviceInfoIter->device, CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV, sizeof(nvComputeMinor), &nvComputeMinor, NULL))
                {
                    sprintf(infoReturnStr, "%d.%d", nvComputeMajor, nvComputeMinor);
                    addPlatformValue(queryResultString, "NVIDIA Compute Capability", infoReturnStr);
                }
            }
#endif

            clGetDeviceInfo (deviceInfoIter->device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(infoReturnInt), &infoReturnInt, NULL);
            sprintf(infoReturnStr, "%d", infoReturnInt);
            addPlatformValue(queryResultString, "CL_DEVICE_MAX_COMPUTE_UNITS", infoReturnStr);

#ifdef CL_DEVICE_WARP_SIZE_NV
            if (CL_SUCCESS == clGetDeviceInfo (deviceInfoIter->device, CL_DEVICE_WARP_SIZE_NV, sizeof(infoReturnInt), &infoReturnInt, NULL))
            {
                sprintf(infoReturnStr, "%d", infoReturnInt);
                addPlatformValue(queryResultString, "CL_DEVICE_WARP_SIZE_NV", infoReturnStr);
            }
#endif
        }

        queryResultString.append("</div>\n");
        queryResultString.append("</body>\n");
    }

    ui->queryOutput->setText(queryResultString);
}

SystemInfoDialog::~SystemInfoDialog()
{
    delete ui;
}
