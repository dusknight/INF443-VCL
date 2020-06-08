#pragma  once

// partly from YUNE<https://github.com/gallickgunner/Yune>

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
#elif defined _WIN32 
#include <windows.h>
#endif // (__linux)

#include <CL/cl.h>
#include <CL/cl.hpp>
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

struct alignas(16) Material
{
    cl_float4 ke;
    cl_float4 kd;
    cl_float4 ks;
    cl_float n;
    cl_float k;
    cl_float px;
    cl_float py;
    cl_float alpha_x;
    cl_float alpha_y;
    cl_int is_specular;
    cl_int is_transmissive;    // total 80 bytes.
};

struct alignas(16) TriangleGPU
{
    cl_float4 v1;
    cl_float4 v2;
    cl_float4 v3;
    cl_float4 vn1;
    cl_float4 vn2;
    cl_float4 vn3;
    cl_int matID;       // total size till here = 100 bytes
    cl_float pad[3];    // padding 12 bytes - to make it 112 (next multiple of 16)
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


inline Material newMaterial()
{
    return {
            {0,0,0,1},          //ke
            {0.3,0.3,0.3,1},    //kd
            {0,0,0,1},          //ks
            1,                  //n
            1,                  //k
            1,                  //px
            1,                  //py
            100,                //alpha_x
            100,                //alpha_y
            0,                  //is_specular
            0                   //is_transmissive
    };
}

