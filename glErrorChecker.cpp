#include "glErrorChecker.h"
#include <SDL_opengl.h>
#include <stdio.h>
void glCheckError_(int line) {
	GLenum errorCode;
	char error[100];
	memset(error, 0, sizeof(error));
	while ((errorCode = glGetError()) != GL_NO_ERROR) {
		switch (errorCode) {
		case GL_INVALID_ENUM:                  sprintf_s(error, "GL_INVALID_ENUM"); break;
		case GL_INVALID_VALUE:                 sprintf_s(error, "GL_INVALID_VALUE"); break;
		case GL_INVALID_OPERATION:             sprintf_s(error, "GL_INVALID_OPERATION"); break;
		case GL_OUT_OF_MEMORY:                 sprintf_s(error, "GL_OUT_OF_MEMORY"); break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: sprintf_s(error, "GL_INVALID_FRAMEBUFFER_OPERATION"); break;
		}
		printf("Line is %d, glError: %s\n", line, error);
	}
}