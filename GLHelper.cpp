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

static const GLfloat quadData[] = {
    -1.0f,-1.0f,0.0f,1.0f,
    1.0f,-1.0f,0.0f,1.0f,
    1.0f,1.0f,0.0f,1.0f,
    -1.0f,1.0f,0.0f,1.0f,
    0.0f,1.0f,
    1.0f,1.0f,
    1.0f,0.0f,
    0.0f,0.0f
};

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

GLHelper::GLHelper() :pWindow(NULL), pContext(NULL), pNumberOfPatches(64),watch(new CStopwatch()) {
    this->pVertexCount = 12288;
    {
        this->vertexCoordinates = new float[this->pVertexCount * 3 * sizeof(float)];
    }
    {
        this->uvCoordinates = new float[this->pVertexCount * 2 * sizeof(float)];
    }
}

GLHelper::GLHelper(int numberOfPatches) : pWindow(NULL), pContext(NULL), watch(new CStopwatch()) {
    this->pNumberOfPatches = numberOfPatches;
    this->pVertexCount = this->pNumberOfPatches * this->pNumberOfPatches * 3;
    this->vertexCoordinates = new float[this->pVertexCount * 3 * sizeof(float)];
    this->uvCoordinates = new float[this->pVertexCount * 2 * sizeof(float)];
}

GLHelper::~GLHelper() {
    delete[] vertexCoordinates;
    delete[] uvCoordinates;
    destroy();
}

bool GLHelper::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        std::cout << __FUNCTION__ << "- SDL could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }

    int windowPosX = 100;
    int windowPosY = 100;
    pWindowWidth = 640;
    pWindowHeight = 360;

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
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

    if (!setupMatrixes()) {
        std::cout << __FUNCTION__ << "- SetupCamera failed." << std::endl;
        return false;
    }

    if (!setupShaders()) {
        std::cout << __FUNCTION__ << "- SetupGraphics failed." << std::endl;
        return false;
    }
    if (!setupTexture()) {
        std::cout << __FUNCTION__ << "- SetupTexture failed." << std::endl;
        return false;
    }
    if (!setupSphereCoordinates()) {
        std::cout << __FUNCTION__ << "- SetupScene failed." << std::endl;
        return false;
    }
    return true;
}

void GLHelper::computeViewMatrix() {
    int distanceX = pCurrentXposition - pPreviousXposition;
    int distanceY = pCurrentYposition - pPreviousYposition;

    float DRAG_FACTOR = 0.01f;

    lon = distanceX * DRAG_FACTOR + lon;
    lat = -distanceY * DRAG_FACTOR + lat;

    lat = (float)fmax(-85, fmin(85, lat));

    float phi = (float)glm::radians(90 - lat);
    float theta = (float)glm::radians(lon);

    float camera[3];
    camera[0] = (float)(4 * sin(phi) * cos(theta));
    camera[1] = (float)(4 * cos(phi));
    camera[2] = (float)(4 * sin(phi) * sin(theta));

    viewMatrix = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(camera[0], camera[1], camera[2]), glm::vec3(0, 1, 0));
}

bool GLHelper::setupMatrixes() {
    modelMatrix = glm::mat4(1.0f);

    glm::vec3 cameraPosition(0, 0, 0);
    glm::vec3 cameraLookintAt(0, 0, 1);
    glm::vec3 cameraUp(0, 1, 0);
    viewMatrix = glm::lookAt(cameraPosition, cameraLookintAt, cameraUp);

    setupProjectionMatrix();

    computeMVPMatrix();
    return true;
}

void GLHelper::setupProjectionMatrix() {
    float aspect = pWindowWidth * 1.0f / pWindowHeight;
    projectMatrix = glm::perspective(45.0f, aspect, 0.1f, 40.0f);
}

bool GLHelper::setupSphereCoordinates() {
    glCheckError();
    int radius = 10;
    int pieces = this->pNumberOfPatches;
    int half_pieces = this->pNumberOfPatches / 2;
    double step_z = M_PI / (half_pieces);
    double step_xy = step_z;
    double angle_z;
    double angle_xy;
    float z[4] = { 0.0f };
    float x[4] = { 0.0f };
    float y[4] = { 0.0f };
    float u[4] = { 0.0f };
    float v[4] = { 0.0f };
    int m = 0, n = 0;
    for (int i = 0; i < half_pieces; i++) {
        angle_z = i * step_z;
        for (int j = 0; j < pieces; j++) {
            angle_xy = j * step_xy;
            z[0] = (float)(radius * sin(angle_z)*cos(angle_xy));
            x[0] = (float)(radius*sin(angle_z)*sin(angle_xy));
            y[0] = (float)(radius*cos(angle_z));
            u[0] = (float)j / pieces;
            v[0] = (float)i / half_pieces;
            z[1] = (float)(radius*sin(angle_z + step_z)*cos(angle_xy));
            x[1] = (float)(radius*sin(angle_z + step_z)*sin(angle_xy));
            y[1] = (float)(radius*cos(angle_z + step_z));
            u[1] = (float)j / pieces;
            v[1] = (float)(i + 1) / half_pieces;
            z[2] = (float)(radius*sin(angle_z + step_z)*cos(angle_xy + step_xy));
            x[2] = (float)(radius *sin(angle_z + step_z)*sin(angle_xy + step_xy));
            y[2] = (float)(radius*cos(angle_z + step_z));
            u[2] = (float)(j + 1) / pieces;
            v[2] = (float)(i + 1) / half_pieces;
            z[3] = (float)(radius*sin(angle_z)*cos(angle_xy + step_xy));
            x[3] = (float)(radius*sin(angle_z)*sin(angle_xy + step_xy));
            y[3] = (float)(radius*cos(angle_z));
            u[3] = (float)(j + 1) / pieces;
            v[3] = (float)i / half_pieces;
            this->vertexCoordinates[m++] = x[0];
            this->vertexCoordinates[m++] = y[0];
            this->vertexCoordinates[m++] = z[0];
            this->uvCoordinates[n++] = u[0];
            this->uvCoordinates[n++] = v[0];
            this->vertexCoordinates[m++] = x[1];
            this->vertexCoordinates[m++] = y[1];
            this->vertexCoordinates[m++] = z[1];
            this->uvCoordinates[n++] = u[1];
            this->uvCoordinates[n++] = v[1];
            this->vertexCoordinates[m++] = x[2];
            this->vertexCoordinates[m++] = y[2];
            this->vertexCoordinates[m++] = z[2];
            this->uvCoordinates[n++] = u[2];
            this->uvCoordinates[n++] = v[2];
            this->vertexCoordinates[m++] = x[2];
            this->vertexCoordinates[m++] = y[2];
            this->vertexCoordinates[m++] = z[2];
            this->uvCoordinates[n++] = u[2];
            this->uvCoordinates[n++] = v[2];
            this->vertexCoordinates[m++] = x[3];
            this->vertexCoordinates[m++] = y[3];
            this->vertexCoordinates[m++] = z[3];
            this->uvCoordinates[n++] = u[3];
            this->uvCoordinates[n++] = v[3];
            this->vertexCoordinates[m++] = x[0];
            this->vertexCoordinates[m++] = y[0];
            this->vertexCoordinates[m++] = z[0];
            this->uvCoordinates[n++] = u[0];
            this->uvCoordinates[n++] = v[0];
        }
    }



    glGenVertexArrays(1, &sceneVAO);
    glBindVertexArray(sceneVAO);

    int vertexSize = this->pVertexCount * 3 * sizeof(float);
    int uvSize = this->pVertexCount * 2 * sizeof(float);

    glGenBuffers(1, &sceneVertBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexSize, this->vertexCoordinates, GL_STATIC_DRAW);

    glGenBuffers(1, &sceneUVBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
    glBufferData(GL_ARRAY_BUFFER, uvSize, this->uvCoordinates, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glCheckError();
    return true;
}

bool GLHelper::setupCppCoordinates() {
    return false;
}

bool GLHelper::setupShaders() {
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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, pWindowWidth, pWindowHeight);
    glDisable(GL_DEPTH_TEST);

    computeMVPMatrix();
    glUseProgram(sceneProgramID);

    glBindTexture(GL_TEXTURE_2D, sceneTextureID);
    glCheckError();

    glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);

    glBindVertexArray(sceneVAO);

    glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

    glDrawArrays(GL_TRIANGLES, 0, this->pVertexCount);
    glBindVertexArray(0);
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
    return true;
}

bool GLHelper::handleInput() {
    SDL_Event event;
    bool willExit = false;
    static bool isMouseSelected = false;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            willExit = true;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                willExit = true;
            }
        } else if (event.type == SDL_MOUSEMOTION) {
            if (isMouseSelected) {
                SDL_GetMouseState(&pCurrentXposition, &pCurrentYposition);
                computeViewMatrix();
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            isMouseSelected = true;
            SDL_GetMouseState(&pPreviousXposition, &pPreviousYposition);
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            isMouseSelected = false;
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                resizeWindow(event);
            }
        }
    }
    return willExit;
}

void GLHelper::resizeWindow(SDL_Event& event) {
    pWindowWidth = event.window.data1;
    pWindowHeight = event.window.data2;
    setupProjectionMatrix();
    computeMVPMatrix();
}

void GLHelper::renderLoop() {

    bool bQuit = false;
    SDL_StartTextInput();
    static int frameIndex = 0;
    while (!bQuit) {
        watch->Start();
        drawFrame();
        __int64 time = watch->elapsedMicroSecondsSinceStart();
        std::cout << "frameIndex: " << frameIndex << ", renderFrame costs " << time << " us." << std::endl;
        frameIndex++;
        bQuit = handleInput();
    }
    SDL_StopTextInput();
}

void GLHelper::computeMVPMatrix() {
    mvpMatrix = projectMatrix * viewMatrix * modelMatrix;
}
