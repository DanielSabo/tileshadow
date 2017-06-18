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

namespace {
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
}

void SystemInfoDialog::showEvent(QShowEvent *event)
{
    QString queryResultString;

    QOpenGLFunctions_3_2_Core gl = QOpenGLFunctions_3_2_Core();

    queryResultString.append("<html><head>\n");

    queryResultString.append("<style>\n");
    queryResultString.append(".queryContent { }\n");
    queryResultString.append(".sectionHeader { font-size: x-large; font-weight: bold;  }\n");
    queryResultString.append(".platformHeader { font-weight: bold; margin-left: 10px; }\n");
    queryResultString.append(".platformValue { margin-left: 20px; white-space: normal; }\n");
    queryResultString.append("</style>\n");

    queryResultString.append("</head><body><div class=\"queryContent\">\n");

    //FIXME: This isn't safe because we don't know which context is bound
    if (gl.initializeOpenGLFunctions())
    {
        addSectionHeader(queryResultString, "OpenGL");

        addPlatformHeader(queryResultString, (const char *)gl.glGetString(GL_VENDOR));
        addPlatformValue(queryResultString, "Renderer", (const char *)gl.glGetString(GL_RENDERER));
        addPlatformValue(queryResultString, "GL Version", (const char *)gl.glGetString(GL_VERSION));
        addPlatformValue(queryResultString, "GLSL Version", (const char *)gl.glGetString(GL_SHADING_LANGUAGE_VERSION));
    }

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

    for (OpenCLDeviceInfo &deviceInfo: enumerateOpenCLDevices())
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

        QString deviceVersionString = deviceInfo.getDeviceInfoString(CL_DEVICE_VERSION);
        addPlatformValue(queryResultString, QString(), deviceVersionString);

        cl_uint nvComputeMajor = deviceInfo.getDeviceInfo<cl_uint>(CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV);
        cl_uint nvComputeMinor = deviceInfo.getDeviceInfo<cl_uint>(CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV);;

        if (nvComputeMajor != 0)
            addPlatformValue(queryResultString, "NVIDIA Compute Capability", QStringLiteral("%1.%2").arg(nvComputeMajor).arg(nvComputeMinor));

        cl_uint nvWarpSize = deviceInfo.getDeviceInfo<cl_uint>(CL_DEVICE_WARP_SIZE_NV);

        if (nvWarpSize != 0)
            addPlatformValue(queryResultString, "NVIDIA Warp Size", QString::number(nvWarpSize));

        cl_uint maxComputeUnits = deviceInfo.getDeviceInfo<cl_uint>(CL_DEVICE_MAX_COMPUTE_UNITS);
        addPlatformValue(queryResultString, "Compute Units", QString::number(maxComputeUnits));

        {
            cl_ulong byteSize = deviceInfo.getDeviceInfo<cl_ulong>(CL_DEVICE_GLOBAL_MEM_SIZE);
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

            addPlatformValue(queryResultString, "Global Memory Size", format.arg(byteSize));
        }

        cl_bool hostUnifiedMemory = deviceInfo.getDeviceInfo<cl_bool>(CL_DEVICE_HOST_UNIFIED_MEMORY);
        addPlatformValue(queryResultString, "Host Unified Memory", hostUnifiedMemory ? "Yes" : "No");
    }

    queryResultString.append("</div>\n");
    queryResultString.append("</body>\n");

    ui->queryOutput->setText(queryResultString);
}

SystemInfoDialog::~SystemInfoDialog()
{
}
