#pragma once
#include "glew.h"
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <vector>

#include "gtc/matrix_transform.hpp"
#include "gtc/constants.hpp"
#include "TimeMeasurer.h"


enum ProjectionMode {
    EQUIRECTANGULAR = 0,
    EQUAL_AREA,
    PM_NOT_SPECIFIED
};

enum DrawMode {
    USE_INDEX = 0,
    DONT_USE_INDEX,
    DM_NOT_SPECTIFIED
};

struct Vertex {
    double x;
    double y;
    double z;
    double u;
    double v;
    int index;
};

class Player {
public:
	Player(int numberOfPatches);
    //Player(int width, int height);
    Player(int width, int height, int numberOfPatches = 128);
    ~Player();
	bool setupTextureData(unsigned char *textureData);
	void renderLoop();
    void setupMode(ProjectionMode projection, DrawMode draw);
private:
    bool init();
    void destroyGL();
	bool setupShaders();
	bool setupTexture();
    bool setupCoordinates();
    bool setupERPCoordinatesWithIndex();
	bool setupERPCoordinatesWithoutIndex();
    bool setupCPPCoordinatesWithoutIndex();
    bool setupCPPCoordinatesWithIndex();
    bool setupCppCoordinates();
	bool setupMatrixes();
	void setupProjectionMatrix();
    void drawFrame();
	void drawFrameERPWithoutIndex();
    void drawFrameERPWithIndex();
    void drawFrameCPPWithIndex();
    void drawFrameCPPWithoutIndex();
	bool handleInput();
	void resizeWindow(SDL_Event& event);
	void computeMVPMatrix();
	void computeViewMatrix();
    void computeSTCoordinates(float latitude, float longitude, float &s, float &t);
    void computeSTCoordinates(double latitude, double longitude, double &u, double &v);
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
	
	int patchNumbers;

	float lon;
	float lat;

	float *vertexArray = NULL;
	float *uvArray = NULL;
    int *indexArray = NULL;
    int vertexCount;
    int indexArraySize;

    std::vector<double> vertexVector;
    std::vector<double> uvVector;
    std::vector<int> indexVector;
    
    int frameHeight;
    int frameWidth;
    TimeMeasurer *timeMeasurer = NULL;
    ProjectionMode projectionMode;
    DrawMode drawMode;
};