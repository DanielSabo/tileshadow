#include "canvaswidget-opencl.h"
#include <string.h>
#include <iostream>
#include <vector>
#include <QFile>
#include <QDebug>
#include <QStringList>
#include <QSettings>
#include <QCoreApplication>

#if defined(Q_OS_WIN32)
#include <windows.h>
#elif defined(Q_OS_MAC)
#include <OpenGL/OpenGL.h>
#include <OpenCL/cl_gl_ext.h>
#elif defined(Q_OS_UNIX)
#include <GL/glx.h>
#endif

using namespace std;

static SharedOpenCL* singleton = nullptr;

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
    deviceName(),
    platformName()
{
    clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(platform), &platform, nullptr);
}

OpenCLDeviceInfo::~OpenCLDeviceInfo()
{
}

const QString &OpenCLDeviceInfo::getDeviceName()
{
    if (deviceName.isEmpty())
    {
        size_t deviceNameSize = 0;
        clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &deviceNameSize);

        vector<char> deviceNameBuffer(deviceNameSize);
        clGetDeviceInfo(device, CL_DEVICE_NAME, deviceNameSize, deviceNameBuffer.data(), nullptr);

        deviceName = QString(deviceNameBuffer.data());
    }

    return deviceName;
}

const QString &OpenCLDeviceInfo::getPlatformName()
{
    if (platformName.isEmpty())
    {
        size_t platformNameSize = 0;
        clGetPlatformInfo(platform, CL_PLATFORM_NAME, 0, nullptr, &platformNameSize);

        vector<char> platformNameBuffer(platformNameSize);
        clGetPlatformInfo(platform, CL_PLATFORM_NAME, platformNameSize, platformNameBuffer.data(), nullptr);

        platformName = QString(platformNameBuffer.data());
    }

    return platformName;
}

cl_device_type OpenCLDeviceInfo::getType()
{
    cl_device_type devType;
    clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(devType), &devType, nullptr);

    return devType;
}

std::list<OpenCLDeviceInfo> enumerateOpenCLDevices()
{
    std::list<OpenCLDeviceInfo> deviceList;

    cl_uint numPlatforms;
    clGetPlatformIDs (0, nullptr, &numPlatforms);

    vector<cl_platform_id> platforms(numPlatforms);
    clGetPlatformIDs (numPlatforms, platforms.data(), nullptr);

    for (cl_platform_id platform: platforms)
    {
        cl_uint numDevices;
        clGetDeviceIDs (platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &numDevices);
        vector<cl_device_id> devices(numDevices);
        clGetDeviceIDs (platform, CL_DEVICE_TYPE_ALL, numDevices, devices.data(), nullptr);

        for (cl_device_id device: devices)
            deviceList.push_back(OpenCLDeviceInfo(device));
    }

    return deviceList;
}

SharedOpenCL *SharedOpenCL::getSharedOpenCL()
{
    if (!singleton)
        singleton = new SharedOpenCL();
    return singleton;
}

static cl_program compileFile (SharedOpenCL *cl, const QString &path, const QString &options = "")
{
    QByteArray options_bytes = options.toUtf8();
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

    err = clBuildProgram (prog, 0, nullptr, options_bytes.constData(), nullptr, nullptr);

    if (err != CL_SUCCESS)
        qWarning() << "CL Program compile failed, build error " << err;

    size_t error_len = 0;
    clGetProgramBuildInfo (prog, cl->device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &error_len);
    if (error_len)
    {
        vector<char> error_buf(error_len);
        error_buf[0] = '\0';

        clGetProgramBuildInfo (prog, cl->device, CL_PROGRAM_BUILD_LOG, error_len, error_buf.data(), nullptr);
        QString errorStr = QString(error_buf.data()).trimmed();
        if (errorStr.size() > 0)
            qWarning() << errorStr;
        else if (err != CL_SUCCESS)
            qWarning() << "No log data";
    }

    cout << "Compiled " << path.toUtf8().constData() << endl;

    return prog;
}

#ifndef cl_khr_gl_sharing
#define cl_khr_gl_sharing 1
typedef cl_uint cl_gl_context_info;

#define CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR -1000
#define CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR 0x2006
#define CL_DEVICES_FOR_GL_CONTEXT_KHR 0x2007
#define CL_GL_CONTEXT_KHR 0x2008
#define CL_EGL_DISPLAY_KHR 0x2009
#define CL_GLX_DISPLAY_KHR 0x200A
#define CL_WGL_HDC_KHR 0x200B
#define CL_CGL_SHAREGROUP_KHR 0x200C

typedef CL_API_ENTRY cl_int
    (CL_API_CALL *clGetGLContextInfoKHR_fn)(
        const cl_context_properties *,
        cl_gl_context_info,
        size_t,
        void *,
        size_t *);
#endif /* cl_khr_gl_sharing */

static cl_context createSharedContext()
{
#if defined(Q_OS_WIN32) || (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    cl_context result;

    cl_uint numPlatforms;
    clGetPlatformIDs (0, nullptr, &numPlatforms);
    vector<cl_platform_id> platforms(numPlatforms);
    clGetPlatformIDs (numPlatforms, platforms.data(), nullptr);

    clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddress ("clGetGLContextInfoKHR");

    if (!clGetGLContextInfoKHR)
    {
        qWarning() << "Could not find shared devices becuase clGetGLContextInfoKHR is unavailable.";
        return 0;
    }

    for (cl_platform_id platform: platforms)
    {
        size_t resultSize;
        clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, 0, nullptr, &resultSize);
        vector<char> resultStr(resultSize);
        clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, resultSize, resultStr.data(), nullptr);
        QStringList platform_extensions = QString(resultStr.data()).split(' ');

        if (platform_extensions.indexOf(QString("cl_khr_gl_sharing")) != -1)
        {
            cl_int err = CL_SUCCESS;
            cl_device_id device = 0;
            size_t resultSize = 0;

#if defined(Q_OS_WIN32)
            cl_context_properties properties[] = {
                CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
                CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
                CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
                0, 0,
            };
#elif defined(Q_OS_UNIX)
            cl_context_properties properties[] = {
                CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
                CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
                CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
                0, 0,
            };
#else
            #error Missing properties
#endif

            err = clGetGLContextInfoKHR (properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(device), &device, &resultSize);
            check_cl_error(err);

            if (err == CL_SUCCESS && resultSize > 0 && device)
            {
                OpenCLDeviceInfo deviceInfo(device);
                if (deviceInfo.getType() == CL_DEVICE_TYPE_GPU)
                {
                    result = clCreateContext(properties, 1, &device, nullptr, nullptr, &err);
                    if (err == CL_SUCCESS)
                        return result;
                    else
                        check_cl_error(err);
                }
            }
            else
            {
                check_cl_error(err);
            }
        }
    }
#elif defined(Q_OS_MAC)
    /* FIXME: This assumes there will only be the Apple platform and that it will
     * inherently support sharing.
     */
    cl_context result;
    cl_int err = CL_SUCCESS;

    cl_context_properties properties[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()),
        0, 0,
    };

    result = clCreateContext(properties, 0, nullptr, nullptr, nullptr, &err);
    if (err == CL_SUCCESS && result)
        return result;
    else
        check_cl_error(err);
#endif
    return 0;
}

static cl_kernel buildOrWarn(cl_program prog, const char *name)
{
    cl_int err = CL_SUCCESS;
    cl_kernel kernel = clCreateKernel (prog, name, &err);

    if (err != CL_SUCCESS)
        qWarning() << "Could not find base kernel \"" << name << "\" (" << err << ")";

    return kernel;
}

SharedOpenCL::SharedOpenCL()
{
    cl_int err = CL_SUCCESS;

    platform = 0;
    device = 0;
    deviceType = 0;
    ctx = nullptr;
    cmdQueue = nullptr;
    gl_sharing = false;

    cl_command_queue_properties command_queue_flags = 0;

    cl_device_type selectedDeviceType = QSettings().value("OpenCL/Device", QVariant::fromValue<int>(0)).toInt();

    if (selectedDeviceType == 0)
    {
        /* Look for a shared device */
        ctx = createSharedContext();
        if (!ctx)
            selectedDeviceType = CL_DEVICE_TYPE_CPU;
    }

    if (ctx)
    {
        clGetContextInfo(ctx, CL_CONTEXT_DEVICES, sizeof(device), &device, nullptr);
        gl_sharing = true;
    }
    else
    {
        std::list<OpenCLDeviceInfo> deviceInfoList = enumerateOpenCLDevices();
        std::list<OpenCLDeviceInfo>::iterator deviceInfoIter;

        /* Then for a CPU device */
        for (deviceInfoIter = deviceInfoList.begin(); deviceInfoIter != deviceInfoList.end(); ++deviceInfoIter)
        {
            if (deviceInfoIter->getType() == selectedDeviceType)
            {
                break;
            }
        }

        /* Finally fall back to whatever non-shared device is left */
        if (deviceInfoIter == deviceInfoList.end())
            deviceInfoIter = deviceInfoList.begin();

        if (deviceInfoIter == deviceInfoList.end())
        {
            qWarning() << "No OpenCL devices found";
            exit(1);
        }

        /* FIXME: Scope issues */
        device = deviceInfoIter->device;

        ctx = clCreateContext (nullptr, 1, &device, nullptr, nullptr, &err);
    }

    OpenCLDeviceInfo deviceInfo(device);
    device = deviceInfo.device;
    platform = deviceInfo.platform;
    deviceType = deviceInfo.getType();

    cout << "CL Platform: " << deviceInfo.getPlatformName().toUtf8().constData() << endl;
    cout << "CL Device: " << deviceInfo.getDeviceName().toUtf8().constData() << endl;
    cout << "CL Sharing: " << (gl_sharing ? "yes" : "no") << endl;

    cmdQueue = clCreateCommandQueue (ctx, device, command_queue_flags, &err);

    /* Compile base kernels */

    cl_program baseKernelProg = compileFile(this, ":/BaseKernels.cl");

    if (baseKernelProg)
    {
        circleKernel = buildOrWarn(baseKernelProg, "circle");
        fillKernel = buildOrWarn(baseKernelProg, "fill");
        blendKernel_over = buildOrWarn(baseKernelProg, "tileSVGOver");
        blendKernel_multiply = buildOrWarn(baseKernelProg, "tileSVGMultipy");
        blendKernel_colorDodge = buildOrWarn(baseKernelProg, "tileSVGMColorDodge");
        blendKernel_colorBurn = buildOrWarn(baseKernelProg, "tileSVGColorBurn");
        blendKernel_screen = buildOrWarn(baseKernelProg, "tileSVGScreen");

        clReleaseProgram (baseKernelProg);
    }

    cl_program myPaintKernelsProg = compileFile(this, ":/MyPaintKernels.cl");
    if (myPaintKernelsProg)
    {
        mypaintDabKernel = buildOrWarn(myPaintKernelsProg, "mypaint_dab");
        mypaintGetColorKernelPart1 = buildOrWarn(myPaintKernelsProg, "mypaint_color_query_part1");
        mypaintGetColorKernelPart2 = buildOrWarn(myPaintKernelsProg, "mypaint_color_query_part2");

        clReleaseProgram (myPaintKernelsProg);
    }
}
