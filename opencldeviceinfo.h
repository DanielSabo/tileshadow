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
  OpenCLDeviceInfo();
  OpenCLDeviceInfo(cl_device_id device);
  ~OpenCLDeviceInfo();

  cl_platform_id platform;
  cl_device_id   device;

  bool isNull() { return !platform || !device; }
  const QString  &getDeviceName();
  const QString  &getPlatformName();
  cl_device_type  getType();

  template <typename T> T getDeviceInfo(cl_device_info info) const
  {
      T value;
      if (CL_SUCCESS != clGetDeviceInfo(device, info, sizeof(T), &value, nullptr))
        return {};
      return value;
  }

  QString getDeviceInfoString(cl_device_info info) const;
  QString getPlatformInfoString(cl_platform_info info) const;

private:
  QString deviceName;
  QString platformName;
};

std::list<OpenCLDeviceInfo> enumerateOpenCLDevices();

#endif // OPENCLDEVICEINFO_H
