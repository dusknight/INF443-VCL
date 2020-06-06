#pragma once


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
#include <windows.h>

#include <CL/cl.hpp>
// #include <CL/cl.h>
#include <CL/cl_gl.h>
#define CL_GL_SHARING_EXT "cl_khr_gl_sharing"
#endif // if defined(__APPLE__) || defined(__MACOSX)

#include "cl_ext/cl_defs.h"

// using namespace cl;
using namespace std;

class cl_manager {
public:
	cl_manager();
	~cl_manager();

	cl::Platform platform;
	cl::Device device;
	cl::CommandQueue queue;
	cl::Kernel kernel;
	cl::Context context;
	cl::Program program;
	cl::Buffer cl_output;
	cl::Buffer cl_spheres;
	cl::Buffer cl_camera;
	cl::Buffer cl_accumbuffer;
	vector<cl::Memory> image_buffers;
	cl::Buffer bvh_buffer;
	cl::Buffer vtx_buffer;
	cl::Buffer mat_buffer;

	void setup_dev(cl_context_properties* properties, cl_device_id device_id, cl_platform_id platform_id);

	void pickDevice(const std::vector<cl::Device>& devices);

	void pickPlatform(const std::vector<cl::Platform>& platforms);

	int test_cl();

	void initOpenCL();

	void initCLKernel(int buffer_switch, int buffer_reset, int window_width, int window_height, int sphere_count, int framenumber);

	bool setupBufferBVH(vector<BVHNodeGPU>& bvh_data, float bvh_size, float scene_size);

	bool setupBufferVtx(vector<TriangleGPU>& vtx_data, float scene_size);

	bool setupBufferMat(vector<Material>& mat_data);

};