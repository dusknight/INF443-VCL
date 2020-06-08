#pragma once

// #include "linear_algebra.h"
#include "vcl/math/math.hpp"

#define M_PI 3.14156265
#define PI_OVER_TWO 1.5707963267948966192313216916397514420985

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


// Camera struct, used to store interactive camera data, copied to the GPU and used by OpenCL for each frame
struct Camera {
	cl_float3 position;		// 16 bytes
	cl_float3 view;			// 16
	cl_float3 up;			// 16
	cl_float2 resolution;	// 8
	cl_float2 fov;			// 8
	cl_float apertureRadius;	// 4
	cl_float focalDistance;	// 4
	//float dummy1;		// 4
	//float dummy2;		// 4
};

// class for interactive camera object, updated on the CPU for each frame and copied into Camera struct
class InteractiveCamera
{
private:

	vcl::vec3 centerPosition;
	vcl::vec3 viewDirection;
	float yaw;
	float pitch;
	float radius;
	float apertureRadius;
	float focalDistance;

	void fixYaw();
	void fixPitch();
	void fixRadius();
	void fixApertureRadius();
	void fixFocalDistance();

public:
	InteractiveCamera();
	virtual ~InteractiveCamera();
	void changeYaw(float m);
	void changePitch(float m);
	void changeRadius(float m);
	void changeAltitude(float m);
	void changeFocalDistance(float m);
	void strafe(float m);
	void goForward(float m);
	void rotateRight(float m);
	void changeApertureDiameter(float m);
	void setResolution(float x, float y);
	void setFOVX(float fovx);

	void buildRenderCamera(Camera* renderCamera);

	float getApertureRadius();
	float getFocalDistance();


	vcl::vec2 resolution;
	vcl::vec2 fov;
};