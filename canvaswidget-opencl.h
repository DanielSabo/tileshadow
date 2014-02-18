#ifndef CANVASWIDGETOPENCL_H
#define CANVASWIDGETOPENCL_H

#include <list>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

void _check_cl_error(const char *file, int line, cl_int err);
#define check_cl_error(err) _check_cl_error(__FILE__,__LINE__,err)

class OpenCLDeviceInfo
{
public:
  OpenCLDeviceInfo(cl_device_id device);
  OpenCLDeviceInfo(OpenCLDeviceInfo const &other);
  ~OpenCLDeviceInfo();

  cl_platform_id platform;
  cl_device_id   device;

  const char     *getDeviceName();
  const char     *getPlatformName();
  cl_device_type  getType();

private:
  char *deviceName;
  char *platformName;
};

std::list<OpenCLDeviceInfo> enumerateOpenCLDevices();

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

    bool gl_sharing;
private:
    SharedOpenCL();
};

#endif // CANVASWIDGETOPENCL_H
