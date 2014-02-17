#ifndef CANVASWIDGETOPENCL_H
#define CANVASWIDGETOPENCL_H

#include <list>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

class OpenCLDeviceInfo
{
public:
  OpenCLDeviceInfo(cl_device_id device);
  OpenCLDeviceInfo(OpenCLDeviceInfo const &other);
  ~OpenCLDeviceInfo();

  cl_platform_id platform;
  cl_device_id   device;

  const char *getDeviceName();
  const char *getPlatformName();
  bool        getSharing();

private:
  char *deviceName;
  char *platformName;
  bool sharing;
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
private:
    SharedOpenCL();
};

#endif // CANVASWIDGETOPENCL_H
