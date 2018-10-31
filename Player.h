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
	PM_CPP_OBSOLETE,
	PM_CPP, // CPP格式
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
	DT_HARDWARE = 0,
	DT_SOFTWARE,
	DT_NOT_SPECIFIED
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

		Player(int numberOfPatches);

		Player(int width, int height, int numberOfPatches = 128);
		~Player();

		// 打开视频文件, 需要指定文件路径和文件类型
		bool openVideoFile(const std::string &filePath);

		// 将解码出的YUV用作纹理数据, 每帧绘制前均需要设置
		bool setupTextureData(unsigned char *textureData);

		// 解码和渲染循环
		void renderLoop();

		// 设置投影和绘制格式, 必须在renderLoop之前进行
		void setupMode(ProjectionMode projection, DrawMode draw, DecodeType decode, VideoFileType fileType);

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
		pthread_t decodeThread;

		sem_t decodeOneFrameFinishedSemaphore;
		sem_t renderFinishedSemaphore;
		sem_t decodeAllFramesFinishedSemaphore;
		pthread_mutex_t lock;

		void destoryThread();
		friend void * decodeFunc(void *args);

	public:
		void initThread();
		void renderLoopThread();

	private:
		bool _setupERPCoordinatesWithIndex();
		bool _setupERPCoordinatesWithoutIndex();
		bool _setupCPPCoordinatesWithoutIndex();
		bool _setupCPPCoordinatesWithIndex();

	private:
		SDL_Window *pWindow = NULL;
		SDL_GLContext pContext;
		TimeMeasurer *timeMeasurer = NULL;
		ProjectionMode projectionMode = PM_NOT_SPECIFIED;
		DrawMode drawMode = DM_NOT_SPECIFIED;
		VideoFileType videoFileType = VFT_NOT_SPECIFIED;
		DecodeType decodeType = DT_NOT_SPECIFIED;

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
		CUVIDSOURCEDATAPACKET inputPacket;
		NvDecoder *pNVDecoder = NULL;
		uint8_t *cudaRGBABuffer = NULL;

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
