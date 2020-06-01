#include <GLFW\glfw3.h>
#include <glad\glad.hpp>
#include <CL\cl.hpp>

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


#include <iostream>

//#define GL_SHARING_EXTENSION "cl_khr_gl_sharing"

const int window_width = 1280;
const int window_height = 720;

// OpenGL vertex buffer object
GLuint vbo;

void render(GLFWwindow * window);


void createVBO(GLuint* vbo)
{
	int err;
	//create vertex buffer object
	// err = glGetError();
	glGenBuffers(1, vbo);
	err = glGetError();
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	err = glGetError();

	//initialise VBO
	unsigned int size = window_width * window_height * sizeof(cl_float3);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void drawGL(GLFWwindow * window) {

	//clear all pixels, then render from the vbo
	glClear(GL_COLOR_BUFFER_BIT);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexPointer(2, GL_FLOAT, 16, 0); // size (2, 3 or 4), type, stride, pointer
	glColorPointer(4, GL_UNSIGNED_BYTE, 16, (GLvoid*)8); // size (3 or 4), type, stride, pointer

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glDrawArrays(GL_POINTS, 0, window_width * window_height);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	// flip backbuffer to screen
	//glutSwapBuffers();
	glfwSwapBuffers(window);
}

//void Timer(int value) {
//	glutPostRedisplay();
//	glutTimerFunc(15, Timer, 0);
//}

void initOpenCL();
int test_cl();

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

void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms) {

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

void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices) {

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