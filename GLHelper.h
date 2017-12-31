#pragma once
#include "GL/glew.h"
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include "gtc/matrix_transform.hpp"
#include "gtc/constants.hpp"
#include <vector>

class CStopwatch {
public:
    CStopwatch() {
        QueryPerformanceFrequency(&freq);
    }

    void Start() {
        QueryPerformanceCounter(&start);
    }

    // 返回从Start()开始到Now()之间经过的时间，以毫秒(ms)为单位
    __int64 elapsedMillionSecondsSinceStart() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (((now.QuadPart - start.QuadPart) * 1000) / freq.QuadPart);
    }

    __int64 elapsedMicroSecondsSinceStart() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return ((now.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart);
    }

    _int64 elapsedTimeInMillionSeconds(void (*func)()) {
        QueryPerformanceCounter(&start);
        func();
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (((now.QuadPart - start.QuadPart) * 1000) / freq.QuadPart);
    }

private:
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
};


class GLHelper {
public:
	GLHelper();
	GLHelper(int numberOfPatches);
	~GLHelper();
	bool init();
	void destroy();
	bool setupTextureData(unsigned char *rgbData, int frameWidth, int frameHeight);
	void renderLoop();
private:
	bool setupShaders();
	bool setupTexture();
	bool setupSphereCoordinates();
	bool setupCppCoordinates();
	bool setupMatrixes();
	void setupProjectionMatrix();
	void drawFrame();
	bool handleInput();
	void resizeWindow(SDL_Event& event);
	void computeMVPMatrix();
	void computeViewMatrix();
private:
	SDL_Window *pWindow;
	SDL_GLContext pContext;
	glm::mat4 modelMatrix;
	glm::mat4 viewMatrix;
	glm::mat4 projectMatrix;
	glm::mat4 mvpMatrix;
	GLuint sceneProgramID;
	GLuint sceneTextureID;
	GLint sceneMVPMatrixPointer;
	GLuint sceneVAO;
	GLuint sceneVertBuffer;
	GLuint sceneUVBuffer;

	int pPreviousXposition;
	int pPreviousYposition;
	int pCurrentXposition;
	int pCurrentYposition;
	int pWindowHeight;
	int pWindowWidth;
	int pVertexCount;
	int pNumberOfPatches;

	float lon;
	float lat;

	float *vertexCoordinates = NULL;
	float *uvCoordinates = NULL;

    CStopwatch *watch;
};