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
	~GLHelper();
	bool init();
	void destroy();
	bool setupTextureData(unsigned char *rgbData, int frameWidth, int frameHeight);
	void renderLoop();
private:
	bool setupGraphics();
	bool setupTexture();
	bool setupScene();
	bool setupMatrix();
	void drawFrame();
	bool handleInput();
	void computeMVPMatrix();
	void addVertex(float x, float y, float s, float t, std::vector<float> &vertexData);
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
	GLint sceneTexturePointer;
	GLint scenePositionPointer;
	GLuint sceneVAO;
	GLuint sceneVertBuffer;

	int pWindowHeight;
	int pWindowWidth;
	int pVertexCount;
};