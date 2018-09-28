#include "Player.h"
#include <cmath>
#include <iostream>
#include <string>

#define glCheckError() glCheckError_(__LINE__)
#define logLine() printf("Line:%d, %s\n",__LINE__,__FUNCTION__);

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


namespace Player {
    Player::Player(int numberOfPatches) :
        pWindow(NULL),
        pContext(NULL),
        projectionMode(PM_NOT_SPECIFIED),
        drawMode(DM_NOT_SPECTIFIED),
        timeMeasurer(new TimeMeasurer()),
        vertexArray(NULL),
        uvArray(NULL),
        indexArray(NULL) {
        this->patchNumbers = numberOfPatches;
        init();
    }

    Player::Player(int width, int height, int numberOfPatches) :
        pWindow(NULL),
        pContext(NULL),
        projectionMode(PM_NOT_SPECIFIED),
        drawMode(DM_NOT_SPECTIFIED),
        patchNumbers(numberOfPatches),
        timeMeasurer(new TimeMeasurer()),
        vertexArray(NULL),
        uvArray(NULL),
        indexArray(NULL),
        frameWidth(width),
        frameHeight(height) {
        init();
    }

    Player::~Player() {
        if (vertexArray != NULL) {
            delete[] vertexArray;
            vertexArray = NULL;
        }
        if (uvArray != NULL) {
            delete[] uvArray;
            uvArray = NULL;
        }
        if (indexArray != NULL) {
            delete[] indexArray;
            indexArray = NULL;
        }
        if (timeMeasurer != NULL) {
            delete timeMeasurer;
            timeMeasurer = NULL;
        }
        if (pWindow != NULL) {
            SDL_DestroyWindow(pWindow);
            pWindow = NULL;
        }
        destroyGL();
        SDL_Quit();
    }

    /**
    * Player的初始化函数
    */
    bool Player::init() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
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

        pWindow = SDL_CreateWindow("Panoramic Video Player", windowPosX, windowPosY, pWindowWidth, pWindowHeight, windowFlags);

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
        return true;
    }

    /**
    * 根据鼠标位置计算观察矩阵
    */
    void Player::computeViewMatrix() {
        int distanceX = pCurrentXposition - pPreviousXposition;
        int distanceY = pCurrentYposition - pPreviousYposition;

        float DRAG_FACTOR = 0.01f;

        this->touchPointX = distanceX * DRAG_FACTOR + this->touchPointX;
        this->touchPointY = -distanceY * DRAG_FACTOR + this->touchPointY;

        this->touchPointY = (float)fmax(-89, fmin(89, this->touchPointY));

        float phi = (float)glm::radians(90 - this->touchPointY);
        float theta = (float)glm::radians(this->touchPointX);

        float camera[3];
        camera[0] = (float)(4 * sin(phi) * cos(theta));
        camera[1] = (float)(4 * cos(phi));
        camera[2] = (float)(4 * sin(phi) * sin(theta));

        viewMatrix = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(camera[0], camera[1], camera[2]), glm::vec3(0, 1, 0));
    }

    /**
    * 初始化MVP矩阵
    */
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

    /**
    * 根据视频的投影格式和绘制格式来设置球体模型的顶点坐标与纹理坐标，默认是无索引，ERP
    */
    bool Player::setupCoordinates() {
        bool result;
        if (this->projectionMode == EQUAL_AREA) {
            if (this->drawMode == USE_INDEX) {
                result = _setupCPPCoordinatesWithIndex();
            } else {
                result = _setupCPPCoordinatesWithoutIndex();
            }
        } else if (this->projectionMode == EQUIRECTANGULAR){
            if (this->drawMode == USE_INDEX) {
                result = _setupERPCoordinatesWithIndex();
            } else {
                result = _setupERPCoordinatesWithoutIndex();
            }
        } else if (this->projectionMode == EQUAL_DISTANCE) {
            setupCppEqualDistanceCoordinates();
            result = true;
        }
        return result;
    }

    bool Player::_setupERPCoordinatesWithIndex() {
        glCheckError();

        int radius = 10;
        int pieces = this->patchNumbers;
        int halfPieces = pieces / 2;

        this->vertexCount = ((halfPieces + 1) * (pieces + 1));

        if (this->indexArray) {
            delete[] indexArray;
            indexArray = NULL;
        }
        if (this->vertexArray) {
            delete[] this->vertexArray;
            vertexArray = NULL;
        }
        if (this->uvArray) {
            delete[] this->uvArray;
            uvArray = NULL;
        }

        this->indexArraySize = (pieces)*(halfPieces)* 6;
        this->vertexArray = new float[this->vertexCount * 3];
        this->uvArray = new float[this->vertexCount * 2];
        this->indexArray = new int[this->indexArraySize];

        double verticalInterval = M_PI / halfPieces;
        double horizontalInterval = verticalInterval;

        double latitude, longitude;
        float xt, yt, zt;
        float ut, vt;

        int m = 0, n = 0;
        for (int verticalIndex = 0; verticalIndex <= halfPieces; verticalIndex++) {
            latitude = verticalIndex * verticalInterval;
            for (int horizontalIndex = 0; horizontalIndex <= pieces; horizontalIndex++) {
                longitude = horizontalIndex * horizontalInterval;

                zt = (float)(radius*sin(latitude)*cos(longitude));
                xt = (float)(radius*sin(latitude)*sin(longitude));
                yt = (float)(radius*cos(latitude));

                ut = 1.0f * horizontalIndex / pieces;
                vt = 1.0f * verticalIndex / halfPieces;
                this->vertexArray[m++] = xt;
                this->vertexArray[m++] = yt;
                this->vertexArray[m++] = zt;
                this->uvArray[n++] = ut;
                this->uvArray[n++] = vt;
            }
        }

        m = 0;
        for (int i = 1; i <= halfPieces; i++) {
            for (int j = 0; j <= pieces - 1; j++) {
                // 第index个矩形,0-1-2, 2-3-0
                // 1---2
                // |  /|
                // | / |
                // |/  |
                // 0---3
                indexArray[m++] = (i - 1) * (pieces + 1) + j;
                indexArray[m++] = i * (pieces + 1) + j;
                indexArray[m++] = i * (pieces + 1) + j + 1;
                indexArray[m++] = i * (pieces + 1) + j + 1;
                indexArray[m++] = (i - 1) * (pieces + 1) + j + 1;
                indexArray[m++] = (i - 1) * (pieces + 1) + j;
            }
        }

        glGenVertexArrays(1, &sceneVAO);
        glBindVertexArray(sceneVAO);

        int vertexBufferSize = this->vertexCount * 3 * sizeof(float);
        int uvBufferSize = this->vertexCount * 2 * sizeof(float);
        int indexBufferSize = this->indexArraySize * sizeof(int);

        glGenBuffers(1, &sceneVertBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, this->vertexArray, GL_STATIC_DRAW);

        glGenBuffers(1, &sceneUVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
        glBufferData(GL_ARRAY_BUFFER, uvBufferSize, this->uvArray, GL_STATIC_DRAW);

        glGenBuffers(1, &sceneIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sceneIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, this->indexArray, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glCheckError();
        return true;
    }


    /**
    * 设置绘制ERP格式视频的球体模型的坐标，不使用索引
    */
    bool Player::_setupERPCoordinatesWithoutIndex() {
        glCheckError();
        this->vertexCount = this->patchNumbers * this->patchNumbers * 3;

        if (this->vertexArray) {
            delete[] this->vertexArray;
        }
        if (this->uvArray) {
            delete[] this->uvArray;
        }
        this->vertexArray = new float[this->vertexCount * 3];
        this->uvArray = new float[this->vertexCount * 2];


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
        for (int verticalIndex = 0; verticalIndex < halfPieces; verticalIndex++) {
            latitude = verticalIndex * verticalInterval;
            for (int horizontalIndex = 0; horizontalIndex < pieces; horizontalIndex++) {
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

                this->vertexArray[m++] = x[0];
                this->vertexArray[m++] = y[0];
                this->vertexArray[m++] = z[0];
                this->uvArray[n++] = u[0];
                this->uvArray[n++] = v[0];

                this->vertexArray[m++] = x[1];
                this->vertexArray[m++] = y[1];
                this->vertexArray[m++] = z[1];
                this->uvArray[n++] = u[1];
                this->uvArray[n++] = v[1];

                this->vertexArray[m++] = x[2];
                this->vertexArray[m++] = y[2];
                this->vertexArray[m++] = z[2];
                this->uvArray[n++] = u[2];
                this->uvArray[n++] = v[2];

                this->vertexArray[m++] = x[2];
                this->vertexArray[m++] = y[2];
                this->vertexArray[m++] = z[2];
                this->uvArray[n++] = u[2];
                this->uvArray[n++] = v[2];


                this->vertexArray[m++] = x[3];
                this->vertexArray[m++] = y[3];
                this->vertexArray[m++] = z[3];
                this->uvArray[n++] = u[3];
                this->uvArray[n++] = v[3];

                this->vertexArray[m++] = x[0];
                this->vertexArray[m++] = y[0];
                this->vertexArray[m++] = z[0];
                this->uvArray[n++] = u[0];
                this->uvArray[n++] = v[0];
            }
        }

        glGenVertexArrays(1, &sceneVAO);
        glBindVertexArray(sceneVAO);

        int vertexSize = this->vertexCount * 3 * sizeof(float);
        int uvSize = this->vertexCount * 2 * sizeof(float);

        glGenBuffers(1, &sceneVertBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexSize, this->vertexArray, GL_STATIC_DRAW);

        glGenBuffers(1, &sceneUVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
        glBufferData(GL_ARRAY_BUFFER, uvSize, this->uvArray, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glCheckError();
        return true;
    }

    /**
    * 设置绘制CPP格式视频的球体模型的坐标，不使用索引
    */
    bool Player::_setupCPPCoordinatesWithoutIndex() {
        glCheckError();
        this->vertexCount = (this->patchNumbers) * (this->patchNumbers / 2) * 6;
        if (this->vertexArray) {
            delete[] this->vertexArray;
        }
        this->vertexArray = new float[this->vertexCount * 3];
        if (this->uvArray) {
            delete[] this->uvArray;
        }
        this->uvArray = new float[this->vertexCount * 2];

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

        for (int verticalIndex = 0; verticalIndex < halfPieces; verticalIndex++) {
            latitude = verticalIndex * verticalInterval;
            for (int horizontalIndex = 0; horizontalIndex < pieces; horizontalIndex++) {
                longitude = horizontalIndex * horizontalInterval;

                z[0] = (float)(radius*sin(latitude)*cos(longitude));
                x[0] = (float)(radius*sin(latitude)*sin(longitude));
                y[0] = (float)(radius*cos(latitude));
                computeCppEqualAreaUVCoordinates(latitude, longitude, u[0], v[0]);

                z[1] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude));
                x[1] = (float)(radius*sin(latitude + verticalInterval)*sin(longitude));
                y[1] = (float)(radius*cos(latitude + verticalInterval));
                computeCppEqualAreaUVCoordinates(latitude + verticalInterval, longitude, u[1], v[1]);

                z[2] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude + horizontalInterval));
                x[2] = (float)(radius *sin(latitude + verticalInterval)*sin(longitude + horizontalInterval));
                y[2] = (float)(radius*cos(latitude + verticalInterval));
                computeCppEqualAreaUVCoordinates(latitude + verticalInterval, longitude + horizontalInterval, u[2], v[2]);


                z[3] = (float)(radius*sin(latitude)*cos(longitude + horizontalInterval));
                x[3] = (float)(radius*sin(latitude)*sin(longitude + horizontalInterval));
                y[3] = (float)(radius*cos(latitude));
                computeCppEqualAreaUVCoordinates(latitude, longitude + horizontalInterval, u[3], v[3]);


                this->vertexArray[m++] = x[0];
                this->vertexArray[m++] = y[0];
                this->vertexArray[m++] = z[0];
                this->uvArray[n++] = u[0];
                this->uvArray[n++] = v[0];

                this->vertexArray[m++] = x[1];
                this->vertexArray[m++] = y[1];
                this->vertexArray[m++] = z[1];
                this->uvArray[n++] = u[1];
                this->uvArray[n++] = v[1];

                this->vertexArray[m++] = x[2];
                this->vertexArray[m++] = y[2];
                this->vertexArray[m++] = z[2];
                this->uvArray[n++] = u[2];
                this->uvArray[n++] = v[2];

                this->vertexArray[m++] = x[2];
                this->vertexArray[m++] = y[2];
                this->vertexArray[m++] = z[2];
                this->uvArray[n++] = u[2];
                this->uvArray[n++] = v[2];

                this->vertexArray[m++] = x[3];
                this->vertexArray[m++] = y[3];
                this->vertexArray[m++] = z[3];
                this->uvArray[n++] = u[3];
                this->uvArray[n++] = v[3];

                this->vertexArray[m++] = x[0];
                this->vertexArray[m++] = y[0];
                this->vertexArray[m++] = z[0];
                this->uvArray[n++] = u[0];
                this->uvArray[n++] = v[0];
            }
        }

        glGenVertexArrays(1, &sceneVAO);
        glBindVertexArray(sceneVAO);

        int vertexSize = this->vertexCount * 3 * sizeof(float);
        int uvSize = this->vertexCount * 2 * sizeof(float);

        glGenBuffers(1, &sceneVertBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexSize, this->vertexArray, GL_STATIC_DRAW);

        glGenBuffers(1, &sceneUVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
        glBufferData(GL_ARRAY_BUFFER, uvSize, this->uvArray, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glCheckError();
        return true;
    }

    /**
    * 设置绘制CPP格式视频的球体模型的坐标，使用索引
    */
    bool Player::_setupCPPCoordinatesWithIndex() {
        glCheckError();

        int radius = 10;
        int pieces = this->patchNumbers;
        int halfPieces = pieces / 2;

        this->vertexCount = ((halfPieces + 1) * (pieces + 1));
        if (this->indexArray) {
            delete[] indexArray;
            this->indexArray = NULL;
        }
        if (this->vertexArray) {
            delete[] this->vertexArray;
            this->vertexArray = NULL;
        }
        if (this->uvArray) {
            delete[] this->uvArray;
            this->uvArray = NULL;
        }

        this->indexArraySize = (pieces)*(halfPieces)* 6;
        this->vertexArray = new float[this->vertexCount * 3];
        this->uvArray = new float[this->vertexCount * 2];
        this->indexArray = new int[this->indexArraySize];


        double verticalInterval = M_PI / halfPieces;
        double horizontalInterval = verticalInterval;

        double latitude, longitude;
        float xt, yt, zt;
        float ut, vt;

        int m = 0, n = 0;
        for (int verticalIndex = 0; verticalIndex <= halfPieces; verticalIndex++) {
            latitude = verticalIndex * verticalInterval;
            for (int horizontalIndex = 0; horizontalIndex <= pieces; horizontalIndex++) {
                longitude = horizontalIndex * horizontalInterval;

                zt = (float)(radius*sin(latitude)*cos(longitude));
                xt = (float)(radius*sin(latitude)*sin(longitude));
                yt = (float)(radius*cos(latitude));

                computeCppEqualAreaUVCoordinates(latitude, longitude, ut, vt);
                this->vertexArray[m++] = xt;
                this->vertexArray[m++] = yt;
                this->vertexArray[m++] = zt;
                this->uvArray[n++] = ut;
                this->uvArray[n++] = vt;
            }
        }

        m = 0;
        for (int i = 1; i <= halfPieces; i++) {
            for (int j = 0; j <= pieces - 1; j++) {
                // 第index个矩形,0-1-2, 2-3-0
                // 1---2
                // |  /|
                // | / |
                // |/  |
                // 0---3
                int index = i * (pieces + 1) + j;
                indexArray[m++] = (i - 1) * (pieces + 1) + j;
                indexArray[m++] = i * (pieces + 1) + j;
                indexArray[m++] = i * (pieces + 1) + j + 1;
                indexArray[m++] = i * (pieces + 1) + j + 1;
                indexArray[m++] = (i - 1) * (pieces + 1) + j + 1;
                indexArray[m++] = (i - 1) * (pieces + 1) + j;
            }
        }


        glGenVertexArrays(1, &sceneVAO);
        glBindVertexArray(sceneVAO);

        int vertexBufferSize = this->vertexCount * 3 * sizeof(float);
        int uvBufferSize = this->vertexCount * 2 * sizeof(float);
        int indexBufferSize = this->indexArraySize * sizeof(int);

        glGenBuffers(1, &sceneVertBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, this->vertexArray, GL_STATIC_DRAW);

        glGenBuffers(1, &sceneUVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
        glBufferData(GL_ARRAY_BUFFER, uvBufferSize, this->uvArray, GL_STATIC_DRAW);

        glGenBuffers(1, &sceneIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sceneIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, this->indexArray, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glCheckError();
        return true;
    }

    /**
    * 根据球体上点的经纬度，计算对应的纹理坐标
    */
    void Player::computeCppEqualAreaUVCoordinates(float latitude, float longitude, float &s, float &t) {
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
    }


    void Player::computeCppEqualAreaUVCoordinates(double latitude, double longitude, double &u, double &v) {
        double x, y;
        double H = frameHeight / 2;
        double R = frameHeight / sqrt(3 * M_PI);

        latitude -= M_PI / 2;
        longitude -= M_PI;

        x = sqrt(3 / M_PI) * R * longitude * (2 * cos(2 * latitude / 3) - 1);
        y = sqrt(3 * M_PI) * R * sin(latitude / 3);

        x += frameWidth / 2;
        y += frameHeight / 2;

        u = x / frameWidth;
        v = y / frameHeight;
    }

    

    /**
    * 根据投影格式来调用不同的渲染方法
    */
    void Player::drawFrame() {
        if (this->projectionMode == EQUIRECTANGULAR) {
            if (drawMode == USE_INDEX) {
                _drawFrameERPWithIndex();
            } else {
                _drawFrameERPWithoutIndex();
            }

        } else if (this->projectionMode == EQUAL_AREA) {
            if (drawMode == USE_INDEX) {
                _drawFrameCPPWithIndex();
            } else {
                _drawFrameCPPWithoutIndex();
            }
        } else if (this->projectionMode == EQUAL_DISTANCE) {
            _drawFrameCppEqualDistance();
        }
    }



    void Player::_drawFrameERPWithIndex() {
        /*glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
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

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sceneIndexBuffer);
        glDrawElements(GL_TRIANGLES, this->indexArraySize, GL_UNSIGNED_INT, (const void *)0);

        glBindVertexArray(0);
        SDL_GL_SwapWindow(pWindow);
        glCheckError();
    }



    /**
    * 绘制投影格式为ERP的视频帧
    */
    void Player::_drawFrameERPWithoutIndex() {
        /*glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
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

    /**
    * 绘制投影格式为CPP的视频帧
    */
    void Player::_drawFrameCPPWithIndex() {
        /*glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
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

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sceneIndexBuffer);

        glDrawElements(GL_TRIANGLES, this->indexArraySize, GL_UNSIGNED_INT, (const void *)0);

        glBindVertexArray(0);
        SDL_GL_SwapWindow(pWindow);
        glCheckError();
    }

    void Player::_drawFrameCPPWithoutIndex() {
        /*glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
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

    /**
    * 销毁OpenGL上下文
    */
    void Player::destroyGL() {
        if (sceneProgramID) {
            glDeleteProgram(sceneProgramID);
        }
    }

    /**
    * 设置视频帧数据
    */
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

    /**
    * 设置投影和绘图格式
    */
    void Player::setupMode(ProjectionMode projection, DrawMode draw) {
        this->projectionMode = projection;
        this->drawMode = draw;
        setupCoordinates();
    }

    /**
    * 计算MVP矩阵
    */
    void Player::computeMVPMatrix() {
        mvpMatrix = projectMatrix * viewMatrix * modelMatrix;
    }

    /**
    * 处理键盘输入、鼠标拖拽、窗口放缩等事件
    */
    bool Player::handleInput() {
        SDL_Event event;
        bool willExit = false;
        static bool isMouseSelected = false;
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
            case SDL_QUIT:
                willExit = true;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                    willExit = true;
                }
                break;
            case SDL_MOUSEMOTION:
                if (isMouseSelected) {
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
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    resizeWindow(event);
                }
                break;
            default:
                break;
            }
        }
        return willExit;
    }

    /**
    * 处理窗口的缩放事件，重新设置视点
    */
    void Player::resizeWindow(SDL_Event& event) {
        pWindowWidth = event.window.data1;
        pWindowHeight = event.window.data2;
        glViewport(0, 0, pWindowWidth, pWindowHeight);
        setupProjectionMatrix();
        computeMVPMatrix();
    }

    /**
    * 编译顶点着色器与片元着色器
    */
    bool Player::setupShaders() {
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

    /**
    * 设置纹理参数
    */
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


    /**
    * 主渲染循环
    */
    void Player::renderLoop() {
        assert(projectionMode != PM_NOT_SPECIFIED);
        assert(drawMode != DM_NOT_SPECTIFIED);
        bool bQuit = false;
        SDL_StartTextInput();
        int frameIndex = 0;
        timeMeasurer->Start();
        while (!bQuit) {
            drawFrame();
            frameIndex++;
            bQuit = handleInput();
        }
        __int64 time = timeMeasurer->elapsedMillionSecondsSinceStart();
        double average = 1.0 * time / frameIndex;

        std::string projectionMode;
        switch (this->projectionMode) {
        case EQUAL_AREA:
            projectionMode = "Craster Parabolic Projection";
            break;
        case EQUAL_DISTANCE:
            projectionMode = "Equal Distance Projection";
            break;
        case EQUIRECTANGULAR:
            projectionMode = "Equirectangular Projection";
            break;
        default:
            projectionMode = " ";
            break;
        }

        std::cout << "projection mode is: " << projectionMode << std::endl;
        std::cout << "Frame count: " << frameIndex << std::endl << "Total time: " << time << " ms." << std::endl << "Average time: " << average << " ms." << std::endl;
        std::cout << "------------------------------" << std::endl;
        SDL_StopTextInput();
    }
}

namespace Player {
    void Player::organizeVerts(std::vector<std::vector<VertexStruct> > & allVerts) {
        if (this->vertexVector.size() > 0) {
            this->vertexVector.clear();
        }
        if (this->uvVector.size() > 0) {
            this->uvVector.clear();
        }

        int levelCount = allVerts.size();
        for (int level = 0; level <= levelCount - 2; level++) {
            int vertCountForLevel = allVerts[level].size();
            int vertCountForNextLevel = allVerts[level + 1].size();

            int pointerForLevel = 0, pointerForNextLevel = 0;

            while (pointerForLevel < vertCountForLevel-1 && pointerForNextLevel < vertCountForNextLevel-1) {
                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].x);
                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].y);
                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].z);
                this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].u);
                this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].v);

                this->vertexVector.push_back(allVerts[level][pointerForLevel].x);
                this->vertexVector.push_back(allVerts[level][pointerForLevel].y);
                this->vertexVector.push_back(allVerts[level][pointerForLevel].z);
                this->uvVector.push_back(allVerts[level][pointerForLevel].u);
                this->uvVector.push_back(allVerts[level][pointerForLevel].v);

                this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].x);
                this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].y);
                this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].z);
                this->uvVector.push_back(allVerts[level][pointerForLevel + 1].u);
                this->uvVector.push_back(allVerts[level][pointerForLevel + 1].v);

                this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].x);
                this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].y);
                this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].z);
                this->uvVector.push_back(allVerts[level][pointerForLevel + 1].u);
                this->uvVector.push_back(allVerts[level][pointerForLevel + 1].v);

                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].x);
                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].y);
                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].z);
                this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].u);
                this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].v);

                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].x);
                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].y);
                this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].z);
                this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].u);
                this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].v);


                pointerForLevel++;
                pointerForNextLevel++;
            }

            if (pointerForLevel == vertCountForLevel - 1) {
                while (pointerForNextLevel < vertCountForNextLevel - 1) {
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].x);
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].y);
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].z);
                    this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].u);
                    this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].v);

                    this->vertexVector.push_back(allVerts[level][pointerForLevel].x);
                    this->vertexVector.push_back(allVerts[level][pointerForLevel].y);
                    this->vertexVector.push_back(allVerts[level][pointerForLevel].z);
                    this->uvVector.push_back(allVerts[level][pointerForLevel].u);
                    this->uvVector.push_back(allVerts[level][pointerForLevel].v);

                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].x);
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].y);
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].z);
                    this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].u);
                    this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel + 1].v);

                    pointerForNextLevel++;
                }
            } else if (pointerForNextLevel == vertCountForNextLevel - 1) {
                while (pointerForLevel < vertCountForLevel - 1) {
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].x);
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].y);
                    this->vertexVector.push_back(allVerts[level + 1][pointerForNextLevel].z);
                    this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].u);
                    this->uvVector.push_back(allVerts[level + 1][pointerForNextLevel].v);

                    this->vertexVector.push_back(allVerts[level][pointerForLevel].x);
                    this->vertexVector.push_back(allVerts[level][pointerForLevel].y);
                    this->vertexVector.push_back(allVerts[level][pointerForLevel].z);
                    this->uvVector.push_back(allVerts[level][pointerForLevel].u);
                    this->uvVector.push_back(allVerts[level][pointerForLevel].v);

                    this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].x);
                    this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].y);
                    this->vertexVector.push_back(allVerts[level][pointerForLevel + 1].z);
                    this->uvVector.push_back(allVerts[level][pointerForLevel + 1].u);
                    this->uvVector.push_back(allVerts[level][pointerForLevel + 1].v);

                    pointerForLevel++;
                }
            }
        }
    }

    /**
    * 设置绘制CPP格式视频的球体模型的坐标，使用索引，分区绘制
    */
    bool Player::setupCppEqualDistanceCoordinates() {

        // 设置顶点坐标、纹理坐标与索引
        glCheckError();

        if (this->vertexArray) {
            delete[] this->vertexArray;
            this->vertexArray = NULL;
        }
        if (this->uvArray) {
            delete[] this->uvArray;
            this->uvArray = NULL;
        }
        if (this->indexArray) {
            delete[] this->indexArray;
            this->indexArray = NULL;
        }

        if (this->vertexVector.size() > 0) {
            this->vertexVector.clear();
        }
        if (this->uvVector.size() > 0) {
            this->uvVector.clear();
        }
        if (this->indexVector.size() > 0) {
            this->indexVector.clear();
        }

        int H = this->frameHeight / 2;

        float SQRT_3_M_PI = sqrt(3 * M_PI);
        float SQRT_3_DIVIDE_M_PI = sqrt(3 / M_PI);

        float R = 2 * H / SQRT_3_M_PI;

        int radius = 10;

        std::vector<std::vector<VertexStruct>> allVerts;

        for (float j = H; j >= -H; j -= 10.0f) {

            std::vector<VertexStruct> verts;

            if (j == H) {
                float z = 0.0f;
                float x = 0.0f;
                float y = radius;
                float u = 0.5f;
                float v = 1.0f;
                VertexStruct vertexStruct(x, y, z, u, v);
                verts.push_back(vertexStruct);
                allVerts.push_back(verts);
                continue;
            } else if (j == -H) {
                float z = 0;
                float x = 0;
                float y = -radius;
                float u = 0.5f;
                float v = 0.0f;
                VertexStruct vertexStruct(x, y, z, u, v);
                verts.push_back(vertexStruct);
                allVerts.push_back(verts);
                continue;
            }

            float latitude = asin(j / (SQRT_3_M_PI*R)) * 3;

            float leftmost = SQRT_3_DIVIDE_M_PI * R * (2 * cos(2 * latitude / 3) - 1) * (-M_PI);

            float rightmost = -leftmost;

            for (float i = leftmost; i < rightmost; i += 10.0f) {
                float longitude = i / (SQRT_3_DIVIDE_M_PI * R * (2 * cos(2 * latitude / 3) - 1));

                longitude += M_PI;

                float z = radius * cos(latitude) * cos(longitude);
                float x = radius * cos(latitude) * sin(longitude);
                float y = radius * sin(latitude);

                float u = 0.0f, v = 0.0f;
                computeCppEqualDistanceUVCoordinates(i, j, u, v);

                VertexStruct vertexStruct(x, y, z, u, v);

                verts.push_back(vertexStruct);
            }

            float rightmost_longitude = rightmost / (SQRT_3_DIVIDE_M_PI * R * (2 * cos(2 * latitude / 3) - 1));
            rightmost_longitude += M_PI;

            float z = radius * cos(latitude) * cos(rightmost_longitude);
            float x = radius * cos(latitude) * sin(rightmost_longitude);
            float y = radius * sin(latitude);

            float u = 0.0f, v = 0.0f;
            computeCppEqualDistanceUVCoordinates(rightmost, j, u, v);
            VertexStruct vertexStruct(x, y, z, u, v);
            verts.push_back(vertexStruct);

            allVerts.push_back(verts);
        }

        this->organizeVerts(allVerts);

        glGenVertexArrays(1, &sceneVAO);
        glBindVertexArray(sceneVAO);

        int vertexDataSize = this->vertexVector.size() * sizeof(float);
        int uvDataSize = this->uvVector.size() * sizeof(float);

        glGenBuffers(1, &sceneVertBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneVertBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexDataSize, &this->vertexVector[0], GL_STATIC_DRAW);

        glGenBuffers(1, &sceneUVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
        glBufferData(GL_ARRAY_BUFFER, uvDataSize, &this->uvVector[0], GL_STATIC_DRAW);

        glBindVertexArray(0);
        glCheckError();
        return true;
    }

    void Player::_drawFrameCppEqualDistance() {
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

        glDrawArrays(GL_TRIANGLES, 0, this->vertexVector.size() / 3);
        glBindVertexArray(0);
        SDL_GL_SwapWindow(pWindow);
        glCheckError();
    }

    void Player::computeCppEqualDistanceUVCoordinates(float x, float y, float &u, float &v) {
        x += this->frameWidth / 2;
        y += this->frameHeight / 2;

        u = x / this->frameWidth;
        v = y / this->frameHeight;

        v = 1.0f - v;
    }
}

