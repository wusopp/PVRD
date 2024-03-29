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
#include "dynlink_nvcuvid.h"
#include "../../NVDecoder/NvDecoder.h"
#include <cuda.h>
#include <cuda_runtime.h>
#include <device_functions.h>
#include <cuda_gl_interop.h>
#include <device_launch_parameters.h>
#include <pthread.h>
#include <semaphore.h>
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
	PM_ERP = 0, // ERP格式
    PM_CUBEMAP = 1, // Cubemap格式
	PM_CPP = 2, // CPP格式
    PM_CPP_OBSOLETE = 3, // CPP，用ERP的方式来进行渲染
    PM_EAC = 4, // 谷歌提出的EAC格式
    PM_ACP = 5,
    PM_TSP = 6,
	PM_NOT_SPECIFIED, // 未指定格式
};

enum DrawMode {
	DM_USE_INDEX = 0, // 使用索引进行绘制
	DM_DONT_USE_INDEX, // 不使用索引进行绘制
	DM_NOT_SPECIFIED // 绘制方法未指定
};

enum VideoFileType {
	VFT_YUV = 0, // YUV Raw格式
	VFT_Encoded, // 经过封装的视频格式如mp4
	VFT_NOT_SPECIFIED
};

enum DecodeType {
    DT_SOFTWARE = 0,
	DT_HARDWARE,
	DT_NOT_SPECIFIED
};

enum FACE_INDEX {
    FACE_INDEX_FRONT = 0,
    FACE_INDEX_BACK = 1,
    FACE_INDEX_TOP = 2,
    FACE_INDEX_BOTTOM = 3,
    FACE_INDEX_LEFT = 4,
    FACE_INDEX_RIGHT = 5
};

typedef struct VertexStruct {
	float x;
	float y;
	float z;
	float u;
	float v;
	int index;
	VertexStruct(float x, float y, float z, float u, float v, int index = 0) :x(x), y(y), z(z), u(u), v(v), index(index) {}
} VertexStruct;

namespace Player {
	class Player {
	public:
        Player(int argc, char **argv);

		~Player();

		// 打开视频文件, 需要指定文件路径和文件类型
		bool openVideo();

		// 将解码出的YUV用作纹理数据, 每帧绘制前均需要设置
		bool setupTextureData(unsigned char *textureData);

		// 解码和渲染循环
		void renderLoop();

		// 设置投影和绘制格式, 必须在renderLoop之前进行
		void setupMode(ProjectionMode projection, DrawMode draw, DecodeType decode, VideoFileType fileType);

        inline void setRenderYUV(bool renderYUV) {
            this->renderYUV = renderYUV;
        }

        inline void setRepeatRendering(bool repeatRendering) {
            this->repeatRendering = repeatRendering;
        }

        void saveViewport();

        char * viewportImageFileName = NULL;
        std::string videoFileName;

        void parseArguments(int argc, char ** argv);

	public:
		HGLRC mainGLRenderContext;
		HDC mainDeviceContext;
		GLuint cudaTextureID;

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
        void setupTSPCoordinates();

        void calculateTSPTextureCoordinates(int faceWidth, int face_idx, int i, int j, float &x, float &y, float &z, float &u, float &v);
        void addVertexData(float x, float y, float z, float u, float v, std::vector<float> &vertexVector, std::vector<float> &uvVector) {
            vertexVector.push_back(x);
            vertexVector.push_back(y);
            vertexVector.push_back(z);
            uvVector.push_back(u);
            uvVector.push_back(v);
        }
        void drawFrameTSP();


    private:
        bool setupEACCoordinates();
        void drawFrameEAC();

    private:
        bool setupCubeMapCoordinates();
        void drawFrameCubeMap();

	private:
		bool setupCppEqualDistanceCoordinates();
		void drawFrameCppEqualDistance();

		void computeCppEqualDistanceUVCoordinates(float x, float y, float &u, float &v);
		void organizeVerts(std::vector<std::vector<VertexStruct>> &allVerts);

	private:

		void drawFrameERPWithoutIndex();
		void drawFrameERPWithIndex();
		void drawFrameCPPWithIndex_Obsolete();
		void drawFrameCPPWithoutIndex_Obsolete();

		void computeCppUVCoordinates_Obsolete(float latitude, float longitude, float &s, float &t);
		
	private:
		pthread_t decodeThread;

		sem_t decodeOneFrameFinishedSemaphore;
		sem_t renderFinishedSemaphore;
		sem_t decodeAllFramesFinishedSemaphore;
		pthread_mutex_t lock;

		void destoryThread();
		static void * decodeFunc(void *args);

	public:
		void setupThread();
		void renderLoopThread();

	private:
		bool setupERPCoordinatesWithIndex();
		bool setupERPCoordinatesWithoutIndex();
		bool setupCPPCoordinatesWithoutIndex_Obsolete();
		bool setupCPPCoordinatesWithIndex_Obsolete();

	private:
		SDL_Window *pWindow = NULL;
		SDL_GLContext glContext;
		TimeMeasurer *timeMeasurer = NULL;
		ProjectionMode projectionMode = PM_ERP;
		DrawMode drawMode = DM_USE_INDEX;
		VideoFileType videoFileType = VFT_Encoded;
		DecodeType decodeType = DT_SOFTWARE;

		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
		glm::mat4 projectMatrix;
		glm::mat4 mvpMatrix;
		GLuint sceneProgramID;
		GLuint sceneTextureID;
		GLuint yuvTexturesID[3];
		GLint sceneMVPMatrixPointer;

		GLuint sceneVAO;
		GLuint sceneVertexBuffer;
		GLuint sceneUVBuffer;
		GLuint sceneIndexBuffer;

	private:
		CUVIDSOURCEDATAPACKET inputPacket;
		NvDecoder *pNVDecoder = NULL;
		uint8_t *cudaRGBABuffer = NULL;

	private:
		int previousXposition;
		int previousYposition;
		int currentXposition;
		int currentYposition;
		int windowHeight;
		int windowWidth;
		int patchNumber;
		int vertexCount;
		int indexArraySize;
		int videoFrameHeight;
		int videoFrameWidth;

		float touchPointX = 0;
		float touchPointY = 0;

		float *vertexArray = NULL;
		float *uvArray = NULL;
		int *indexArray = NULL;

		std::vector<float> vertexVector;
		std::vector<float> uvVector;
		std::vector<int> indexVector;

	private:
		AVFormatContext   *pFormatContext = NULL;
		int               videoStreamIndex;
		AVCodecContext    *pCodecContextOriginal = NULL;
		AVCodecContext    *pCodecContext = NULL;
		AVCodec           *pCodec = NULL;
		AVFrame           *pFrame = NULL;
        AVFrame           *pFrameRGB = NULL;
		AVPacket          packet;
		int               frameFinished;
		bool              allFrameRead = false;
		int               numberOfBytesPerFrame;
        uint8_t           *decodedRGB24Buffer = NULL;
        uint8_t           *compressedTextureBuffer = NULL;
        uint8_t           *decodedRGBABuffer = NULL;
		uint8_t           *decodedYUVBuffer = NULL;
		struct SwsContext *swsContext = NULL;
		std::ifstream     videoFileInputStream;
        bool              renderYUV = true;
        bool              repeatRendering = false;
        int frameIndex = 0;
        uint8_t * faceBufferOne = NULL;
        uint8_t * faceBufferTwo = NULL;
	};
}
