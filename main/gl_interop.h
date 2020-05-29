#include <GLFW/glfw3.h>
#include <CL\cl.hpp>

//#define GL_SHARING_EXTENSION "cl_khr_gl_sharing"

const int window_width = 1280;
const int window_height = 720;

// OpenGL vertex buffer object
GLuint vbo;

void render();


void createVBO(GLuint* vbo)
{
	//create vertex buffer object
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);

	//initialise VBO
	unsigned int size = window_width * window_height * sizeof(cl_float3);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawGL() {

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
	glutSwapBuffers();
}

//void Timer(int value) {
//	glutPostRedisplay();
//	glutTimerFunc(15, Timer, 0);
//}