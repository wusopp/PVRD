#pragma once
#include "glew.h"
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

#define CPP 0
enum ProjectionMode {
    EQUIRECTANGULAR,
    EQUAL_AREA,
    NOT_SPECIFIED
};


class TimeMeasurer {
public:
    TimeMeasurer() {
        QueryPerformanceFrequency(&freq);
    }

    void Start() {
        QueryPerformanceCounter(&start);
    }

    /**
     * 返回从Start()开始到Now()之间经过的时间，以毫秒(ms)为单位
     */
    __int64 elapsedMillionSecondsSinceStart() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (((now.QuadPart - start.QuadPart) * 1000) / freq.QuadPart);
    }

    /**
     * 返回从Start()开始到Now()之间经过的时间，以微秒(us)为单位
     */
    __int64 elapsedMicroSecondsSinceStart() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return ((now.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart);
    }

    __int64 elapsedTimeInMillionSeconds(void (*func)()) {
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

class Player {
    
public:
	Player(int numberOfPatches);
    Player(int width, int height);
    Player(int width, int height, int numberOfPatches);
    ~Player();
	bool setupTextureData(unsigned char *textureData);
	void renderLoop();
    void setupProjectionMode(ProjectionMode mode);
private:
    bool init();
    void destroyGL();
	bool setupShaders();
	bool setupTexture();
    bool setupCoordinates();
	bool setupSphereCoordinates();
    bool setupCppCoordinates_();
    bool setupCppCoordinates();
	bool setupMatrixes();
	void setupProjectionMatrix();
    void drawFrame();
	void drawFrameERP();
    void drawFrameCpp();
	bool handleInput();
	void resizeWindow(SDL_Event& event);
	void computeMVPMatrix();
	void computeViewMatrix();
    void computeSTCoordinates(float latitude, float longitude, float &s, float &t);
private:
    SDL_Window *pWindow = NULL;
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
    GLuint sceneIndexBuffer;

	int pPreviousXposition;
	int pPreviousYposition;
	int pCurrentXposition;
	int pCurrentYposition;
	int pWindowHeight;
	int pWindowWidth;
	int vertexCount;
	int patchNumbers;

	float lon;
	float lat;

	float *vertexArray = NULL;
	float *uvArray = NULL;
    int *indexArray = NULL;

    int indexArraySize;

    int frameHeight;
    int frameWidth;
    TimeMeasurer *watch = NULL;
    ProjectionMode mode;
};