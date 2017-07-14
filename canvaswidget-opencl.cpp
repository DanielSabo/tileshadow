#include "canvaswidget-opencl.h"
#include "canvastile.h"
#include "opencldeviceinfo.h"
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

SharedOpenCL *SharedOpenCL::getSharedOpenCL()
{
    if (!singleton)
        singleton = new SharedOpenCL();
    return singleton;
}

SharedOpenCL *SharedOpenCL::getSharedOpenCLMaybe()
{
    return singleton;
}

static QByteArray checkedFileRead(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Couldn't open " << path;
        return {};
    }

    QByteArray source = file.readAll();
    if (source.isNull())
    {
        qWarning() << "Couldn't read " << path;
        return {};
    }

    return source;
}

static QByteArray kernelFromTemplate(const QString &path)
{
    QByteArray source = checkedFileRead(path);
    if (source.isNull())
    {
        qWarning() << "Kernel template compile failed " << path;
        return {};
    }
    source.replace("\r\n", "\n");

    const QByteArray PRAGMA_TEMPLATE = QByteArray("#pragma template");
    const QByteArray PRAGMA_TEMPLATE_BODY = QByteArray("#pragma template_body");

    // Split at the template boundary
    int idx = source.indexOf(PRAGMA_TEMPLATE_BODY);
    if (idx == -1)
    {
        qWarning() << "Kernel template compile failed, no body, " << path;
        return {};
    }
    QByteArray sourceHeader = source.left(idx);
    QByteArray sourceBody = source.mid(idx + PRAGMA_TEMPLATE_BODY.size() + 1);
    idx = 0;

    QMap<QByteArray, QMap<QByteArray, QByteArray>> substitutionMap;
    QList<QByteArray> substitutionOrder;

    while (true)
    {
        idx = sourceHeader.indexOf(PRAGMA_TEMPLATE, idx);
        if (idx == -1)
            break;
        int idxNewline = sourceHeader.indexOf("\n", idx);
        idx += PRAGMA_TEMPLATE.size();
        auto prag_args = sourceHeader.mid(idx, idxNewline - idx).split(' ');
        prag_args.removeAll(QByteArray(""));

        if (prag_args.size() == 1)
            prag_args.push_back(QByteArray(""));

        if (prag_args.size() == 2)
        {
            int endIdx = sourceHeader.indexOf(PRAGMA_TEMPLATE, idxNewline + 1);
            QByteArray subValue = sourceHeader.mid(idxNewline + 1, endIdx - (idxNewline + 1));
            substitutionMap[prag_args[0]][prag_args[1]] = subValue;
            if (!prag_args[0].endsWith("_ARGS") && !substitutionOrder.contains(prag_args[0]))
                substitutionOrder.push_back(prag_args[0]);
        }
        idx = idxNewline;
    }

    auto doSub = [&](QByteArray target, QByteArray const subName, QByteArray const subInstance) -> QByteArray
    {
        target.replace(QByteArray("#pragma template ") + subName + "\n",
                       substitutionMap[subName][subInstance]);

        return target;
    };

    QList<QByteArray> processedKernels;
    QList<QByteArray> inputKernels;
    inputKernels.push_back(sourceBody);
    while (!substitutionOrder.empty())
    {
        QByteArray subName = substitutionOrder.takeFirst();
        for (auto const &subInstance: substitutionMap[subName].keys())
        {
            for (auto const &inKernel: inputKernels)
            {
                QByteArray subResult = doSub(inKernel, subName, subInstance);
                QByteArray argsName = subName + "_ARGS";

                if (!subInstance.isEmpty())
                    subResult.replace("FUNCTION_SUFFIX", QByteArray("_") + subInstance + "FUNCTION_SUFFIX");

                if (substitutionMap.contains(argsName))
                {
                    if (substitutionMap[argsName].contains(subInstance))
                        subResult = doSub(subResult, argsName, subInstance);
                    else
                        qWarning() << "Missing argument substitution:" << argsName << "has no instance for" << subInstance;
                }

                processedKernels.push_back(subResult);
            }
        }

        std::swap(inputKernels, processedKernels);
        processedKernels.clear();
    }

    for (auto &inKernel: inputKernels)
        inKernel.replace("FUNCTION_SUFFIX", "");

    QByteArray result = inputKernels.join();
//    qDebug() << qPrintable(result);

    return result;
}

static cl_program compileBytes(SharedOpenCL *cl, const QByteArray &source, const QString &options = "")
{
    QByteArray options_bytes = options.toUtf8();
    cl_int err = CL_SUCCESS;

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
        {
            QStringList errorLines = errorStr.split("\n");
            for (auto &line: errorLines)
                line.prepend("\t");
            errorStr = errorLines.join("\n");
            cout << qPrintable(errorStr) << endl;
        }
        else if (err != CL_SUCCESS)
            qWarning() << "No log data";
    }

    return prog;
}

static cl_program compileFile(SharedOpenCL *cl, const QString &path, const QString &options = "")
{
    QByteArray source = checkedFileRead(path);
    if (source.isNull())
    {
        qWarning() << "CL Program compile failed " << path;
        return {};
    }

    cout << "Compiling " << qPrintable(path) << endl;

    return compileBytes(cl, source, options);
}

static cl_program compileTemplate(SharedOpenCL *cl, QString path, const QString &options = "")
{
    QByteArray templateSource = kernelFromTemplate(path);
    QByteArray headerSource = checkedFileRead(path.replace(QStringLiteral("-template.cl"), QStringLiteral(".cl")));
    if (headerSource.isNull())
    {
        qWarning() << "Failed to read template header " << path;
        return {};
    }

    cout << "Compiling " << qPrintable(path) << " (template)" << endl;

    return compileBytes(cl, headerSource + templateSource, options);
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

    for (cl_platform_id platform: platforms)
    {
        OpenCLPlatformInfo platformInfo(platform);

        if (platformInfo.hasExtension("cl_khr_gl_sharing"))
        {
            cl_int err = CL_SUCCESS;
            cl_device_id device = 0;
            size_t resultSize = 0;

            auto clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)platformInfo.getExtensionFunction("clGetGLContextInfoKHR");

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

            err = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(device), &device, &resultSize);
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
#elif defined(Q_OS_MAC) && 0 /* Disable sharing on OSX due to instability */
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

    OpenCLDeviceInfo deviceInfo;

    platform = 0;
    device = 0;
    deviceType = 0;
    ctx = nullptr;
    cmdQueue = nullptr;
    gl_sharing = false;

    cl_command_queue_properties command_queue_flags = 0;

    cl_device_type selectedDeviceType = CL_DEVICE_TYPE_CPU;

    QSettings appSettings;
    if (appSettings.contains("OpenCL/Device"))
    {
        bool ok = false;
        selectedDeviceType = appSettings.value("OpenCL/Device").toString().toInt(&ok);
        if (!ok)
            selectedDeviceType = CL_DEVICE_TYPE_CPU;
    }

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
        deviceInfo = OpenCLDeviceInfo(device);
        gl_sharing = true;
    }
    else
    {
        std::list<OpenCLDeviceInfo> deviceInfoList = enumerateOpenCLDevices();
        std::list<OpenCLDeviceInfo>::iterator deviceInfoIter;

        for (deviceInfoIter = deviceInfoList.begin(); deviceInfoIter != deviceInfoList.end(); ++deviceInfoIter)
        {
            if (deviceInfoIter->getType() == selectedDeviceType)
            {
                break;
            }
        }

        /* If we can't find the selected device fall back to whatever is left */
        if (deviceInfoIter == deviceInfoList.end())
            deviceInfoIter = deviceInfoList.begin();

        if (deviceInfoIter == deviceInfoList.end())
        {
            qWarning() << "No OpenCL devices found";
            exit(1);
        }

        deviceInfo = *deviceInfoIter;
        ctx = clCreateContext(nullptr, 1, &deviceInfo.device, nullptr, nullptr, &err);
    }

    device = deviceInfo.device;
    platform = deviceInfo.platform;
    deviceType = deviceInfo.getType();

    cout << "CL Platform: " << qPrintable(deviceInfo.getPlatformName().simplified()) << endl;
    cout << "CL Device: " << qPrintable(deviceInfo.getDeviceName().simplified()) << endl;
    cout << "CL Sharing: " << (gl_sharing ? "yes" : "no") << endl;

    cmdQueue = clCreateCommandQueue (ctx, device, command_queue_flags, &err);

    /* Compile base kernels */
    QString kernelDefs = QStringLiteral("-cl-denorms-are-zero -cl-no-signed-zeros");
            kernelDefs += QString(" -DTILE_PIXEL_WIDTH=%1").arg((size_t)TILE_PIXEL_WIDTH);
            kernelDefs += QString(" -DTILE_PIXEL_HEIGHT=%1").arg((size_t)TILE_PIXEL_HEIGHT);

    cl_program baseKernelProg = compileFile(this, ":/BaseKernels.cl", kernelDefs);
    if (baseKernelProg)
    {
        circleKernel = buildOrWarn(baseKernelProg, "circle");
        fillKernel = buildOrWarn(baseKernelProg, "fill");
        floatToU8 = buildOrWarn(baseKernelProg, "floatToU8");
        gradientApply = buildOrWarn(baseKernelProg, "gradientApply");
        colorMask = buildOrWarn(baseKernelProg, "tileColorMask");
        matrixApply = buildOrWarn(baseKernelProg, "matrixApply");
        blendKernel_over = buildOrWarn(baseKernelProg, "tileSVGOver");
        blendKernel_multiply = buildOrWarn(baseKernelProg, "tileSVGMultipy");
        blendKernel_colorDodge = buildOrWarn(baseKernelProg, "tileSVGMColorDodge");
        blendKernel_colorBurn = buildOrWarn(baseKernelProg, "tileSVGColorBurn");
        blendKernel_screen = buildOrWarn(baseKernelProg, "tileSVGScreen");
        blendKernel_hue = buildOrWarn(baseKernelProg, "tileSVGHue");
        blendKernel_saturation = buildOrWarn(baseKernelProg, "tileSVGSaturation");
        blendKernel_color = buildOrWarn(baseKernelProg, "tileSVGColor");
        blendKernel_luminosity = buildOrWarn(baseKernelProg, "tileSVGLuminosity");
        blendKernel_dstOut = buildOrWarn(baseKernelProg, "tileSVGDstOut");
        blendKernel_dstIn = buildOrWarn(baseKernelProg, "tileSVGDstIn");
        blendKernel_srcAtop = buildOrWarn(baseKernelProg, "tileSVGSrcAtop");
        blendKernel_dstAtop = buildOrWarn(baseKernelProg, "tileSVGDstAtop");

        clReleaseProgram (baseKernelProg);
    }

    cl_program myPaintKernelsProg = compileTemplate(this, ":/MyPaintKernels-template.cl", kernelDefs);
    if (myPaintKernelsProg)
    {
        mypaintDabKernel = buildOrWarn(myPaintKernelsProg, "mypaint_dab");
        mypaintDabLockedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_dab_locked");
        mypaintDabIsolateKernel = buildOrWarn(myPaintKernelsProg, "mypaint_dab_isolate");
        mypaintMicroDabKernel = buildOrWarn(myPaintKernelsProg, "mypaint_micro");
        mypaintMicroDabLockedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_micro_locked");
        mypaintMicroDabIsolateKernel = buildOrWarn(myPaintKernelsProg, "mypaint_micro_isolate");
        mypaintMaskDabKernel = buildOrWarn(myPaintKernelsProg, "mypaint_mask");
        mypaintMaskDabLockedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_mask_locked");
        mypaintMaskDabIsolateKernel = buildOrWarn(myPaintKernelsProg, "mypaint_mask_isolate");
        mypaintDabTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_dab_textured");
        mypaintDabLockedTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_dab_locked_textured");
        mypaintDabIsolateTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_dab_isolate_textured");
        mypaintMicroDabTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_micro_textured");
        mypaintMicroDabLockedTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_micro_locked_textured");
        mypaintMicroDabIsolateTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_micro_isolate_textured");
        mypaintMaskDabTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_mask_textured");
        mypaintMaskDabLockedTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_mask_locked_textured");
        mypaintMaskDabIsolateTexturedKernel = buildOrWarn(myPaintKernelsProg, "mypaint_mask_isolate_textured");
        mypaintGetColorKernelPart1 = buildOrWarn(myPaintKernelsProg, "mypaint_color_query_part1");
        mypaintGetColorKernelEmptyPart1 = buildOrWarn(myPaintKernelsProg, "mypaint_color_query_empty_part1");
        mypaintGetColorKernelPart2 = buildOrWarn(myPaintKernelsProg, "mypaint_color_query_part2");

        clReleaseProgram (myPaintKernelsProg);
    }

    cl_program paintKernelsProg = compileFile(this, ":/PaintKernels.cl", kernelDefs);
    if (paintKernelsProg)
    {
        paintKernel_fillFloats = buildOrWarn(paintKernelsProg, "fillFloats");
        paintKernel_maskCircle = buildOrWarn(paintKernelsProg, "maskCircle");
        paintKernel_applyMaskTile = buildOrWarn(paintKernelsProg, "applyMaskTile");

        clReleaseProgram(paintKernelsProg);
    }

    cl_program patternKernelsProg = compileFile(this, ":/PatternKernels.cl", kernelDefs);
    if (patternKernelsProg)
    {
        patternFill_fillCircle = buildOrWarn(patternKernelsProg, "patternFillCircle");

        clReleaseProgram(patternKernelsProg);
    }


}
