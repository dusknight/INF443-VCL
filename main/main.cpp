
// Include VCL library
#include "vcl/vcl.hpp"

// Include common part for exercises
#include "main/helper_scene/helper_scene.hpp"

// Include exercises
#include "scenes/scenes.hpp"

// for CL
#include "main/gl_interop.h"
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

#include <CL/cl.hpp>
#include <CL/cl_gl.h>
#define CL_GL_SHARING_EXT "cl_khr_gl_sharing"
#endif // if defined(__APPLE__) || defined(__MACOSX)

// #include <windows.h>  // TODO: config

#include <vector>
#include <fstream>
#include "linear_algebra.h"
#include "camera.h"
#include "geometry.h"

//-------------CL----------------------

using namespace std;
using namespace cl;

const int sphere_count = 4;


// OpenCL objects
Device device;
CommandQueue queue;
Kernel kernel;
Context context;
Program program;
Buffer cl_output;
Buffer cl_spheres;
Buffer cl_camera;
Buffer cl_accumbuffer;
// BufferGL cl_vbo;
// vector<Memory> cl_vbos;
vector<Memory> image_buffers;
// image_buffers.resize(4);
GLuint rbo_IDs[2];
GLuint fbo_ID;
// image buffer (not needed with real-time viewport)
// cl_float4* cpu_output;
cl_int err;
unsigned int framenumber = 0;

Camera* hostRendercam = NULL;
InteractiveCamera* interactiveCamera;
Sphere cpu_spheres[sphere_count];

// image buffer (not needed with real-time viewport)
cl_float4* cpu_output;
int buffer_switch = 1;
int buffer_reset = 0;


void initOpenCL()
{
	// Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
	vector<Platform> platforms;
	Platform::get(&platforms);
	cout << "Available OpenCL platforms : " << endl << endl;
	for (int i = 0; i < platforms.size(); i++)
		cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

	cout << endl << "WARNING: " << endl << endl;
	cout << "OpenCL-OpenGL interoperability is only tested " << endl;
	cout << "on discrete GPUs from Nvidia and AMD" << endl;
	cout << "Other devices (such as Intel integrated GPUs) may fail" << endl << endl;

	// Pick one platform
	Platform platform;
	pickPlatform(platform, platforms);
	cout << "\nUsing OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << endl;

	// Get available OpenCL devices on platform
	vector<Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);

	cout << "Available OpenCL devices on this platform: " << endl << endl;
	for (int i = 0; i < devices.size(); i++) {
		cout << "\t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << endl;
		cout << "\t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
		cout << "\t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl << endl;
	}


	// Pick one device
	//Device device;
	pickDevice(device, devices);
	cout << "\nUsing OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << endl;

#if defined _WIN32
	static cl_context_properties properties[] =
	{
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
		0
	};
#elif defined __linux
	static cl_context_properties properties[] =
	{
		CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
		CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
		0
	};
#elif defined(__APPLE__) || defined(__MACOSX)
	CGLContextObj glContext = CGLGetCurrentContext();
	CGLShareGroupObj shareGroup = CGLGetShareGroup(glContext);
	static cl_context_properties properties[] =
	{
		CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,(cl_context_properties)shareGroup,
		0
	};
#endif // _WIN_32

	context = Context(device, properties);

	// Create a command queue
	queue = CommandQueue(context, device);


	// Convert the OpenCL source code to a string// Convert the OpenCL source code to a string
	string source;
	ifstream file("../../../cl_kernels/simple_fbo.cl");
	if (!file) {
		cout << "\nNo OpenCL file found!" << endl << "Exiting..." << endl;
		system("PAUSE");
		exit(1);
	}
	while (!file.eof()) {
		char line[256];
		file.getline(line, 255);
		source += line;
	}

	const char* kernel_source = source.c_str();
    // std::cout << source.substr(8742) << endl;
	// Create an OpenCL program with source
	program = Program(context, kernel_source);

	// Build the program for the selected device 
	cl_int result = program.build({ device }); // "-cl-fast-relaxed-math"
	if (result) cout << "Error during compilation OpenCL code!!!\n (" << result << ")" << endl;
    // std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
    // std::cout << buildlog << endl;
    // std::cout << source.substr(10560) << endl;
	if (result == CL_BUILD_PROGRAM_FAILURE || result == CL_INVALID_PROGRAM) {
		// Get the build log
		std::string name = device.getInfo<CL_DEVICE_NAME>();
		std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
		std::cerr << "Build log for " << name << ":" << std::endl
			<< buildlog << std::endl;
		printErrorLog(program, device);
		system("PAUSE");
	}
}

void initScene(Sphere* cpu_spheres) {

    //// left wall
    //cpu_spheres[0].radius = 200.0f;
    //cpu_spheres[0].position = Vector3Df(-200.6f, 0.0f, 0.0f);
    //cpu_spheres[0].color = Vector3Df(0.75f, 0.25f, 0.25f);
    //cpu_spheres[0].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// right wall
    //cpu_spheres[1].radius = 200.0f;
    //cpu_spheres[1].position = Vector3Df(200.6f, 0.0f, 0.0f);
    //cpu_spheres[1].color = Vector3Df(0.25f, 0.25f, 0.75f);
    //cpu_spheres[1].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// floor
    //cpu_spheres[2].radius = 200.0f;
    //cpu_spheres[2].position = Vector3Df(0.0f, -200.4f, 0.0f);
    //cpu_spheres[2].color = Vector3Df(0.9f, 0.8f, 0.7f);
    //cpu_spheres[2].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// ceiling
    //cpu_spheres[3].radius = 200.0f;
    //cpu_spheres[3].position = Vector3Df(0.0f, 200.4f, 0.0f);
    //cpu_spheres[3].color = Vector3Df(0.9f, 0.8f, 0.7f);
    //cpu_spheres[3].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// back wall
    //cpu_spheres[4].radius = 200.0f;
    //cpu_spheres[4].position = Vector3Df(0.0f, 0.0f, -200.4f);
    //cpu_spheres[4].color = Vector3Df(0.9f, 0.8f, 0.7f);
    //cpu_spheres[4].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// front wall 
    //cpu_spheres[5].radius = 200.0f;
    //cpu_spheres[5].position = Vector3Df(0.0f, 0.0f, 202.0f);
    //cpu_spheres[5].color = Vector3Df(0.9f, 0.8f, 0.7f);
    //cpu_spheres[5].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// left sphere
    //cpu_spheres[6].radius = 0.16f;
    //cpu_spheres[6].position = Vector3Df(-0.25f, -0.24f, -0.1f);
    //cpu_spheres[6].color = Vector3Df(0.9f, 0.8f, 0.7f);
    //cpu_spheres[6].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// right sphere
    //cpu_spheres[7].radius = 0.16f;
    //cpu_spheres[7].position = Vector3Df(0.25f, -0.24f, 0.1f);
    //cpu_spheres[7].color = Vector3Df(0.9f, 0.8f, 0.7f);
    //cpu_spheres[7].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    //// lightsource
    //cpu_spheres[8].radius = 1.0f;
    //cpu_spheres[8].position = Vector3Df(0.0f, 1.36f, 0.0f);
    //cpu_spheres[8].color = Vector3Df(0.0f, 0.0f, 0.0f);
    //cpu_spheres[8].emission = Vector3Df(9.0f, 8.0f, 6.0f);
        // floor
    cpu_spheres[0].radius = 200.0f;
    cpu_spheres[0].position = Vector3Df(0.0f, -200.4f, 0.0f);
    cpu_spheres[0].color = Vector3Df(0.9f, 0.3f, 0.0f);
    cpu_spheres[0].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    // left sphere
    cpu_spheres[1].radius = 0.16f;
    cpu_spheres[1].position = Vector3Df(-0.25f, -0.24f, -0.1f);
    cpu_spheres[1].color = Vector3Df(0.9f, 0.8f, 0.7f);
    cpu_spheres[1].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    // right sphere
    cpu_spheres[2].radius = 0.16f;
    cpu_spheres[2].position = Vector3Df(0.25f, -0.24f, 0.1f);
    cpu_spheres[2].color = Vector3Df(0.9f, 0.8f, 0.7f);
    cpu_spheres[2].emission = Vector3Df(0.0f, 0.0f, 0.0f);

    // lightsource
    cpu_spheres[3].radius = 1.0f;
    cpu_spheres[3].position = Vector3Df(0.0f, 1.36f, 0.0f);
    cpu_spheres[3].color = Vector3Df(0.0f, 0.0f, 0.0f);
    cpu_spheres[3].emission = Vector3Df(9.0f, 8.0f, 6.0f);
}


void initCLKernel() {

    // pick a rendermode
    unsigned int rendermode = 1;

    // Create a kernel (entry point in the OpenCL source program)
    kernel = Kernel(program, "render_kernel");

    // specify OpenCL kernel arguments
    //kernel.setArg(0, cl_output);
    if (buffer_switch) {
        kernel.setArg(0, image_buffers[0]);
        kernel.setArg(1, image_buffers[1]);
    }
    else {
        kernel.setArg(1, image_buffers[0]);
        kernel.setArg(0, image_buffers[1]);
    }
    // kernel.setArg(2, buffer_switch);
    kernel.setArg(2, buffer_reset);
    kernel.setArg(3, cl_spheres);
    kernel.setArg(4, window_width);
    kernel.setArg(5, window_height);
    kernel.setArg(6, sphere_count);
    kernel.setArg(7, framenumber);
    kernel.setArg(8, cl_camera);
    kernel.setArg(9, rand());
    kernel.setArg(10, rand());
    // kernel.setArg(9, cl_accumbuffer);
    // kernel.setArg(11, framenumber);
}

void runKernel() {
    // every pixel in the image has its own thread or "work item",
    // so the total amount of work items equals the number of pixels
    std::size_t global_work_size = window_width * window_height;
    std::size_t local_work_size = kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device);;

    // Ensure the global work size is a multiple of local work size
    if (global_work_size % local_work_size != 0)
        global_work_size = (global_work_size / local_work_size + 1) * local_work_size;

    // setup RBO for reading
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_ID);  // FBO for read back
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  // default FBO for rendering
    glDrawBuffer(GL_BACK);

    //Make sure OpenGL is done using the VBOs
    glFinish();

    cl_int err_code;
    //this passes in the vector of VBO buffer objects => RBO objects
    err_code= queue.enqueueAcquireGLObjects(&image_buffers);
    if (err_code != CL_SUCCESS) {
        std::cerr << "ERROR in locking texture : " << err_code << std::endl;
    }
    queue.finish();

    // launch the kernel
    err_code = queue.enqueueNDRangeKernel(kernel, NULL, global_work_size, local_work_size); // local_work_size
    if (err_code != CL_SUCCESS) {
        std::cerr << "ERROR in running queue :" << err_code << std::endl;
    }
    queue.finish();

    //Release the VBOs so OpenGL can play with them
    // err_code = queue.enqueueReleaseGLObjects(&cl_vbos);
    err_code = queue.enqueueReleaseGLObjects(&image_buffers);
    if (err_code != CL_SUCCESS) {
        std::cerr << "ERROR in unlocking texture :" << err_code << std::endl;
    }
    queue.finish();

    // read from FBO, save a copy
    //glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_ID);
    //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_ID);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_ID);
    if (buffer_switch) {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glDrawBuffer(GL_COLOR_ATTACHMENT1);
    }
    else {
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(
        0, 0, window_width, window_height,
        0, 0, window_width, window_height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST); opengl_debug();
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_ID);  // for rendering: FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  // for rendering: FBO

    // clear previous frame and draw
    if (buffer_switch) glReadBuffer(GL_COLOR_ATTACHMENT0);
    else  glReadBuffer(GL_COLOR_ATTACHMENT1);
    glDrawBuffer(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBlitFramebuffer(
        0, 0, window_width, window_height,
        0, 0, window_width, window_height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST); opengl_debug();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); opengl_debug();  // back to default
    // glDrawBuffer(GL_BACK); opengl_debug();  // draw to back buffer, waiting for swap

    buffer_switch = 1- buffer_switch;
    
}

void render() {
    glReadBuffer(GL_COLOR_ATTACHMENT3);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBlitFramebuffer(
        0, 0, window_width, window_height,
        0, 0, window_width, window_height,
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR); opengl_debug();
    //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); opengl_debug();
    //glDrawBuffer(GL_BACK); opengl_debug();
}

bool setupBufferFBO() {
    // clear previous buffers
    glDeleteRenderbuffers(2, rbo_IDs);
    glDeleteFramebuffers(1, &fbo_ID);

    // create FBO, bind
    glGenFramebuffers(1, &fbo_ID);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_ID);

    // add Renderbufer to FBO
    glGenRenderbuffers(2, rbo_IDs);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_IDs[0]);

    glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rbo_IDs[1]);

    //glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[2]);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, rbo_IDs[2]);

    //glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[3]);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, window_width, window_height);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_RENDERBUFFER, rbo_IDs[3]);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete, code: " << status << endl;
    }

    // clear buffer
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glClear(GL_COLOR_BUFFER_BIT);

    //glDrawBuffer(GL_COLOR_ATTACHMENT2);
    //glClear(GL_COLOR_BUFFER_BIT);

    //glDrawBuffer(GL_COLOR_ATTACHMENT3);
    //glClear(GL_COLOR_BUFFER_BIT);

    // back to default
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}



void cleanUp() {
    //	delete cpu_output;
}

// initialise camera on the CPU
void initCamera()
{
    delete interactiveCamera;
    interactiveCamera = new InteractiveCamera();

    interactiveCamera->setResolution(window_width, window_height);
    interactiveCamera->setFOVX(45);
}


// ************************************** //
// Global data declaration
// ************************************** //

// Storage for shaders indexed by their names
std::map<std::string,GLuint> shaders;

// General shared elements of the scene such as camera and its controler, visual elements, etc
scene_structure scene;

// The graphical interface. Contains Window object and GUI related variables
gui_structure gui;

// Part specific data - you will specify this object in the corresponding exercise part
scene_model scene_current;


// ************************************** //
// GLFW event listeners
// ************************************** //

void window_size_callback(GLFWwindow* /*window*/, int width, int height);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_click_callback(GLFWwindow* window, int button, int action, int mods);
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

// ************************************** //
// Start program
// ************************************** //

// test

void setup_dev(cl_context_properties* properties, cl_device_id device_id, cl_platform_id platform_id) {
    // Load extension
    clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = NULL;

#if CL_TARGET_OPENCL_VERSION > 110
    clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddressForPlatform(platform_id, "clGetGLContextInfoKHR");
#else
    clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
#endif
    if (!clGetGLContextInfoKHR)
        throw std::runtime_error("\"clGetGLContextInfoKHR\" Function failed to load.");

    // err = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &device_id, &num_devices);
}

int test_cl()
{
    // Find all available OpenCL platforms (e.g. AMD, Nvidia, Intel)
    vector<Platform> platforms;
    Platform::get(&platforms);

    // Show the names of all available OpenCL platforms
    cout << "Available OpenCL platforms: \n\n";
    for (unsigned int i = 0; i < platforms.size(); i++)
        cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

    // Choose and create an OpenCL platform
    cout << endl << "Enter the number of the OpenCL platform you want to use: ";
    unsigned int input = 0;
    cin >> input;
    // Handle incorrect user input
    while (input < 1 || input > platforms.size()) {
        cin.clear(); //clear errors/bad flags on cin
        cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
        cout << "No such platform." << endl << "Enter the number of the OpenCL platform you want to use: ";
        cin >> input;
    }

    Platform platform = platforms[input - 1];

    // Print the name of chosen OpenCL platform
    cout << "Using OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << endl;

    // Find all available OpenCL devices (e.g. CPU, GPU or integrated GPU)
    vector<Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

    // Print the names of all available OpenCL devices on the chosen platform
    cout << "Available OpenCL devices on this platform: " << endl << endl;
    for (unsigned int i = 0; i < devices.size(); i++)
        cout << "\t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << endl;

    // Choose an OpenCL device 
    cout << endl << "Enter the number of the OpenCL device you want to use: ";
    input = 0;
    cin >> input;
    // Handle incorrect user input
    while (input < 1 || input > devices.size()) {
        cin.clear(); //clear errors/bad flags on cin
        cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
        cout << "No such device. Enter the number of the OpenCL device you want to use: ";
        cin >> input;
    }

    Device device = devices[input - 1];

    // Print the name of the chosen OpenCL device
    cout << endl << "Using OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << endl << endl;

    // Create an OpenCL context on that device.
    // the context manages all the OpenCL resources 
    static cl_context_properties properties[] =
    {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
        0
    };

    setup_dev(properties, device(), platform());
    Context context = Context(device, properties);

    ///////////////////
    // OPENCL KERNEL //
    ///////////////////

    // the OpenCL kernel in this tutorial is a simple program that adds two float arrays in parallel  
    // the source code of the OpenCL kernel is passed as a string to the host
    // the "__global" keyword denotes that "global" device memory is used, which can be read and written 
    // to by all work items (threads) and all work groups on the device and can also be read/written by the host (CPU)

    const char* source_string =
        " __kernel void parallel_add(__global float* x, __global float* y, __global float* z){ "
        " const int i = get_global_id(0); " // get a unique number identifying the work item in the global pool
        " z[i] = y[i] + x[i];    " // add two arrays 
        "}";

    // Create an OpenCL program by performing runtime source compilation
    Program program = Program(context, source_string);

    // Build the program and check for compilation errors 
    cl_int result = program.build({ device }, "");
    if (result) cout << "Error during compilation! (" << result << ")" << endl;

    // Create a kernel (entry point in the OpenCL source program)
    // kernels are the basic units of executable code that run on the OpenCL device
    // the kernel forms the starting point into the OpenCL program, analogous to main() in CPU code
    // kernels can be called from the host (CPU)
    Kernel kernel = Kernel(program, "parallel_add");

    // Create input data arrays on the host (= CPU)
    const int numElements = 10;
    float cpuArrayA[numElements] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f };
    float cpuArrayB[numElements] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
    float cpuOutput[numElements] = {}; // empty array for storing the results of the OpenCL program

    // Create buffers (memory objects) on the OpenCL device, allocate memory and copy input data to device.
    // Flags indicate how the buffer should be used e.g. read-only, write-only, read-write
    Buffer clBufferA = Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, numElements * sizeof(cl_int), cpuArrayA);
    Buffer clBufferB = Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, numElements * sizeof(cl_int), cpuArrayB);
    Buffer clOutput = Buffer(context, CL_MEM_WRITE_ONLY, numElements * sizeof(cl_int), NULL);

    // Specify the arguments for the OpenCL kernel
    // (the arguments are __global float* x, __global float* y and __global float* z)
    kernel.setArg(0, clBufferA); // first argument 
    kernel.setArg(1, clBufferB); // second argument 
    kernel.setArg(2, clOutput);  // third argument 

    // Create a command queue for the OpenCL device
    // the command queue allows kernel execution commands to be sent to the device
    CommandQueue queue = CommandQueue(context, device);

    // Determine the global and local number of "work items"
    // The global work size is the total number of work items (threads) that execute in parallel
    // Work items executing together on the same compute unit are grouped into "work groups"
    // The local work size defines the number of work items in each work group
    // Important: global_work_size must be an integer multiple of local_work_size 
    std::size_t global_work_size = numElements;
    std::size_t local_work_size = 10; // could also be 1, 2 or 5 in this example
    // when local_work_size equals 10, all ten number pairs from both arrays will be added together in one go

    // Launch the kernel and specify the global and local number of work items (threads)
    queue.enqueueNDRangeKernel(kernel, NULL, global_work_size, local_work_size);

    // Read and copy OpenCL output to CPU 
    // the "CL_TRUE" flag blocks the read operation until all work items have finished their computation
    queue.enqueueReadBuffer(clOutput, CL_TRUE, 0, numElements * sizeof(cl_float), cpuOutput);

    // Print results to console
    for (int i = 0; i < numElements; i++)
        cout << cpuArrayA[i] << " + " << cpuArrayB[i] << " = " << cpuOutput[i] << endl;

    system("PAUSE");
    return 0;
}

int main()
{

    // ************************************** //
    // Initialization and data setup
    // ************************************** //

    // Initialize external libraries and window
    initialize_interface(gui, window_width, window_height, true);
    // glfwSwapInterval(0);
    // Set GLFW events listener
    glfwSetCursorPosCallback(gui.window, cursor_position_callback );
    glfwSetMouseButtonCallback(gui.window, mouse_click_callback);
    glfwSetScrollCallback(gui.window, mouse_scroll_callback);
    glfwSetKeyCallback(gui.window, keyboard_input_callback);
    // glfwSetWindowSizeCallback(gui.window, window_size_callback);

    // load_shaders(shaders);
    // setup_scene(scene, gui, shaders);

    //opengl_debug();
    //std::cout<<"*** Setup Data ***"<<std::endl;
    //scene_current.setup_data(shaders, scene, gui);
    //std::cout<<"\t [OK] Data setup"<<std::endl;
    //opengl_debug();

    //////////////// OPENCL  ////////////////////////////
    // test_cl();
    
    // initialise OpenCL
    initOpenCL();

    // create FBO
    setupBufferFBO();  // first GL
    // createVBO(&vbo);

    cl_int err;
    for (int i = 0; i < 2; i++) {  // Then CL
        image_buffers.push_back(cl::BufferRenderGL(context, CL_MEM_READ_WRITE, rbo_IDs[i], &err));
        if (err != CL_SUCCESS) std::cerr << "ERROR allocating RBO : " << err << std::endl;
    }

	////make sure OpenGL is finished before we proceed
	//glFinish();

	// initialise scene
	initScene(cpu_spheres);
    interactiveCamera = new InteractiveCamera;
	cl_spheres = Buffer(context, CL_MEM_READ_ONLY, sphere_count * sizeof(Sphere));
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);

	// intitialise the kernel
	initCLKernel();


    // ************************************** //
    // Animation loop
    // ************************************** //

    std::cout<<"*** Start GLFW animation loop ***"<<std::endl;
    vcl::glfw_fps_counter fps_counter;
    while( !glfwWindowShouldClose(gui.window) )
    {
        opengl_debug();

        // Clear all color and zbuffer information before drawing on the screen
        clear_screen();opengl_debug();
        // Set a white image texture by default
        // glBindTexture(GL_TEXTURE_2D,scene.texture_white);

        // Create the basic gui structure with ImGui
        gui_start_basic_structure(gui,scene);

        // Perform computation and draw calls for each iteration loop
        // scene_current.frame_draw(shaders, scene, gui); opengl_debug();
        // render(gui.window);
        // framenumber++;

        // host to device: write
        queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);

        //if (buffer_reset) {
        //    float arg = 0;
        //    queue.enqueueFillBuffer(cl_accumbuffer, arg, 0, window_width * window_height * sizeof(cl_float3));
        //    framenumber = 0;
        //}
        // buffer_reset = false;
        framenumber++;

        // build a new camera for each frame on the CPU
        hostRendercam = new Camera;
        interactiveCamera->buildRenderCamera(hostRendercam);
        // copy the host camera to a OpenCL camera
        queue.enqueueWriteBuffer(cl_camera, CL_TRUE, 0, sizeof(Camera), hostRendercam);      

        // update params
        kernel.setArg(3, cl_spheres);  // in case that spheres move
        //kernel.setArg(5, framenumber);
        //kernel.setArg(6, cl_camera);
        //kernel.setArg(7, rand());
        //kernel.setArg(8, rand());
        //kernel.setArg(10, WangHash(framenumber));
        kernel.setArg(2, buffer_reset);
        kernel.setArg(7, framenumber);
        kernel.setArg(8, cl_camera);
        kernel.setArg(9, rand());
        kernel.setArg(10, rand());
        // kernel.setArg(11, WangHash(framenumber));

        runKernel();

        // draw

        ////////////////////////////// GUI ////////////////////////////
        // Render GUI and update window
        ImGui::End();
        // scene.camera_control.update = !(ImGui::IsAnyWindowFocused());
        vcl::imgui_render_frame(gui.window);

        update_fps_title(gui.window, gui.window_title, fps_counter);

        glfwSwapBuffers(gui.window);
        glfwPollEvents();
        opengl_debug();

    }
    std::cout<<"*** Stop GLFW loop ***"<<std::endl;

    // Cleanup ImGui and GLFW
    vcl::imgui_cleanup();

    glfwDestroyWindow(gui.window);
    glfwTerminate();

    return 0;
}

void window_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
    scene.camera.perspective.image_aspect = width / static_cast<float>(height);;
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    scene.camera_control.update_mouse_move(scene.camera, window, float(xpos), float(ypos));
    scene_current.mouse_move(scene, window);
}
void mouse_click_callback(GLFWwindow* window, int button, int action, int mods)
{
    ImGui::SetWindowFocus(nullptr);
    scene.camera_control.update_mouse_click(scene.camera, window, button, action, mods);
    scene_current.mouse_click(scene, window,button,action,mods);
}
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    scene_current.mouse_scroll(scene, window, float(xoffset), float(yoffset));
}
void keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    scene_current.keyboard_input(scene, window, key, scancode, action, mods);
}
