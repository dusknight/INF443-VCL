#include "cl_ext/cl_manager.h"
#include <GLFW\glfw3.h>

#include <iostream>
#include <fstream>

void printErrorLog(const cl::Program& program, const cl::Device& device) {

    // Get the error log and print to console
    std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
    std::cerr << "Build log:" << std::endl << buildlog << std::endl;

    // Print the error log to a file
    FILE* log = fopen("errorlog.txt", "w");
    fprintf(log, "%s\n", buildlog);
    std::cout << "Error log saved in 'errorlog.txt'" << std::endl;
    system("PAUSE");
    exit(1);
}



cl_manager::cl_manager()
{
}

cl_manager::~cl_manager()
{
}

void cl_manager::setup_dev(cl_context_properties* properties, cl_device_id device_id, cl_platform_id platform_id) {
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

void cl_manager::pickDevice(const std::vector<cl::Device>& devices) {

	if (devices.size() == 1) device = devices[0];
	else {
		int input = 0;
		std::cout << "\nChoose an OpenCL device: ";
		std::cin >> input;

		// handle incorrect user input
		while (input < 1 || input > devices.size()) {
			std::cin.clear(); //clear errors/bad flags on cin
			std::cin.ignore(std::cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			std::cout << "No such option. Choose an OpenCL device: ";
			std::cin >> input;
		}
		device = devices[input - 1];
	}
}

void cl_manager::pickPlatform(const std::vector<cl::Platform>& platforms) {

	if (platforms.size() == 1) platform = platforms[0];
	else {
		int input = 0;
		std::cout << "\nChoose an OpenCL platform: ";
		// TODO:
		// cin >> input;
		input = 1;

		// handle incorrect user input
		while (input < 1 || input > platforms.size()) {
			std::cin.clear(); //clear errors/bad flags on cin
			std::cin.ignore(std::cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			std::cout << "No such option. Choose an OpenCL platform: ";
			std::cin >> input;
		}
		platform = platforms[input - 1];
	}
}

int cl_manager::test_cl()
{
    using namespace std;
    using namespace cl;
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


void cl_manager::initOpenCL()
{
    // Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
    vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    cout << "Available OpenCL platforms : " << endl << endl;
    for (int i = 0; i < platforms.size(); i++)
        cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

    cout << endl << "WARNING: " << endl << endl;
    cout << "OpenCL-OpenGL interoperability is only tested " << endl;
    cout << "on discrete GPUs from Nvidia and AMD" << endl;
    cout << "Other devices (such as Intel integrated GPUs) may fail" << endl << endl;

    // Pick one platform
    // Platform platform;
    pickPlatform(platforms);
    cout << "\nUsing OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << endl;

    // Get available OpenCL devices on platform
    vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);

    cout << "Available OpenCL devices on this platform: " << endl << endl;
    for (int i = 0; i < devices.size(); i++) {
        cout << "\t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << endl;
        cout << "\t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
        cout << "\t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl << endl;
    }

    // Pick one device
    pickDevice(devices);
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

    context = cl::Context(device, properties);

    // Create a command queue
    queue = cl::CommandQueue(context, device);

    // Convert the OpenCL source code to a string// Convert the OpenCL source code to a string
    string source;
    ifstream file("../../../cl_kernels/simple_fbo.cl");
    streamoff len;
    if (!file) {
        cout << "\nNo OpenCL file found!" << endl << "Exiting..." << endl;
        system("PAUSE");
        exit(1);
    }

    // Read Kernel
    file.seekg(0, ios::end);
    len = file.tellg();
    file.seekg(0, ios::beg);
    source.resize(len);
    file.read(&source[0], len);
    cout << "[INFO] CL source read successfully." << endl;

    // Create an OpenCL program with source
    const char* kernel_source = source.c_str();
    program = cl::Program(context, kernel_source);

    // Build the program for the selected device 
    cl_int result = program.build({ device }); // "-cl-fast-relaxed-math"
    if (result) cout << "Error during compilation OpenCL code!!!\n (" << result << ")" << endl;
    // std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
    // std::cout << buildlog << endl;
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

void cl_manager::initCLKernel(int buffer_switch, int buffer_reset, int window_width, int window_height, int sphere_count, int framenumber) {

    // pick a rendermode
    unsigned int rendermode = 1;

    // Create a kernel (entry point in the OpenCL source program)
    kernel = cl::Kernel(program, "render_kernel");

    // specify OpenCL kernel arguments
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
    // kernel.setArg(8, cl_camera);
    kernel.setArg(9, rand());
    kernel.setArg(10, rand());
    kernel.setArg(11, hdr_buffer);
}

bool cl_manager::setupBufferBVH(vector<BVHNodeGPU>& bvh_data, float bvh_size, float scene_size) {
    cl_int err = 0;
    if (bvh_buffer()) clReleaseMemObject(bvh_buffer());

    //if (bvh_size + scene_size > target_device.global_mem_size)
    //    throw std::runtime_error("BVH and Scene Data size combined exceed Device's global memory size.");

    bvh_buffer = cl::Buffer(CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR,
        sizeof(BVHNodeGPU) * bvh_data.size(), bvh_data.data(), &err);
    // error? TODO: error
    return true;
}

bool cl_manager::setupBufferVtx(vector<TriangleGPU>& vtx_data, float scene_size) {
    cl_int err = 0;
    if (vtx_buffer()) clReleaseMemObject(vtx_buffer());

    //if (bvh_size + scene_size > target_device.global_mem_size)
    //    throw std::runtime_error("BVH and Scene Data size combined exceed Device's global memory size.");

    bvh_buffer = cl::Buffer(CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR,
        sizeof(TriangleGPU) * vtx_data.size(), vtx_data.data(), &err);
    // error? TODO: error
    return true;
}

bool cl_manager::setupBufferMat(vector<Material>& mat_data) {
    cl_int err = 0;

    if (mat_buffer()) clReleaseMemObject(mat_buffer());

    //if (bvh_size + scene_size > target_device.global_mem_size)
    //    throw std::runtime_error("BVH and Scene Data size combined exceed Device's global memory size.");

    bvh_buffer = cl::Buffer(CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR,
        sizeof(Material) * mat_data.size(), mat_data.data(), &err);
    // error? TODO: error
    return true;
}

bool cl_manager::setupBUfferHDR(cl_float4 * cpu_HDR, int height, int width)
{
    cl_int err = 0;
    bvh_buffer = cl::Buffer(CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR,
        sizeof(cl_float4) * height * width, cpu_HDR, &err);
    if (!err)  return true;
    return false;
}
