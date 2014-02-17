#include "canvaswidget-opencl.h"
#include <string.h>
#include <iostream>
#include <QFile>
#include <QDebug>

using namespace std;

static SharedOpenCL* singleton = NULL;

OpenCLDeviceInfo::OpenCLDeviceInfo(cl_device_id d) :
    platform(0),
    device(d),
    deviceName(0),
    platformName(0),
    sharing(false)
{
    clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(platform), &platform, NULL);
}

OpenCLDeviceInfo::OpenCLDeviceInfo(OpenCLDeviceInfo const &other)
{
    if (other.deviceName)
        deviceName = strdup(other.deviceName);
    else
        deviceName = 0;

    if (other.platformName)
        platformName = strdup(other.platformName);
    else
        platformName = 0;

    platform = other.platform;
    device = other.device;
    sharing = other.sharing;
}

OpenCLDeviceInfo::~OpenCLDeviceInfo()
{
    if (deviceName)
        delete[] deviceName;

    if (platformName)
        delete[] platformName;
}

const char *OpenCLDeviceInfo::getDeviceName()
{
    if (!deviceName)
    {
        size_t deviceNameSize = 0;
        clGetDeviceInfo(device, CL_DEVICE_NAME, 0, NULL, &deviceNameSize);

        deviceName = new char[deviceNameSize];
        clGetDeviceInfo(device, CL_DEVICE_NAME, deviceNameSize, deviceName, NULL);
    }

    return deviceName;
}

const char *OpenCLDeviceInfo::getPlatformName()
{
    if (!platformName)
    {
        size_t platformNameSize = 0;
        clGetPlatformInfo(platform, CL_PLATFORM_NAME, 0, NULL, &platformNameSize);

        platformName = new char[platformNameSize];
        clGetPlatformInfo(platform, CL_PLATFORM_NAME, platformNameSize, platformName, NULL);
    }

    return platformName;
}

std::list<OpenCLDeviceInfo> enumerateOpenCLDevices()
{
    std::list<OpenCLDeviceInfo> deviceList;

    cl_uint i, j;

    cl_uint numPlatforms;
    clGetPlatformIDs (0, NULL, &numPlatforms);
    cl_platform_id platforms[numPlatforms];
    clGetPlatformIDs (numPlatforms, platforms, NULL);

    for (i = 0; i < numPlatforms; ++i)
    {
        cl_uint numDevices;
        clGetDeviceIDs (platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
        cl_device_id devices[numDevices];
        clGetDeviceIDs (platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);

        for (j = 0; j < numDevices; ++j)
            deviceList.push_back(OpenCLDeviceInfo(devices[j]));
    }

    return deviceList;
}

SharedOpenCL *SharedOpenCL::getSharedOpenCL()
{
    if (!singleton)
        singleton = new SharedOpenCL();
    return singleton;
}

static cl_program compileFile (SharedOpenCL *cl, const QString &path)
{
    cl_int err = CL_SUCCESS;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "CL Program compile failed, couldn't open " << path;
        return 0;
    }

    QByteArray source = file.readAll();
    if (source.isNull())
    {
        qWarning() << "CL Program compile failed, couldn't read " << path;
        return 0;
    }

    const char *source_str = source.data();
    size_t source_len = source.size();

    cl_program prog = clCreateProgramWithSource (cl->ctx, 1, &source_str, &source_len, &err);

    if (err != CL_SUCCESS)
    {
        qWarning() << "CL Program compile failed, create error " << err;
        return 0;
    }

    err = clBuildProgram (prog, 0, NULL, NULL, NULL, NULL);

    if (err != CL_SUCCESS)
    {
        size_t error_len = 0;
        qWarning() << "CL Program compile failed, build error " << err;

        clGetProgramBuildInfo (prog, cl->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &error_len);
        if (error_len)
        {
            char *error_buf = new char[error_len];
            error_buf[0] = '\0';

            clGetProgramBuildInfo (prog, cl->device, CL_PROGRAM_BUILD_LOG, error_len, error_buf, NULL);
            qWarning() << error_buf;
        }
        else
        {
            qWarning() << "No log data";
        }
        return 0;
    }

    // errcode = gegl_clGetProgramBuildInfo (prog, cl->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &s);

    cout << "Compiled " << path.toUtf8().constData() << endl;

    return prog;
}

SharedOpenCL::SharedOpenCL()
{
    cl_int err = CL_SUCCESS;

    platform = 0;
    device = 0;
    ctx = NULL;
    cmdQueue = NULL;

    cl_command_queue_properties command_queue_flags = 0;

    err = clGetPlatformIDs(1, &platform, 0);
    if (err != CL_SUCCESS)
    {
        qWarning() << "No OpenCL platforms found";
        exit(1);
    }

    // FIXME: Use the default device
    // err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_DEFAULT, 1, &device, NULL);
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    if (err != CL_SUCCESS)
    {
        qWarning() << "No OpenCL devices found";
        exit(1);
    }

    OpenCLDeviceInfo deviceInfo = OpenCLDeviceInfo(device);

    cout << "CL Platform: " << deviceInfo.getPlatformName() << endl;
    cout << "CL Device: " << deviceInfo.getDeviceName() << endl;

    ctx = clCreateContext (NULL, 1, &device, NULL, NULL, &err);
    cmdQueue = clCreateCommandQueue (ctx, device, command_queue_flags, &err);


    /* Compile base kernels */

    cl_program baseKernelProg = compileFile(this, ":/BaseKernels.cl");

    if (baseKernelProg)
    {
        circleKernel = clCreateKernel (baseKernelProg, "circle", &err);

        if (err != CL_SUCCESS)
        {
            qWarning() << "Could not find base kernel \"" << "circle" << "\" (" << err << ")";
        }

        fillKernel = clCreateKernel (baseKernelProg, "fill", &err);;

        if (err != CL_SUCCESS)
        {
            qWarning() << "Could not find base kernel \"" << "fill" << "\" (" << err << ")";
        }

        clReleaseProgram (baseKernelProg);
    }
}
