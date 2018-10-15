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
#include <string>
#include "gtc/matrix_transform.hpp"
#include "gtc/constants.hpp"
#include "TimeMeasurer.h"
#include <fstream>



extern "C"
{
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avdevice.lib")
#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avutil.lib")
#pragma comment (lib, "swresample.lib")
#pragma comment (lib, "swscale.lib")
};


enum ProjectionMode {
    PM_ERP = 0, // ERP��ʽ
    PM_CPP_OBSOLETE,
    PM_CPP, // CPP��ʽ
    PM_NOT_SPECIFIED, // δָ����ʽ
};

enum DrawMode {
    DM_USE_INDEX = 0, // ʹ���������л���
    DM_DONT_USE_INDEX, // ��ʹ���������л���
    DM_NOT_SPECTIFIED // ���Ʒ���δָ��
};

enum VideoFileType {
    VFT_YUV = 0, // YUV Raw��ʽ
    VFT_Encoded // ������װ����Ƶ��ʽ��mp4
};


typedef struct VertexStruct {
    float x;
    float y;
    float z;
    float u;
    float v;
    int index;
    VertexStruct(float x, float y, float z, float u, float v, int index = 0):x(x),y(y),z(z),u(u),v(v),index(index) {}
} VertexStruct;

namespace Player {
    class Player {
    public:

        Player(int numberOfPatches);
        
        Player(int width, int height, int numberOfPatches = 128);
        ~Player();

        // ����Ƶ�ļ�, ��Ҫָ���ļ�·�����ļ�����
        bool openVideoFile(const std::string &filePath, VideoFileType fileType);

        // ���������YUV������������, ÿ֡����ǰ����Ҫ����
        bool setupTextureData(unsigned char *textureData);

        // �������Ⱦѭ��
        void renderLoop();

        // ����ͶӰ�ͻ��Ƹ�ʽ, ������renderLoop֮ǰ����
        void setupMode(ProjectionMode projection, DrawMode draw);

    private:
        bool init();
        void destroyGL();
        void destroyCodec();
        bool setupCodec();
        bool setupShaders();
        bool setupTexture();
        bool setupCoordinates();
        bool setupMatrixes();
        void setupProjectionMatrix();
        void drawFrame();
        bool handleInput();
        void resizeWindow(SDL_Event& event);
        void computeMVPMatrix();
        void computeViewMatrix();
        bool decodeOneFrame();

    private:
        bool setupCppEqualDistanceCoordinates();        
        void _drawFrameCppEqualDistance();

        void computeCppEqualDistanceUVCoordinates(float x, float y, float &u, float &v);
        void organizeVerts(std::vector<std::vector<VertexStruct>> &allVerts);

    private:

        void _drawFrameERPWithoutIndex();
        void _drawFrameERPWithIndex();
        void _drawFrameCPPWithIndex();
        void _drawFrameCPPWithoutIndex();
        
        void computeCppEqualAreaUVCoordinates(float latitude, float longitude, float &s, float &t);
        void computeCppEqualAreaUVCoordinates(double latitude, double longitude, double &u, double &v);
        

    private:
        bool _setupERPCoordinatesWithIndex();
        bool _setupERPCoordinatesWithoutIndex();
        bool _setupCPPCoordinatesWithoutIndex();
        bool _setupCPPCoordinatesWithIndex();

    private:
        SDL_Window *pWindow = NULL;
        SDL_GLContext pContext;
        TimeMeasurer *timeMeasurer = NULL;
        ProjectionMode projectionMode;
        DrawMode drawMode;
        VideoFileType videoFileType;

        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 projectMatrix;
        glm::mat4 mvpMatrix;
        GLuint sceneProgramID;
        GLuint sceneTextureID;
        GLuint yuvTextures[3];
        GLint sceneMVPMatrixPointer;

        GLuint sceneVAO;
        GLuint sceneVertBuffer;
        GLuint sceneUVBuffer;
        GLuint sceneIndexBuffer;

    private:
        int pPreviousXposition;
        int pPreviousYposition;
        int pCurrentXposition;
        int pCurrentYposition;
        int pWindowHeight;
        int pWindowWidth;
        int patchNumbers;
        int vertexCount;
        int indexArraySize;
        int frameHeight;
        int frameWidth;

        float touchPointX = 0;
        float touchPointY = 0;

        float *vertexArray = NULL;
        float *uvArray = NULL;
        int *indexArray = NULL;

        std::vector<float> vertexVector;
        std::vector<float> uvVector;
        std::vector<int> indexVector;

    private:
        AVFormatContext   *pFormatCtx = NULL;
        int               videoStream;
        int               index;
        AVCodecContext    *pCodecCtxOrig = NULL;
        AVCodecContext    *pCodecCtx = NULL;
        AVCodec           *pCodec = NULL;
        AVFrame           *pFrame = NULL;
        AVFrame           *pFrameRGB = NULL;
        AVPacket          packet;
        int               frameFinished;
        bool              allFrameRead = false;
        int               numBytes;
        uint8_t           *decodedBufferRGB24 = NULL;
        uint8_t           *compressedTexture = NULL;
        uint8_t           *decodedBufferRGBA = NULL;
        uint8_t           *rawBuffer = NULL;
        struct SwsContext *sws_ctx = NULL;
        std::ifstream     videoFileInputStream;
       
    };
}
