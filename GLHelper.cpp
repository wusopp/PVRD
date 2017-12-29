#include "GLHelper.h"

#define glCheckError() glCheckError_(__LINE__)

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

static const char VERTEX_SHADER[] =
"#version 410 core\n"
"uniform mat4 matrix;\n"
"layout(location = 0) in vec4 position;\n"
"layout(location = 1) in vec2 uvCoords;\n"
"out vec2 uvCoordsOut;\n"
"void main() {\n"
"	uvCoordsOut = uvCoords;\n"
"	gl_Position = matrix * position;\n"
"}\n";

static const char FRAGMENT_SHADER[] =
"#version 410 core\n"
"uniform sampler2D mytexture;\n"
"in vec2 uvCoordsOut;\n"
"void main() {\n"
"	gl_FragColor = texture(mytexture,uvCoordsOut);\n"
"}\n";

void addShader(int type, const char * source, int program) {
	int shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        std::cout << "Failed to add shader." << std::endl;
    }
	glAttachShader(program, shader);
	glDeleteShader(shader);
}

GLHelper::GLHelper():pWindow(NULL),pContext(NULL) {

}

GLHelper::~GLHelper() {

}


bool GLHelper::init()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		std::cout << __FUNCTION__ << "- SDL could not initialize! SDL Error: " << SDL_GetError() << std::endl;
		return false;
	}

	int windowPosX = 100;
	int windowPosY = 100;
	pWindowWidth = 640;
	pWindowHeight = 360;

	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

	pWindow = SDL_CreateWindow("Display CPP", windowPosX, windowPosY, pWindowWidth, pWindowHeight, windowFlags);

	if (pWindow == NULL) {
		std::cout << __FUNCTION__ << "- Window could not be created! SDL Error: " << SDL_GetError() << std::endl;
		return false;
	}

	pContext = SDL_GL_CreateContext(pWindow);
	if (pContext == NULL) {
		std::cout << __FUNCTION__ << "- OpenGL context could not be created! SDL Error: " << SDL_GetError() << std::endl;
		return false;
	}

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		std::cout << __FUNCTION__ << "- GLEW could not be inited! SDL Error: " << SDL_GetError() << std::endl;
		return false;
	}
	glGetError();


	if (SDL_GL_SetSwapInterval(0) < 0) {
		std::cout << __FUNCTION__ << "- SDL could not swapInteval! SDL Error: " << SDL_GetError() << std::endl;
		return false;
	}

	if (!setupMatrix()) {
		std::cout << __FUNCTION__ << "- SetupCamera failed." << std::endl;
		return false;
	}
	
	if (!setupGraphics()) {
		std::cout << __FUNCTION__ << "- SetupGraphics failed." << std::endl;
		return false;
	}
	if (!setupTexture()) {
		std::cout << __FUNCTION__ << "- SetupTexture failed." << std::endl;
		return false;
	}
    if (!setupScene()) {
        std::cout << __FUNCTION__ << "- SetupScene failed." << std::endl;
        return false;
    }
	return true;
}                 

bool GLHelper::setupMatrix()
{
	modelMatrix = glm::mat4(1.0f);

	glm::vec3 cameraPosition(0, 0, 3);
	glm::vec3 cameraLookintAt(0, 0, -1);
	glm::vec3 cameraUp(0, 1, 0);
	viewMatrix = glm::lookAt(cameraPosition, cameraLookintAt, cameraUp);

	float aspect = pWindowWidth * 1.0f / pWindowHeight;
	projectMatrix = glm::perspective(45.0f, aspect, 0.1f, 100.0f);
	return true;
}

bool GLHelper::setupScene()
{
	glCheckError();
	std::vector<float> vertexDatas;

	addVertex(-1.0f, 1.0f, 0.0f, 1.0f, vertexDatas);
	addVertex(-1.0f, -1.0f, 0.0f, 0.0f, vertexDatas);
	addVertex(1.0f, 1.0f, 1.0f, 1.0f, vertexDatas);
	addVertex(1.0f, -1.0f, 1.0f, 0.0f, vertexDatas);

	pVertexCount = vertexDatas.size() / 5;
	
	glGenVertexArrays(1, &sceneVAO);
	glBindVertexArray(sceneVAO);
	glCheckError();
	glGenBuffers(1, &sceneVertBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertexDatas.size(), &vertexDatas[0], GL_STATIC_DRAW);
	glCheckError();
	glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
	glCheckError();
	GLsizei stride = 4;
	uintptr_t offset = 0;
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (const void *)offset);
	glCheckError();
	offset = 2;
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)offset);
	glCheckError();
	glBindVertexArray(0);
	return true;
}

bool GLHelper::setupGraphics() {
	glCheckError();
	sceneProgramID = glCreateProgram();
	if (!sceneProgramID) {
		return false;
	}
	addShader(GL_VERTEX_SHADER, VERTEX_SHADER, sceneProgramID);
    glCheckError();
	addShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER, sceneProgramID);
    glCheckError();
	glLinkProgram(sceneProgramID);
	GLint linkStatus = GL_FALSE;
	glGetProgramiv(sceneProgramID, GL_LINK_STATUS, &linkStatus);
    glCheckError();
	if (linkStatus != GL_TRUE) {
		printf("Link sceneProgram failed.\n");
		return false;
	}
	sceneMVPMatrixPointer = glGetUniformLocation(sceneProgramID, "matrix");
	if (sceneMVPMatrixPointer == -1) {
		return false;
	}
	return true;
}

bool GLHelper::setupTexture() {
	glUseProgram(sceneProgramID);
	glGenTextures(1, &sceneTextureID);
    glUniform1i(glGetUniformLocation(sceneProgramID, "mytexture"), 0);
	glBindTexture(GL_TEXTURE_2D, sceneTextureID);
	glTexParameterf(GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D,
		GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D,
		GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,
		GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glUseProgram(0);
    glCheckError();
	return true;
}

void GLHelper::drawFrame() {
    glCheckError();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, pWindowWidth, pWindowHeight);
    computeMVPMatrix();
    glCheckError();
	glUseProgram(sceneProgramID);
    glCheckError();
	glBindTexture(GL_TEXTURE_2D, sceneTextureID);
    glCheckError();
	glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);

    glCheckError();
    glBindVertexArray(sceneVAO);
    glCheckError();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glCheckError();
    glBindVertexArray(0);
    glCheckError();
	SDL_GL_SwapWindow(pWindow);
    glCheckError();
}

void GLHelper::destroy() {
	if (sceneProgramID) {
		glDeleteProgram(sceneProgramID);
	}
}

bool GLHelper::setupTextureData(unsigned char *rgbData, int frameWidth, int frameHeight) {
	glUseProgram(sceneProgramID);
    glCheckError();
    glBindTexture(GL_TEXTURE_2D, sceneTextureID);
    glCheckError();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameWidth, frameHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbData);
    glCheckError();
    glCheckError();
	return true;
}

bool GLHelper::handleInput() {
	SDL_Event event;
	bool willExit = false;
	while (SDL_PollEvent(&event) != 0) {
		if (event.type == SDL_QUIT) {
			willExit = true;
		} else if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
				willExit = true;
			}
		}
	}
	return willExit;
}

void GLHelper::renderLoop() {
	bool bQuit = false;
	SDL_StartTextInput();
	SDL_ShowCursor(SDL_DISABLE);
	while (! bQuit) {
        drawFrame();
		bQuit = handleInput();
	}
	SDL_StopTextInput();
}

void GLHelper::computeMVPMatrix()
{
	mvpMatrix = projectMatrix * viewMatrix * modelMatrix;
}

void GLHelper::addVertex(float x, float y, float s, float t, std::vector<float> &vertexData) {
	vertexData.push_back(x);
	vertexData.push_back(y);
	vertexData.push_back(s);
	vertexData.push_back(t);
}
