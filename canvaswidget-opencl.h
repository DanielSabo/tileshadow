#ifndef CANVASWIDGETOPENCL_H
#define CANVASWIDGETOPENCL_H

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

template <typename T> static inline cl_int clSetKernelArg(cl_kernel kernel, cl_uint idx, T const &value)
{
    return clSetKernelArg(kernel, idx, sizeof(T), &value);
}

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
    cl_kernel gradientApply;
    cl_kernel colorMask;

    cl_kernel blendKernel_over;
    cl_kernel blendKernel_multiply;
    cl_kernel blendKernel_colorDodge;
    cl_kernel blendKernel_colorBurn;
    cl_kernel blendKernel_screen;
    cl_kernel blendKernel_hue;
    cl_kernel blendKernel_saturation;
    cl_kernel blendKernel_color;
    cl_kernel blendKernel_luminosity;
    cl_kernel blendKernel_dstOut;
    cl_kernel blendKernel_dstIn;
    cl_kernel blendKernel_srcAtop;
    cl_kernel blendKernel_dstAtop;

    cl_kernel mypaintDabKernel;
    cl_kernel mypaintDabLockedKernel;
    cl_kernel mypaintMicroDabKernel;
    cl_kernel mypaintMicroDabLockedKernel;
    cl_kernel mypaintMaskDabKernel;
    cl_kernel mypaintMaskDabLockedKernel;
    cl_kernel mypaintDabTexturedKernel;
    cl_kernel mypaintDabLockedTexturedKernel;
    cl_kernel mypaintMicroDabTexturedKernel;
    cl_kernel mypaintMicroDabLockedTexturedKernel;
    cl_kernel mypaintMaskDabTexturedKernel;
    cl_kernel mypaintMaskDabLockedTexturedKernel;
    cl_kernel mypaintGetColorKernelPart1;
    cl_kernel mypaintGetColorKernelEmptyPart1;
    cl_kernel mypaintGetColorKernelPart2;

    cl_kernel paintKernel_fillFloats;
    cl_kernel paintKernel_maskCircle;
    cl_kernel paintKernel_applyMaskTile;

    cl_kernel patternFill_fillCircle;

    bool gl_sharing;
private:
    SharedOpenCL();
};

namespace cl {
    static inline cl_mem createImage2D(SharedOpenCL          *opencl,
                                       cl_mem_flags           flags,
                                       const cl_image_format *fmt,
                                       size_t                 width,
                                       size_t                 height,
                                       size_t                 row_pitch,
                                       void                  *host_ptr,
                                       cl_int                *errcode_ret)
    {
#if defined(__APPLE__) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8)
        cl_image_desc image_desc = {CL_MEM_OBJECT_IMAGE2D, width, height, 0, 0, row_pitch};
        return clCreateImage(opencl->ctx, flags, fmt, &image_desc, host_ptr, errcode_ret);
#else
        return clCreateImage2D(opencl->ctx, flags, fmt, width, height, row_pitch, host_ptr, errcode_ret);
#endif
    }

    static inline cl_mem createImage3D(SharedOpenCL          *opencl,
                                       cl_mem_flags           flags,
                                       const cl_image_format *fmt,
                                       size_t                 width,
                                       size_t                 height,
                                       size_t                 depth,
                                       size_t                 row_pitch,
                                       size_t                 slice_pitch,
                                       void                  *host_ptr,
                                       cl_int                *errcode_ret)
    {
#if defined(__APPLE__) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_8)
        cl_image_desc image_desc = {CL_MEM_OBJECT_IMAGE3D, width, height, depth, 0, row_pitch, slice_pitch};
        return clCreateImage(opencl->ctx, flags, fmt, &image_desc, host_ptr, errcode_ret);
#else
        return clCreateImage3D(opencl->ctx, flags, fmt, width, height, depth, row_pitch, slice_pitch, host_ptr, errcode_ret);
#endif
    }
}

#endif // CANVASWIDGETOPENCL_H
