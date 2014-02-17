#include "canvaswidget-opencl.h"
#include <string.h>
#include <iostream>
#include <QFile>
#include <QDebug>

using namespace std;

static SharedOpenCL* singleton = NULL;

void _check_cl_error(const char *file, int line, cl_int err) {
    if (err != CL_SUCCESS)
    {
        static const char* strings[] =
        {
            /* Error Codes */
              "success"                         /*  0  */
            , "device not found"                /* -1  */
            , "device not available"            /* -2  */
            , "compiler not available"          /* -3  */
            , "mem object allocation failure"   /* -4  */
            , "out of resources"                /* -5  */
            , "out of host memory"              /* -6  */
            , "profiling info not available"    /* -7  */
            , "mem copy overlap"                /* -8  */
            , "image format mismatch"           /* -9  */
            , "image format not supported"      /* -10 */
            , "build program failure"           /* -11 */
            , "map failure"                     /* -12 */
            , "unknown error"                   /* -13 */
            , "unknown error"                   /* -14 */
            , "unknown error"                   /* -15 */
            , "unknown error"                   /* -16 */
            , "unknown error"                   /* -17 */
            , "unknown error"                   /* -18 */
            , "unknown error"                   /* -19 */
            , "unknown error"                   /* -20 */
            , "unknown error"                   /* -21 */
            , "unknown error"                   /* -22 */
            , "unknown error"                   /* -23 */
            , "unknown error"                   /* -24 */
            , "unknown error"                   /* -25 */
            , "unknown error"                   /* -26 */
            , "unknown error"                   /* -27 */
            , "unknown error"                   /* -28 */
            , "unknown error"                   /* -29 */
            , "invalid value"                   /* -30 */
            , "invalid device type"             /* -31 */
            , "invalid platform"                /* -32 */
            , "invalid device"                  /* -33 */
            , "invalid context"                 /* -34 */
            , "invalid queue properties"        /* -35 */
            , "invalid command queue"           /* -36 */
            , "invalid host ptr"                /* -37 */
            , "invalid mem object"              /* -38 */
            , "invalid image format descriptor" /* -39 */
            , "invalid image size"              /* -40 */
            , "invalid sampler"                 /* -41 */
            , "invalid binary"                  /* -42 */
            , "invalid build options"           /* -43 */
            , "invalid program"                 /* -44 */
            , "invalid program executable"      /* -45 */
            , "invalid kernel name"             /* -46 */
            , "invalid kernel definition"       /* -47 */
            , "invalid kernel"                  /* -48 */
            , "invalid arg index"               /* -49 */
            , "invalid arg value"               /* -50 */
            , "invalid arg size"                /* -51 */
            , "invalid kernel args"             /* -52 */
            , "invalid work dimension"          /* -53 */
            , "invalid work group size"         /* -54 */
            , "invalid work item size"          /* -55 */
            , "invalid global offset"           /* -56 */
            , "invalid event wait list"         /* -57 */
            , "invalid event"                   /* -58 */
            , "invalid operation"               /* -59 */
            , "invalid gl object"               /* -60 */
            , "invalid buffer size"             /* -61 */
            , "invalid mip level"               /* -62 */
            , "invalid global work size"        /* -63 */
        };
        static const int strings_len = sizeof(strings) / sizeof(strings[0]);

        const char *error_str;

        if (-err < 0 || -err >= strings_len)
            error_str = "unknown error";
        else
            error_str = strings[-err];

        qWarning() << "OpenCL Error (" << err << "): " << error_str << " - " << file << ":" << line;
    }
}

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

cl_device_type OpenCLDeviceInfo::getType()
{
    cl_device_type devType;
    clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(devType), &devType, NULL);

    return devType;
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

    std::list<OpenCLDeviceInfo> deviceInfoList = enumerateOpenCLDevices();
    std::list<OpenCLDeviceInfo>::iterator deviceInfoIter;

    for (deviceInfoIter = deviceInfoList.begin(); deviceInfoIter != deviceInfoList.end(); ++deviceInfoIter)
    {
        if (deviceInfoIter->getType() == CL_DEVICE_TYPE_CPU)
        {
            break;
        }
    }

    if (deviceInfoIter == deviceInfoList.end())
    {
        qWarning() << "No OpenCL devices found";
        exit(1);
    }

    OpenCLDeviceInfo deviceInfo = *deviceInfoIter;
    device = deviceInfo.device;
    platform = deviceInfo.platform;

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
