#ifndef CANVASWIDGETOPENCL_H
#define CANVASWIDGETOPENCL_H

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

class SharedOpenCL
{
public:
    static SharedOpenCL *getSharedOpenCL();

    cl_platform_id platform;
    cl_device_id   device;

    cl_context ctx;
    cl_command_queue cmdQueue;

    cl_kernel circleKernel;
    cl_kernel fillKernel;
private:
    SharedOpenCL();
};

#endif // CANVASWIDGETOPENCL_H
