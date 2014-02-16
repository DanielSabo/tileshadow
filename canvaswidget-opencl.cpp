#include "canvaswidget-opencl.h"
#include <iostream>
#include <QFile>
#include <QDebug>

using namespace std;

static SharedOpenCL* singleton = NULL;

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
    char result_buffer[1024];
    cl_int err = CL_SUCCESS;

    platform = 0;
    device = 0;
    ctx = NULL;
    cmdQueue = NULL;

    cl_command_queue_properties command_queue_flags = 0;

    err = clGetPlatformIDs(1, &platform, 0);
    // FIXME: Change this back to default
    // err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_DEFAULT, 1, &device, NULL);
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);

    clGetPlatformInfo(platform, CL_PLATFORM_NAME, sizeof(result_buffer), result_buffer, NULL);
    cout << "CL Platform: " << result_buffer << endl;
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(result_buffer), result_buffer, NULL);
    cout << "CL Device: " << result_buffer << endl;

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
