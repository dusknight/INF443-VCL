#pragma once

#include "GLFW/glfw3.h"
#include <glad/glad.hpp>
#include <CL/cl.hpp>

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
#include <CL/cl.hpp>
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


