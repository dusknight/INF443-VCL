#pragma once
#include "vcl/math/math.hpp"
#include "cl_ext/cl_defs.h"

// padding with dummy variables are required for memory alignment
// float3 is considered as float4 by OpenCL
// alignment can also be enforced by using __attribute__ ((aligned (16)));
// see https://www.khronos.org/registry/cl/sdk/1.0/docs/man/xhtml/attributes-variables.html

struct Sphere
{
	float radius;
	int materialPara;
	int dummy1;;
	float dummy2;
	cl_float3 position;
	cl_float3 color;
	cl_float3 emission;
}; 

struct alignas(16) Triangle {
	cl_float3 vertex1;
	cl_float3 vertex2;
	cl_float3 vertex3;
	cl_float3 color;
	cl_float3 emission; // 80 bytes
	cl_int materialPara;
	cl_float pad[3]; // padding
	//float dummy1;
	//float dummy2;
	//int dummy0;
	//cl_float3 dummy3;
	//cl_float3 dummy4;
};