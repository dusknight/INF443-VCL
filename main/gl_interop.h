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

//bool setupGLBuffer(GLuint rbo_IDs[4], GLuint &fbo_ID) {
//	glDeleteRenderbuffers(4, rbo_IDs);
//	glDeleteFramebuffers(1, fbo_ID);
//
//	glGenFramebuffers(1, &fbo_ID);
//	glBindFramebuffer(GL_FRAMEBUFFER, fbo_ID);
//
//	glGenRenderbuffers(4, rbo_IDs);
//
//	glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[0]);
//	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
//	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_IDs[0]);
//
//	glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[1]);
//	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
//	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rbo_IDs[1]);
//
//	glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[2]);
//	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
//	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, rbo_IDs[2]);
//
//	glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[3]);
//	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
//	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_RENDERBUFFER, rbo_IDs[3]);
//
//	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//
//	if (status != GL_FRAMEBUFFER_COMPLETE) {
//		// TODO ERROR
//		std::cerr << " ERROR ... TODO" << std::endl;
//	}
//
//	glClearColor(0.f, 0.f, 0.f, 1.0f);
//	glDrawBuffer(GL_COLOR_ATTACHMENT0);
//	glClear(GL_COLOR_BUFFER_BIT);
//
//	glDrawBuffer(GL_COLOR_ATTACHMENT1);
//	glClear(GL_COLOR_BUFFER_BIT);
//
//	glDrawBuffer(GL_COLOR_ATTACHMENT2);
//	glClear(GL_COLOR_BUFFER_BIT);
//
//	glDrawBuffer(GL_COLOR_ATTACHMENT3);
//	glClear(GL_COLOR_BUFFER_BIT);
//
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
//	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//
//	return true;
//}

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