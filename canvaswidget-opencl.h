#ifndef CANVASWIDGETOPENCL_H
#define CANVASWIDGETOPENCL_H

#include <list>
#include <QString>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

void _check_cl_error(const char *file, int line, cl_int err);
#define check_cl_error(err) _check_cl_error(__FILE__,__LINE__,err)

#define CL_DIM1(a) {(size_t)(a)}
#define CL_DIM2(a, b) {(size_t)(a), (size_t)(b)}
#define CL_DIM3(a, b, c) {(size_t)(a), (size_t)(b), (size_t)(c)}

class OpenCLDeviceInfo
{
public:
  OpenCLDeviceInfo(cl_device_id device);
  ~OpenCLDeviceInfo();

  cl_platform_id platform;
  cl_device_id   device;

  const QString  &getDeviceName();
  const QString  &getPlatformName();
  cl_device_type  getType();
  template <typename T> T getDeviceInfo(cl_device_info info) const
  {
      T value;
      clGetDeviceInfo(device, info, sizeof(T), &value, nullptr);
      return value;
  }

private:
  QString deviceName;
  QString platformName;
};

std::list<OpenCLDeviceInfo> enumerateOpenCLDevices();

class SharedOpenCL
{
public:
    static SharedOpenCL *getSharedOpenCL();
    static SharedOpenCL *getSharedOpenCLMaybe();

    cl_platform_id platform;
    cl_device_id   device;
    cl_device_type deviceType;

    cl_context ctx;
    cl_command_queue cmdQueue;

    cl_kernel circleKernel;
    cl_kernel fillKernel;
    cl_kernel floatToU8;

    cl_kernel blendKernel_over;
    cl_kernel blendKernel_multiply;
    cl_kernel blendKernel_colorDodge;
    cl_kernel blendKernel_colorBurn;
    cl_kernel blendKernel_screen;

    cl_kernel mypaintDabKernel;
    cl_kernel mypaintGetColorKernelPart1;
    cl_kernel mypaintGetColorKernelPart2;

    cl_kernel paintKernel_fillFloats;
    cl_kernel paintKernel_maskCircle;
    cl_kernel paintKernel_applyMaskTile;

    bool gl_sharing;
private:
    SharedOpenCL();
};

#endif // CANVASWIDGETOPENCL_H
