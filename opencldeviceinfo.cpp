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
        deviceName = getDeviceInfoString(CL_DEVICE_NAME);

    return deviceName;
}

const QString &OpenCLDeviceInfo::getPlatformName()
{
    if (platformName.isEmpty())
        platformName = getPlatformInfoString(CL_PLATFORM_NAME);

    return platformName;
}

cl_device_type OpenCLDeviceInfo::getType()
{
    cl_device_type devType;
    clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(devType), &devType, nullptr);

    return devType;
}

QString OpenCLDeviceInfo::getDeviceInfoString(cl_device_info info) const
{
    size_t size;
    if (clGetDeviceInfo(device, info, 0, nullptr, &size) != CL_SUCCESS)
        return {};
    QByteArray bytes(size, '\0');
    if (clGetDeviceInfo(device, info, bytes.size(), bytes.data(), nullptr) != CL_SUCCESS)
        return {};
    return bytes;
}

QString OpenCLDeviceInfo::getPlatformInfoString(cl_device_info info) const
{
    size_t size;
    if (clGetPlatformInfo(platform, info, 0, nullptr, &size) != CL_SUCCESS)
        return {};
    QByteArray bytes(size, '\0');
    if (clGetPlatformInfo(platform, info, bytes.size(), bytes.data(), nullptr) != CL_SUCCESS)
        return {};
    return bytes;
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
