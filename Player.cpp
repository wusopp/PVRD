#include "Player.h"
#include <math.h>
#include <iostream>
#define glCheckError() glCheckError_(__LINE__)

void glCheckError_(int line) {
    GLenum errorCode;
    char error[100];
    memset(error, 0, sizeof(error));
    while((errorCode = glGetError()) != GL_NO_ERROR) {

        switch(errorCode) {
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
    if(compiled != GL_TRUE) {
        std::cout << "Failed to add shader." << std::endl;
    }
    glAttachShader(program, shader);
    glDeleteShader(shader);
}

Player::Player(int numberOfPatches):
    pWindow(NULL),
    pContext(NULL),
    mode(NOT_SPECIFIED),
    watch(new TimeMeasurer()) {
    this->patchNumbers = numberOfPatches;
    init();
}

Player::Player(int width, int height):
    pWindow(NULL),
    pContext(NULL),
    mode(NOT_SPECIFIED),
    patchNumbers(128),
    watch(new TimeMeasurer()),
    frameWidth(width),
    frameHeight(height) {
    init();
}

Player::Player(int width, int height, int numberOfPatches) :
    pWindow(NULL),
    pContext(NULL),
    mode(NOT_SPECIFIED),
    patchNumbers(numberOfPatches),
    watch(new TimeMeasurer()),
    frameWidth(width),
    frameHeight(height) {
    init();
}

Player::~Player() {
    if(vertexCoordinates != NULL) {
        delete[] vertexCoordinates;
        vertexCoordinates = NULL;
    }
    if(uvCoordinates != NULL) {
        delete[] uvCoordinates;
        uvCoordinates = NULL;
    }
    destroyGL();
}

bool Player::init() {
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        std::cout << __FUNCTION__ << "- SDL could not initialize! SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }

    int windowPosX = 100;
    int windowPosY = 100;
    pWindowWidth = 1280;
    pWindowHeight = 640;

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

    pWindow = SDL_CreateWindow("Display CPP", windowPosX, windowPosY, pWindowWidth, pWindowHeight, windowFlags);

    if(pWindow == NULL) {
        std::cout << __FUNCTION__ << "- Window could not be created! SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }

    pContext = SDL_GL_CreateContext(pWindow);
    if(pContext == NULL) {
        std::cout << __FUNCTION__ << "- OpenGL context could not be created! SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }

    glewExperimental = GL_TRUE;
    if(glewInit() != GLEW_OK) {
        std::cout << __FUNCTION__ << "- GLEW could not be inited! SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }
    glGetError();

    if(SDL_GL_SetSwapInterval(0) < 0) {
        std::cout << __FUNCTION__ << "- SDL could not swapInteval! SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }

    if(!setupMatrixes()) {
        std::cout << __FUNCTION__ << "- SetupCamera failed." << std::endl;
        return false;
    }

    if(!setupShaders()) {
        std::cout << __FUNCTION__ << "- SetupGraphics failed." << std::endl;
        return false;
    }
    if(!setupTexture()) {
        std::cout << __FUNCTION__ << "- SetupTexture failed." << std::endl;
        return false;
    }
    return true;
}

void Player::computeViewMatrix() {
    int distanceX = pCurrentXposition - pPreviousXposition;
    int distanceY = pCurrentYposition - pPreviousYposition;

    float DRAG_FACTOR = 0.01f;

    lon = distanceX * DRAG_FACTOR + lon;
    lat = -distanceY * DRAG_FACTOR + lat;

    lat = (float)fmax(-89, fmin(89, lat));

    float phi = (float)glm::radians(90 - lat);
    float theta = (float)glm::radians(lon);

    float camera[3];
    camera[0] = (float)(4 * sin(phi) * cos(theta));
    camera[1] = (float)(4 * cos(phi));
    camera[2] = (float)(4 * sin(phi) * sin(theta));

    viewMatrix = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(camera[0], camera[1], camera[2]), glm::vec3(0, 1, 0));
}



bool Player::setupMatrixes() {
    modelMatrix = glm::mat4(1.0f);

    glm::vec3 cameraPosition(0, 0, 0);
    glm::vec3 cameraLookintAt(0, 0, 1);
    glm::vec3 cameraUp(0, 1, 0);
    viewMatrix = glm::lookAt(cameraPosition, cameraLookintAt, cameraUp);

    setupProjectionMatrix();
    computeMVPMatrix();
    return true;
}

void Player::setupProjectionMatrix() {
    float aspect = pWindowWidth * 1.0f / pWindowHeight;
    projectMatrix = glm::perspective(45.0f, aspect, 0.1f, 40.0f);
}

void Player::drawFrame() {
    if(mode == EQUIRECTANGULAR) {
        drawFrameERP();
    } else if(mode == EQUAL_AREA) {
        drawFrameCpp();
    }
}

bool Player::setupSphereCoordinates() {
    glCheckError();
    this->vertexCount = this->patchNumbers * this->patchNumbers * 3;
    this->vertexCoordinates = new float[this->vertexCount * 3 * sizeof(float)];
    this->uvCoordinates = new float[this->vertexCount * 2 * sizeof(float)];

    int radius = 10;
    int pieces = this->patchNumbers;
    int halfPieces = this->patchNumbers / 2;
    double verticalInterval = M_PI / (halfPieces);
    double horizontalInterval = verticalInterval;
    double latitude;
    double longitude;
    float z[4] = { 0.0f };
    float x[4] = { 0.0f };
    float y[4] = { 0.0f };
    float u[4] = { 0.0f };
    float v[4] = { 0.0f };
    int m = 0, n = 0;
    for(int verticalIndex = 0; verticalIndex < halfPieces; verticalIndex++) {
        latitude = verticalIndex * verticalInterval;
        for(int horizontalIndex = 0; horizontalIndex < pieces; horizontalIndex++) {
            longitude = horizontalIndex * horizontalInterval;
            z[0] = (float)(radius*sin(latitude)*cos(longitude));
            x[0] = (float)(radius*sin(latitude)*sin(longitude));
            y[0] = (float)(radius*cos(latitude));
            u[0] = (float)horizontalIndex / pieces;
            v[0] = (float)verticalIndex / halfPieces;

            z[1] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude));
            x[1] = (float)(radius*sin(latitude + verticalInterval)*sin(longitude));
            y[1] = (float)(radius*cos(latitude + verticalInterval));
            u[1] = (float)horizontalIndex / pieces;
            v[1] = (float)(verticalIndex + 1) / halfPieces;

            z[2] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude + horizontalInterval));
            x[2] = (float)(radius*sin(latitude + verticalInterval)*sin(longitude + horizontalInterval));
            y[2] = (float)(radius*cos(latitude + verticalInterval));
            u[2] = (float)(horizontalIndex + 1) / pieces;
            v[2] = (float)(verticalIndex + 1) / halfPieces;

            z[3] = (float)(radius*sin(latitude)*cos(longitude + horizontalInterval));
            x[3] = (float)(radius*sin(latitude)*sin(longitude + horizontalInterval));
            y[3] = (float)(radius*cos(latitude));
            u[3] = (float)(horizontalIndex + 1) / pieces;
            v[3] = (float)verticalIndex / halfPieces;

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

    int vertexSize = this->vertexCount * 3 * sizeof(float);
    int uvSize = this->vertexCount * 2 * sizeof(float);

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

void Player::computeSTCoordinates(float latitude, float longitude, float &s, float &t) {
    float x, y;
    float H = frameHeight / 2;
    float R = frameHeight / sqrt(3 * M_PI);

    latitude -= M_PI / 2;
    longitude -= M_PI;

    x = sqrt(3 / M_PI) * R * longitude * (2 * cos(2 * latitude / 3) - 1);
    y = sqrt(3 * M_PI) * R * sin(latitude / 3);

    x += frameWidth / 2;
    y += frameHeight / 2;

    s = x / frameWidth;
    t = y / frameHeight;

    printf("longtitude: %lf\t, latitude: %lf\t, s: %lf\t, t: %lf\t\n", glm::degrees(longitude), glm::degrees(latitude), s, t);
}

bool Player::setupCppCoordinates() {
    glCheckError();
    this->vertexCount = this->patchNumbers * this->patchNumbers * 3;
    this->vertexCoordinates = new float[this->vertexCount * 3 * sizeof(float)];
    this->uvCoordinates = new float[this->vertexCount * 2 * sizeof(float)];

    int radius = 10;
    int pieces = this->patchNumbers;
    int halfPieces = this->patchNumbers / 2;
    double verticalInterval = M_PI / (halfPieces);
    double horizontalInterval = verticalInterval;
    double latitude;
    double longitude;
    float z[4] = { 0.0f };
    float x[4] = { 0.0f };
    float y[4] = { 0.0f };
    float u[4] = { 0.0f };
    float v[4] = { 0.0f };
    int m = 0, n = 0;
    for(int verticalIndex = 0; verticalIndex < halfPieces; verticalIndex++) {
        latitude = verticalIndex * verticalInterval;
        for(int horizontalIndex = 0; horizontalIndex < pieces; horizontalIndex++) {
            longitude = horizontalIndex * horizontalInterval;

            z[0] = (float)(radius*sin(latitude)*cos(longitude));
            x[0] = (float)(radius*sin(latitude)*sin(longitude));
            y[0] = (float)(radius*cos(latitude));
            computeSTCoordinates(latitude, longitude, u[0], v[0]);

            z[1] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude));
            x[1] = (float)(radius*sin(latitude + verticalInterval)*sin(longitude));
            y[1] = (float)(radius*cos(latitude + verticalInterval));
            computeSTCoordinates(latitude + verticalInterval, longitude, u[1], v[1]);

            z[2] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude + horizontalInterval));
            x[2] = (float)(radius *sin(latitude + verticalInterval)*sin(longitude + horizontalInterval));
            y[2] = (float)(radius*cos(latitude + verticalInterval));
            computeSTCoordinates(latitude + verticalInterval, longitude + horizontalInterval, u[2], v[2]);


            z[3] = (float)(radius*sin(latitude)*cos(longitude + horizontalInterval));
            x[3] = (float)(radius*sin(latitude)*sin(longitude + horizontalInterval));
            y[3] = (float)(radius*cos(latitude));
            computeSTCoordinates(latitude, longitude + horizontalInterval, u[3], v[3]);


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

    int vertexSize = this->vertexCount * 3 * sizeof(float);
    int uvSize = this->vertexCount * 2 * sizeof(float);

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

bool Player::setupShaders() {
    glCheckError();
    sceneProgramID = glCreateProgram();
    if(!sceneProgramID) {
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
    if(linkStatus != GL_TRUE) {
        printf("Link sceneProgram failed.\n");
        return false;
    }
    sceneMVPMatrixPointer = glGetUniformLocation(sceneProgramID, "matrix");
    if(sceneMVPMatrixPointer == -1) {
        return false;
    }
    return true;
}

bool Player::setupTexture() {
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

bool Player::setupCoordinates() {
    bool result;
    if(this->mode == EQUAL_AREA) {
        result = setupCppCoordinates();
    } else if(this->mode == EQUIRECTANGULAR) {
        result = setupSphereCoordinates();
    } else {
        result = false;
    }
    return result;
}

void Player::drawFrameCpp() {
#ifdef CPP
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, pWindowWidth, pWindowHeight);
    glDisable(GL_DEPTH_TEST);

    computeMVPMatrix();
    glUseProgram(sceneProgramID);

    glBindTexture(GL_TEXTURE_2D, sceneTextureID);
    glCheckError();

    glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);
    // 绘制中部的矩形
    glCheckError();
    glBindVertexArray(sceneVAO);

    glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

    glDrawArrays(GL_TRIANGLES, 0, this->vertexCount);
    glBindVertexArray(0);
    glCheckError();
    SDL_GL_SwapWindow(pWindow);
    glCheckError();
#endif 
}

void Player::drawFrameERP() {
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

    glDrawArrays(GL_TRIANGLES, 0, this->vertexCount);
    glBindVertexArray(0);
    SDL_GL_SwapWindow(pWindow);
    glCheckError();
}

void Player::destroyGL() {
    if(sceneProgramID) {
        glDeleteProgram(sceneProgramID);
    }
}

bool Player::setupTextureData(unsigned char *textureData) {
    assert(frameHeight > 0 && frameWidth > 0);
    glUseProgram(sceneProgramID);
    glCheckError();
    glBindTexture(GL_TEXTURE_2D, sceneTextureID);
    glCheckError();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameWidth, frameHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
    glCheckError();
    return true;
}

bool Player::handleInput() {
    SDL_Event event;
    bool willExit = false;
    static bool isMouseSelected = false;
    while(SDL_PollEvent(&event) != 0) {
        switch(event.type) {
            case SDL_QUIT:
                willExit = true;
                break;
            case SDL_KEYDOWN:
                if(event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                    willExit = true;
                }
                break;
            case SDL_MOUSEMOTION:
                if(isMouseSelected) {
                    SDL_GetMouseState(&pCurrentXposition, &pCurrentYposition);
                    computeViewMatrix();
                    computeMVPMatrix();
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                isMouseSelected = true;
                SDL_GetMouseState(&pPreviousXposition, &pPreviousYposition);
                break;
            case SDL_MOUSEBUTTONUP:
                isMouseSelected = false;
                break;
            case SDL_WINDOWEVENT:
                if(event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    resizeWindow(event);
                }
                break;
            default:
                break;
        }
    }
    return willExit;
}

void Player::resizeWindow(SDL_Event& event) {
    pWindowWidth = event.window.data1;
    pWindowHeight = event.window.data2;
    glViewport(0, 0, pWindowWidth, pWindowHeight);
    setupProjectionMatrix();
    computeMVPMatrix();
}

void Player::renderLoop() {
    assert(mode != NOT_SPECIFIED);
    bool bQuit = false;
    SDL_StartTextInput();
    int frameIndex = 0;
    watch->Start();
    while(!bQuit) {
        drawFrame();
        frameIndex++;
        bQuit = handleInput();
    }
    __int64 time = watch->elapsedMillionSecondsSinceStart();
    double average = 1.0 * time / frameIndex;
    printf("------------------------------\n");
    printf("projection mode is: %s\n", mode == EQUAL_AREA ? "Craster Parabolic Projection" : "Equirectangular Projection");
    std::cout << "Frame count: " << frameIndex << std::endl << "Total time: " << time << " ms." << std::endl << "Average time: " << average << " ms." << std::endl;
    SDL_StopTextInput();
}

void Player::setupProjectionMode(ProjectionMode mode) {
    this->mode = mode;
    setupCoordinates();
}

void Player::computeMVPMatrix() {
    mvpMatrix = projectMatrix * viewMatrix * modelMatrix;
}
