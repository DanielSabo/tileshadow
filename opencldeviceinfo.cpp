#include "opencldeviceinfo.h"
#include <vector>

using namespace std;

OpenCLDeviceInfo::OpenCLDeviceInfo() :
    platform(0),
    device(0)
{
}

OpenCLDeviceInfo::OpenCLDeviceInfo(cl_device_id d) :
    platform(0),
    device(d)
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
