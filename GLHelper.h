#pragma once
#include "GL/glew.h"
#include <iostream>
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include "gtc/matrix_transform.hpp"
#include "gtc/constants.hpp"
#include <vector>
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
	bool setupGraphics();
	bool setupTexture();
	bool setupCoordinates();
	bool setupMatrixes();
	void setupProjectionMatrix();
	void drawFrame();
	bool handleInput();
	void resizeWindow(SDL_Event& event);
	void computeMVPMatrix();
	void addVertex(float x, float y, float s, float t, std::vector<float> &vertexData);
	void computeViewMatrixFromMouseInput();
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
};