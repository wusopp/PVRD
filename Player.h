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

#define CPP 1
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
     * ���ش�Start()��ʼ��Now()֮�侭����ʱ�䣬�Ժ���(ms)Ϊ��λ
     */
    __int64 elapsedMillionSecondsSinceStart() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (((now.QuadPart - start.QuadPart) * 1000) / freq.QuadPart);
    }

    /**
     * ���ش�Start()��ʼ��Now()֮�侭����ʱ�䣬��΢��(us)Ϊ��λ
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
    bool setupCppCoordinatesOld();
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

#ifdef CPP
    GLuint topTriangleVAO;
    GLuint topTriangleBuffer;
    GLuint topUVBuffer;

    GLuint bottomTriangleVAO;
    GLuint bottomTriangleBuffer;
    GLuint bottomUVBuffer;


    int topTriangleVertexCount;
    int bottomTriangleVertexCount;
    int midTriangleVertexCount;

    std::vector<float> topTriangleVector;
    std::vector<float> topUVVector;
    std::vector<float> bottomTriangleVector;
    std::vector<float> bottomUVVector;
    std::vector<float> midTriangleVector;
    std::vector<float> midUVVector;

#endif // CPP

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

	float *vertexCoordinates = NULL;
	float *uvCoordinates = NULL;
    int frameHeight;
    int frameWidth;
    TimeMeasurer *watch = NULL;
    ProjectionMode mode;
};