
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

#include <fstream>

//-------------CL----------------------

using namespace std;
using namespace cl;

const int sphere_count = 9;


// OpenCL objects
Device device;
CommandQueue queue;
Kernel kernel;
Context context;
Program program;
Buffer cl_output;
Buffer cl_spheres;
BufferGL cl_vbo;
vector<Memory> cl_vbos;

// image buffer (not needed with real-time viewport)
cl_float4* cpu_output;
cl_int err;
unsigned int framenumber = 0;


struct Sphere
{
    cl_float radius;
    cl_float dummy1;
    cl_float dummy2;
    cl_float dummy3;
    cl_float3 position;
    cl_float3 color;
    cl_float3 emission;
};

Sphere cpu_spheres[sphere_count];

void pickPlatform(Platform& platform, const vector<Platform>& platforms) {

    if (platforms.size() == 1) platform = platforms[0];
    else {
        int input = 0;
        cout << "\nChoose an OpenCL platform: ";
        cin >> input;

        // handle incorrect user input
        while (input < 1 || input > platforms.size()) {
            cin.clear(); //clear errors/bad flags on cin
            cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
            cout << "No such option. Choose an OpenCL platform: ";
            cin >> input;
        }
        platform = platforms[input - 1];
    }
}

void pickDevice(Device& device, const vector<Device>& devices) {

    if (devices.size() == 1) device = devices[0];
    else {
        int input = 0;
        cout << "\nChoose an OpenCL device: ";
        cin >> input;

        // handle incorrect user input
        while (input < 1 || input > devices.size()) {
            cin.clear(); //clear errors/bad flags on cin
            cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
            cout << "No such option. Choose an OpenCL device: ";
            cin >> input;
        }
        device = devices[input - 1];
    }
}

void printErrorLog(const Program& program, const Device& device) {

    // Get the error log and print to console
    string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
    cerr << "Build log:" << std::endl << buildlog << std::endl;

    // Print the error log to a file
    FILE* log = fopen("errorlog.txt", "w");
    fprintf(log, "%s\n", buildlog);
    cout << "Error log saved in 'errorlog.txt'" << endl;
    system("PAUSE");
    exit(1);
}


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

	// Create an OpenCL context on that device.
	// Windows specific OpenCL-OpenGL interop
	//cl_context_properties properties[] =
	//{
	//	CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
	//	CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
	//	CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
	//	0
	//};
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
	ifstream file("../../../cl_kernels/test.cl");
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

	// Create an OpenCL program with source
	program = Program(context, kernel_source);

	// Build the program for the selected device 
	cl_int result = program.build({ device }); // "-cl-fast-relaxed-math"
	if (result) cout << "Error during compilation OpenCL code!!!\n (" << result << ")" << endl;
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

#define float3(x, y, z) {{x, y, z}}  // macro to replace ugly initializer braces

void initScene(Sphere* cpu_spheres) {

	// left wall
	cpu_spheres[0].radius = 200.0f;
	cpu_spheres[0].position = float3(-200.6f, 0.0f, 0.0f);
	cpu_spheres[0].color = float3(0.75f, 0.25f, 0.25f);
	cpu_spheres[0].emission = float3(0.0f, 0.0f, 0.0f);

	// right wall
	cpu_spheres[1].radius = 200.0f;
	cpu_spheres[1].position = float3(200.6f, 0.0f, 0.0f);
	cpu_spheres[1].color = float3(0.25f, 0.25f, 0.75f);
	cpu_spheres[1].emission = float3(0.0f, 0.0f, 0.0f);

	// floor
	cpu_spheres[2].radius = 200.0f;
	cpu_spheres[2].position = float3(0.0f, -200.4f, 0.0f);
	cpu_spheres[2].color = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[2].emission = float3(0.0f, 0.0f, 0.0f);

	// ceiling
	cpu_spheres[3].radius = 200.0f;
	cpu_spheres[3].position = float3(0.0f, 200.4f, 0.0f);
	cpu_spheres[3].color = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[3].emission = float3(0.0f, 0.0f, 0.0f);

	// back wall
	cpu_spheres[4].radius = 200.0f;
	cpu_spheres[4].position = float3(0.0f, 0.0f, -200.4f);
	cpu_spheres[4].color = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[4].emission = float3(0.0f, 0.0f, 0.0f);

	// front wall 
	cpu_spheres[5].radius = 200.0f;
	cpu_spheres[5].position = float3(0.0f, 0.0f, 202.0f);
	cpu_spheres[5].color = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[5].emission = float3(0.0f, 0.0f, 0.0f);

	// left sphere
	cpu_spheres[6].radius = 0.16f;
	cpu_spheres[6].position = float3(-0.25f, -0.24f, -0.1f);
	cpu_spheres[6].color = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[6].emission = float3(0.0f, 0.0f, 0.0f);

	// right sphere
	cpu_spheres[7].radius = 0.16f;
	cpu_spheres[7].position = float3(0.25f, -0.24f, 0.1f);
	cpu_spheres[7].color = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[7].emission = float3(0.0f, 0.0f, 0.0f);

	// lightsource
	cpu_spheres[8].radius = 1.0f;
	cpu_spheres[8].position = float3(0.0f, 1.36f, 0.0f);
	cpu_spheres[8].color = float3(0.0f, 0.0f, 0.0f);
	cpu_spheres[8].emission = float3(9.0f, 8.0f, 6.0f);

}

void initCLKernel() {

	// pick a rendermode
	unsigned int rendermode = 1;

	// Create a kernel (entry point in the OpenCL source program)
	kernel = Kernel(program, "render_kernel");

	// specify OpenCL kernel arguments
	//kernel.setArg(0, cl_output);
	kernel.setArg(0, cl_spheres);
	kernel.setArg(1, window_width);
	kernel.setArg(2, window_height);
	kernel.setArg(3, sphere_count);
	kernel.setArg(4, cl_vbo);
	kernel.setArg(5, framenumber);
}

void runKernel() {
	// every pixel in the image has its own thread or "work item",
	// so the total amount of work items equals the number of pixels
	std::size_t global_work_size = window_width * window_height;
	std::size_t local_work_size = kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device);;

	// Ensure the global work size is a multiple of local work size
	if (global_work_size % local_work_size != 0)
		global_work_size = (global_work_size / local_work_size + 1) * local_work_size;

	//Make sure OpenGL is done using the VBOs
	glFinish();

	//this passes in the vector of VBO buffer objects 
	queue.enqueueAcquireGLObjects(&cl_vbos);
	queue.finish();

	// launch the kernel
	queue.enqueueNDRangeKernel(kernel, NULL, global_work_size, local_work_size); // local_work_size
	queue.finish();

	//Release the VBOs so OpenGL can play with them
	queue.enqueueReleaseGLObjects(&cl_vbos);
	queue.finish();
}


// hash function to calculate new seed for each frame
// see http://www.reedbeta.com/blog/2013/01/12/quick-and-easy-gpu-random-numbers-in-d3d11/
unsigned int WangHash(unsigned int a) {
	a = (a ^ 61) ^ (a >> 16);
	a = a + (a << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}


void render(GLFWwindow * window) {

	framenumber++;

	cpu_spheres[6].position.s[1] += 0.01;

	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);

	kernel.setArg(0, cl_spheres);
	kernel.setArg(5, WangHash(framenumber));

	runKernel();

	drawGL(window);
}

void cleanUp() {
	//	delete cpu_output;
}

void main_old(int argc, char** argv) {

	

	// start rendering continuously
	// glutMainLoop();

	// release memory
	cleanUp();

	system("PAUSE");
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

	//make sure OpenGL is finished before we proceed
	glFinish();

	// initialise scene
	initScene(cpu_spheres);

    //
    // test_cl();
    // return 0;
    // initialise OpenCL
    initOpenCL();
    // create vertex buffer object
    createVBO(&vbo);

	cl_spheres = Buffer(context, CL_MEM_READ_ONLY, sphere_count * sizeof(Sphere));
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);

	// create OpenCL buffer from OpenGL vertex buffer object
	cl_vbo = BufferGL(context, CL_MEM_WRITE_ONLY, vbo);
	cl_vbos.push_back(cl_vbo);

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
        glBindTexture(GL_TEXTURE_2D,scene.texture_white);

        // Create the basic gui structure with ImGui
        gui_start_basic_structure(gui,scene);

        // Perform computation and draw calls for each iteration loop
        // scene_current.frame_draw(shaders, scene, gui); opengl_debug();


        // Render GUI and update window
        ImGui::End();
        scene.camera_control.update = !(ImGui::IsAnyWindowFocused());
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
