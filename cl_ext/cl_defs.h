#progma once

#ifndef CL_MINIMUM_OPENCL_VERSION
#define CL_MINIMUM_OPENCL_VERSION 120
#endif // CL_MINIMUM_OPENCL_VERSION

#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 120
#endif // CL_TARGET_OPENCL_VERSION

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/opencl.h>
#include <OpenGL/OpenGL.h>
#define CL_GL_SHARING_EXT "cl_apple_gl_sharing"

#else
#if defined (__linux)
#include <GL/glx.h>
#elif defined( __WIN32 )
#include <windows.h>
#endif // (__linux)

#include <CL/cl.h>
#include <CL/cl_gl.h>
#define CL_GL_SHARING_EXT "cl_khr_gl_sharing"
#endif // if defined(__APPLE__) || defined(__MACOSX)

struct alignas(16) AABB
{
    cl_float4 p_min;
    cl_float4 p_max;

};

struct alignas(16) BVHNodeGPU
{
    AABB aabb;          //32
    cl_int vert_list[10];//40
    cl_int child_idx;   //4
    cl_int vert_len;    //4 - total 80
};


inline cl_float4 operator+ (cl_float4& a, cl_float4& b)
{
    return { a.s[0] + b.s[0], a.s[1] + b.s[1], a.s[2] + b.s[2], ((a.s[3] + b.s[3]) >= 1) ? 1.0f : 0.0f };
}

inline cl_float4 operator- (cl_float4& a, cl_float4& b)
{
    return { a.s[0] - b.s[0], a.s[1] - b.s[1], a.s[2] - b.s[2], (a.s[3] == 1 || b.s[3] == 1) ? 1.0f : 0.0f };
}

inline cl_float4 operator/ (cl_float4& a, const cl_float& b)
{
    return { a.s[0] / b, a.s[1] / b, a.s[2] / b, (a.s[3] == 1) ? 1.0f : 0.0f };
}

inline cl_float4 operator* (cl_float4& a, cl_float& b)
{
    return { a.s[0] * b, a.s[1] * b, a.s[2] * b, (a.s[3] == 1) ? 1.0f : 0.0f };
}