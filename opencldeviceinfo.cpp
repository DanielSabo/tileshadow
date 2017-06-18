#include "opencldeviceinfo.h"
#include <vector>
#include <QDebug>

#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8
#define NEVER_USE_OPENCL_1_1
#endif
#endif

using namespace std;

namespace {
QString platformInfoString(cl_platform_id platform, cl_platform_info info)
{
    size_t size;
    if (clGetPlatformInfo(platform, info, 0, nullptr, &size) != CL_SUCCESS)
        return {};
    QByteArray bytes(size, '\0');
    if (clGetPlatformInfo(platform, info, bytes.size(), bytes.data(), nullptr) != CL_SUCCESS)
        return {};
    return bytes;
}
}

OpenCLPlatformInfo::OpenCLPlatformInfo() :
    platform(0),
    isOpenCL1_2(false)
{

}

OpenCLPlatformInfo::OpenCLPlatformInfo(cl_platform_id platform) :
    platform(platform),
    isOpenCL1_2(false)
{
    QStringList version = getPlatformInfoString(CL_PLATFORM_VERSION).split(' ');
    if (version.size() >= 2)
    {
        QStringList numeric = version[1].split('.');
        if (numeric.size() >= 2)
        {
            int major = numeric.at(0).toInt();
            int minor = numeric.at(1).toInt();

            if (major > 1 || (major == 1 && minor >= 2))
                isOpenCL1_2 = true;
        }
    }

#ifdef NEVER_USE_OPENCL_1_1
    if (!isOpenCL1_2)
        qCritical() << "OpenCL platform version < 1.2 but 1.1 functions are disabled";
#endif
}

bool OpenCLPlatformInfo::hasExtension(QString name)
{
    if (extensions.isEmpty())
        extensions = getPlatformInfoString(CL_PLATFORM_EXTENSIONS).split(' ');
    return extensions.contains(name);
}

void *OpenCLPlatformInfo::getExtensionFunction(const char *name)
{
#ifndef NEVER_USE_OPENCL_1_1
    if (!isOpenCL1_2)
        return clGetExtensionFunctionAddress(name);
#endif

    return clGetExtensionFunctionAddressForPlatform(platform, name);
}

QString OpenCLPlatformInfo::getPlatformInfoString(cl_platform_info info) const
{
    return platformInfoString(platform, info);
}

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

QString OpenCLDeviceInfo::getPlatformInfoString(cl_platform_info info) const
{
    return platformInfoString(platform, info);
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
