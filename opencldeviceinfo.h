#ifndef OPENCLDEVICEINFO_H
#define OPENCLDEVICEINFO_H

#include <list>
#include <QString>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

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

#endif // OPENCLDEVICEINFO_H