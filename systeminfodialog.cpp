#include "systeminfodialog.h"
#include "ui_systeminfodialog.h"
#include "opencldeviceinfo.h"

#include <QOpenGLFunctions_3_2_Core>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#include <OpenCL/cl_ext.h>
#else
#include <CL/cl.h>
#include <CL/cl_ext.h>
#endif


#ifndef CL_DEVICE_WARP_SIZE_NV
#define CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV       0x4000
#define CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV       0x4001
#define CL_DEVICE_WARP_SIZE_NV                      0x4003
#endif

#include "canvaswidget-opencl.h"

SystemInfoDialog::SystemInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SystemInfoDialog)
{
    ui->setupUi(this);
}

void addSectionHeader(QString &str, QString const &name)
{
   str.append("<div class=\"sectionHeader\">");
   str.append(name);
   str.append("</div>\n");
}

void addPlatformHeader(QString &str, QString const &name)
{
    str.append("<div class=\"platformHeader\">");
    str.append(name);
    str.append("</div>\n");
}

void addPlatformValue(QString &str, QString const &name, QString const &value)
{
    str.append("<div class=\"platformValue\">");
    if (!name.isEmpty())
        str.append(name).append(": ");
    str.append(value);
    str.append("</div>\n");

}

void SystemInfoDialog::showEvent(QShowEvent *event)
{
    QString queryResultString;

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


    cl_platform_id activePlatform = 0;
    cl_device_id activeDevice = 0;
    bool activeDeviceShared = false;

    if (SharedOpenCL *context = SharedOpenCL::getSharedOpenCLMaybe())
    {
        activePlatform = context->platform;
        activeDevice = context->device;
        activeDeviceShared = context->gl_sharing;
    }

    for (auto &deviceInfo: enumerateOpenCLDevices())
    {
        QString devStr;

        devStr.append(deviceInfo.getPlatformName());
        devStr.append(" - ");
        devStr.append(deviceInfo.getDeviceName());

        if (deviceInfo.platform == activePlatform && deviceInfo.device == activeDevice)
        {
            if (activeDeviceShared)
                devStr.append(" (active shared)");
            else
                devStr.append(" (active)");
        }

        addPlatformHeader(queryResultString, devStr);

        char infoReturnStr[1024];
        cl_uint infoReturnInt = 0;
        clGetDeviceInfo (deviceInfo.device, CL_DEVICE_VERSION, sizeof(infoReturnStr), infoReturnStr, NULL);
        addPlatformValue(queryResultString, NULL, infoReturnStr);

        {
            cl_uint nvComputeMajor = 0;
            cl_uint nvComputeMinor = 0;
            if (CL_SUCCESS == clGetDeviceInfo (deviceInfo.device, CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV, sizeof(nvComputeMajor), &nvComputeMajor, NULL)
                &&
                CL_SUCCESS == clGetDeviceInfo (deviceInfo.device, CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV, sizeof(nvComputeMinor), &nvComputeMinor, NULL))
            {
                addPlatformValue(queryResultString, "NVIDIA Compute Capability", QStringLiteral("%1.%2").arg(nvComputeMajor).arg(nvComputeMinor));
            }
        }

        clGetDeviceInfo (deviceInfo.device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(infoReturnInt), &infoReturnInt, NULL);
        addPlatformValue(queryResultString, "CL_DEVICE_MAX_COMPUTE_UNITS", QString::number(infoReturnInt));

        if (CL_SUCCESS == clGetDeviceInfo (deviceInfo.device, CL_DEVICE_WARP_SIZE_NV, sizeof(infoReturnInt), &infoReturnInt, NULL))
        {
            addPlatformValue(queryResultString, "CL_DEVICE_WARP_SIZE_NV", QString::number(infoReturnInt));
        }

        {
            cl_ulong byteSize;
            clGetDeviceInfo (deviceInfo.device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(byteSize), &byteSize, NULL);
            QString format = QStringLiteral("%1 bytes");

            if (byteSize > 1024)
            {
                byteSize /= 1024;
                format = QStringLiteral("%1 kB");
            }

            if (byteSize > 1024)
            {
                byteSize /= 1024;
                format = QStringLiteral("%1 MB");
            }

            addPlatformValue(queryResultString, "CL_DEVICE_GLOBAL_MEM_SIZE", format.arg(byteSize));
        }

        {
            cl_bool infoReturnBool;
            clGetDeviceInfo (deviceInfo.device, CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(infoReturnBool), &infoReturnBool, NULL);
            addPlatformValue(queryResultString, "CL_DEVICE_HOST_UNIFIED_MEMORY", infoReturnBool ? "True" : "False");
        }
    }

    queryResultString.append("</div>\n");
    queryResultString.append("</body>\n");

    ui->queryOutput->setText(queryResultString);
}

SystemInfoDialog::~SystemInfoDialog()
{
    delete ui;
}
