#include "Player.h"
#include <cmath>
#include <iostream>
#include <string>
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"
#include "glErrorChecker.h"
//#define glCheckError() glCheckError_(__LINE__)

#define RENDER_YUV 0

const char* TEXTURE_UNIFORMS[] = { "y_tex", "u_tex", "v_tex" };

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

#if RENDER_YUV
static const char FRAGMENT_SHADER[] =
"precision mediump float;\n"
"varying vec2 uvCoordsOut;\n"
"uniform sampler2D y_tex;\n"
"uniform sampler2D u_tex;\n"
"uniform sampler2D v_tex;\n"
"void main() {\n"
"       float y = 1.164 * (texture2D(y_tex, uvCoordsOut).r - 0.0625);\n"
"       float u = texture2D(u_tex, uvCoordsOut).r - 0.5;\n"
"       float v = texture2D(v_tex, uvCoordsOut).r - 0.5;\n"
"       gl_FragColor = vec4(y + 1.596 * v, "
"                      y - 0.391 * u - 0.813 * v, "
"                      y + 2.018 * u, "
"                      1.0);\n"
"}\n";
#else 
static const char FRAGMENT_SHADER[] =
"#version 410 core\n"
"uniform sampler2D mytexture;\n"
"in vec2 uvCoordsOut;\n"
"void main() {\n"
"	gl_FragColor = texture(mytexture,uvCoordsOut);\n"
"}\n";
#endif

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


namespace Player {
	Player::Player(int numberOfPatches) :
		pWindow(NULL),
		pContext(NULL),
		projectionMode(PM_NOT_SPECIFIED),
		drawMode(DM_NOT_SPECIFIED),
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
		drawMode(DM_NOT_SPECIFIED),
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
		if (rawBuffer != NULL) {
			delete[] rawBuffer;
			rawBuffer = NULL;
		}

		if (videoFileInputStream.is_open()) {
			videoFileInputStream.close();
		}
        


        if (compressedTexture != NULL) {
            delete[] compressedTexture;
            compressedTexture = NULL;
        }

        if (decodedBufferRGBA != NULL) {
            delete[] decodedBufferRGBA;
            decodedBufferRGBA = NULL;
        }

		if (cudaRGBABuffer != NULL) {
			cudaFree(cudaRGBABuffer);
			cudaRGBABuffer = NULL;
		}

		if (pNVDecoder != NULL) {
			delete pNVDecoder;
			pNVDecoder = NULL;
		}

		destroyGL();
		destroyCodec();
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
		//if (!setupTexture()) {
		//	std::cout << __FUNCTION__ << "- SetupTexture failed." << std::endl;
		//	return false;
		//}

		if (!setupCodec()) {
			std::cout << __FUNCTION__ << "- setupCodec failed." << std::endl;
			return false;
		}


		mainGLRenderContext = wglGetCurrentContext();
		mainDeviceContext = wglGetCurrentDC();
		if (!mainGLRenderContext || !mainDeviceContext) {
			return false;
		}
		return true;
	}
	bool Player::openVideoFile(const std::string &filePath) {
		assert(this->videoFileType != VFT_NOT_SPECIFIED && this->decodeType != DT_NOT_SPECIFIED && this->drawMode != DM_NOT_SPECIFIED);

		if (videoFileType == VFT_Encoded && decodeType == DT_SOFTWARE) {
			if (filePath.length() == 0) {
				return false;
			}

			if (avformat_open_input(&pFormatCtx, filePath.c_str(), NULL, NULL) != 0) {
				return false;
			}

			if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
				return false;
			}

			av_dump_format(pFormatCtx, 0, filePath.c_str(), 0);

			videoStream = -1;

			for (int i = 0; i < pFormatCtx->nb_streams; i++) {
				if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
					videoStream = i;
					break;
				}
			}
			if (videoStream == -1) {
				return false;
			}

			pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

			pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);

			if (pCodec == NULL) {
				fprintf(stderr, "Unsupported codec!\n");
				return false; // Codec not found
			}

			pCodecCtx = avcodec_alloc_context3(pCodec);
			if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
				fprintf(stderr, "Couldn't copy codec context");
				return false; // Error copying codec context
			}

			// Open codec
			if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
				return false; // Could not open codec
			}


			// Allocate video frame
			pFrame = av_frame_alloc();
			if (pFrame == NULL) {
				return false;
			}

            pFrameRGB = av_frame_alloc();
            


			av_init_packet(&packet);

            numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
            decodedBufferRGB24 = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

            decodedBufferRGBA = new uint8_t[numBytes * sizeof(uint8_t) / 3 * 4];
            if (decodedBufferRGBA == NULL) {
                std::cout << "Failed to malloc for decodedBufferRGBA!" << std::endl;
                return false;
            }

            compressedTexture = new uint8_t[numBytes * sizeof(uint8_t) / 3 * 4 / 8];
            if (compressedTexture == NULL) {
                std::cout << "Failed to malloc for compressedTexture!" << std::endl;
                return false;
            }

            avpicture_fill((AVPicture *)pFrameRGB, decodedBufferRGB24, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

            sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24,SWS_BILINEAR,NULL,NULL,NULL);

            if (decodedBufferRGB24 == NULL) {
				return false;
			}

			return true;
		} else if (videoFileType == VFT_Encoded && decodeType == DT_HARDWARE) {
			this->pNVDecoder = new NvDecoder();

			/*fp = fopen(filePath.c_str(), "rb+");

			int bufsize = 4096;

			uint8_t *buf = (uint8_t *)av_mallocz(sizeof(uint8_t)*bufsize);

			AVIOContext *ioContext = NULL;
			ioContext = avio_alloc_context(buf, bufsize, 0, this, read_data, NULL, NULL);
			if (!ioContext) {
			return false;
			}
			AVInputFormat *inputFormat = NULL;
			if (av_probe_input_buffer(ioContext, &inputFormat, "", NULL, 0, 0) < 0) {
			return false;
			}

			pFormatCtx = avformat_alloc_context();
			pFormatCtx->pb = ioContext;*/

			if (filePath.length() == 0) {
				return false;
			}
			pFormatCtx = avformat_alloc_context();
			if (avformat_open_input(&pFormatCtx, filePath.c_str(), NULL, NULL) < 0) {
				return false;
			}

			if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
				return false;
			}

			av_dump_format(pFormatCtx, 0, "", 0);

			videoStream = -1;

			for (int i = 0; i < pFormatCtx->nb_streams; i++) {
				if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
					videoStream = i;
					break;
				}
			}
			if (videoStream == -1) {
				return false;
			}

			pNVDecoder->parseCommandLineArguments();

			if (pNVDecoder->init(pFormatCtx, false, 0, frameWidth, frameHeight) == false) {
				std::cout << "Failed to init nvDecoder" << std::endl;
				return false;
			}

			return true;
		} else if (videoFileType == VFT_YUV) {
			this->videoFileInputStream.open(filePath, std::ios::binary | std::ios::in);

			assert(this->frameWidth != 0 && this->frameHeight != 0);
			this->rawBuffer = new uint8_t[this->frameWidth*this->frameHeight * 3 / 2];
			if (this->rawBuffer == NULL) {
				return false;
			} else {
				return true;
			}
		} else {
			return false;
		}


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
		if (this->projectionMode == PM_CPP_OBSOLETE) {
			if (this->drawMode == DM_USE_INDEX) {
				result = _setupCPPCoordinatesWithIndex();
			} else {
				result = _setupCPPCoordinatesWithoutIndex();
			}
		} else if (this->projectionMode == PM_ERP) {
			if (this->drawMode == DM_USE_INDEX) {
				result = _setupERPCoordinatesWithIndex();
			} else {
				result = _setupERPCoordinatesWithoutIndex();
			}
		} else if (this->projectionMode == PM_CPP) {
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
		if (this->decodeType == DT_SOFTWARE) {
#if RENDER_YUV
#else 
			glBindTexture(GL_TEXTURE_2D, sceneTextureID);
			glCheckError();
#endif
		} else if (this->decodeType == DT_HARDWARE) {
			glBindTexture(GL_TEXTURE_2D, cudaTextureID);
		} else if (this->decodeType == DT_NOT_SPECIFIED) {
			std::cout << "DecodeType not specified!" << std::endl;
		}
		if (this->videoFileType == VFT_YUV) {
			this->setupTextureData(rawBuffer);
		} else if (this->videoFileType == VFT_Encoded && this->decodeType == DT_SOFTWARE) {
            this->setupTextureData(decodedBufferRGBA);
		} else if (this->videoFileType == VFT_Encoded && this->decodeType == DT_HARDWARE) {
			static bool firstTime = true;
			if (firstTime) {
				bool make = wglMakeCurrent(mainDeviceContext, mainGLRenderContext);
				if (make) {
					std::cout << "wglMakeCurrent ok" << std::endl;
				} else {
					std::cout << "wglMakeCurrent error" << std::endl;
				}
				firstTime = false;
			}
		} else {
			return;
		}
		glCheckError();
		if (this->projectionMode == PM_ERP) {
			if (drawMode == DM_USE_INDEX) {
				_drawFrameERPWithIndex();
			} else {
				_drawFrameERPWithoutIndex();
			}

		} else if (this->projectionMode == PM_CPP_OBSOLETE) {
			if (drawMode == DM_USE_INDEX) {
				_drawFrameCPPWithIndex();
			} else {
				_drawFrameCPPWithoutIndex();
			}
		} else if (this->projectionMode == PM_CPP) {
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
		static bool firstTime = true;
		assert(frameHeight > 0 && frameWidth > 0);
		glUseProgram(sceneProgramID);
		glCheckError();

#if RENDER_YUV
		unsigned char *yuvPlanes[3];
		yuvPlanes[0] = textureData;
		yuvPlanes[1] = textureData + this->frameWidth * this->frameHeight;
		yuvPlanes[2] = textureData + this->frameWidth * this->frameHeight / 4 * 5;

		for (int i = 0; i < 3; i++) {
			int w = (i == 0 ? this->frameWidth : this->frameWidth / 2);
			int h = (i == 0 ? this->frameHeight : this->frameHeight / 2);

			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, yuvTextures[i]);
			if (firstTime) {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(yuvPlanes[i]));
				firstTime = true;
				glCheckError();
			} else {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(yuvPlanes[i]));
				glCheckError();
			}
		}
#else 
		glCheckError();
		glBindTexture(GL_TEXTURE_2D, sceneTextureID);
		glCheckError();

       //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
        unsigned int size = ((frameWidth + 3) / 4)*((frameHeight + 3) / 4)*8;
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, frameWidth, frameHeight, 0, size, compressedTexture);
		glCheckError();
#endif

		glCheckError();
		return true;
	}

	/**
	* 设置投影和绘图格式
	*/
	void Player::setupMode(ProjectionMode projection, DrawMode draw, DecodeType decode, VideoFileType fileType) {
		this->projectionMode = projection;
		this->drawMode = draw;
		this->decodeType = decode;
		this->videoFileType = fileType;
		setupCoordinates();
		setupTexture();
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


	bool Player::setupCodec() {
		av_register_all();
		return true;
	}

	void Player::destroyCodec() {
        av_free(decodedBufferRGB24);

		if (&pFrame != NULL) {
			av_frame_free(&pFrame);
		}

		if (pCodecCtx != NULL) {
			avcodec_close(pCodecCtx);
			pCodecCtx = NULL;
		}

		if (pCodecCtxOrig != NULL) {
			avcodec_close(pCodecCtxOrig);
			pCodecCtxOrig = NULL;
		}

		if (&pFormatCtx != NULL) {
			avformat_close_input(&pFormatCtx);
		}

		if (pNVDecoder != NULL) {
			pNVDecoder->cleanup(true);
			pNVDecoder = NULL;
		}
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
		if (this->decodeType == DT_SOFTWARE) {

#if RENDER_YUV
			glGenTextures(3, yuvTextures);
			for (int i = 0; i < 3; i++) {
				glUniform1i(glGetUniformLocation(sceneProgramID, TEXTURE_UNIFORMS[i]), i);
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, yuvTextures[i]);
				glTexParameterf(GL_TEXTURE_2D,
					GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D,
					GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D,
					GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameterf(GL_TEXTURE_2D,
					GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			} 
#else 
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
#endif
		} else if (this->decodeType == DT_HARDWARE) {
			cudaDeviceProp prop;
			int dev;
			memset(&prop, 0, sizeof(cudaDeviceProp));

			prop.major = 1;
			prop.minor = 0;

			cudaError_t err = cudaChooseDevice(&dev, &prop);
			assert(err == CUDA_SUCCESS);

			cudaGLSetGLDevice(dev);

			glGenTextures(1, &cudaTextureID);
			glUniform1i(glGetUniformLocation(sceneProgramID, "mytexture"), 0);
			glBindTexture(GL_TEXTURE_2D, cudaTextureID);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

			//glBindTexture(GL_TEXTURE_2D, 0);
		}
		glUseProgram(0);
		glCheckError();
		return true;
	}

    void fast_unpack(char* rgba, const char* rgb, const int count) {
        if (count == 0)
            return;
        for (int i = count; --i; rgba += 4, rgb += 3) {
            *(uint32_t*)(void*)rgba = *(const uint32_t*)(const void*)rgb;
        }
        for (int j = 0; j < 3; ++j) {
            rgba[j] = rgb[j];
        }
    }

	bool Player::decodeOneFrame() {
		if (this->videoFileType == VFT_Encoded && this->decodeType == DT_SOFTWARE) {
			int readSuccess = av_read_frame(pFormatCtx, &packet);
			if (readSuccess < 0) {
				allFrameRead = true;
				return false;
			}

			if (packet.stream_index == videoStream) {
				avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
				if (frameFinished) {
                    sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
                        pFrame->linesize, 0, pCodecCtx->height,
                        pFrameRGB->data, pFrameRGB->linesize);
                    avpicture_layout((AVPicture *)pFrameRGB, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, decodedBufferRGB24, numBytes);

                    
                    fast_unpack((char *)decodedBufferRGBA, (const char *)decodedBufferRGB24, pCodecCtx->width * pCodecCtx->height);

                    rygCompress(compressedTexture, decodedBufferRGBA, pCodecCtx->width, pCodecCtx->height,0);
                    

					av_free_packet(&packet);
					return true;
				} else {
					return false;
				}
			} else {
				std::cout << "packet.stream_index != videoStream, will return false" << std::endl;
				return false;
			}
		} else if (this->videoFileType == VFT_Encoded && this->decodeType == DT_HARDWARE) {
			while (true) {
				int readSuccess = av_read_frame(pFormatCtx, &packet);
				if (readSuccess < 0) {
					allFrameRead = true;
					return false;
				}
				if (packet.stream_index == videoStream) {
					inputPacket.payload = packet.data;
					inputPacket.payload_size = packet.size;
					inputPacket.flags = CUVID_PKT_TIMESTAMP;
					cuvidParseVideoData(pNVDecoder->g_pVideoSource->oSourceData_.hVideoParser, &inputPacket);
					bool needsCudaMalloc = true;
					if (pNVDecoder->copyDecodedFrameToTexture(&cudaRGBABuffer, frameHeight, frameWidth, this->cudaTextureID, this->mainDeviceContext, this->mainGLRenderContext, needsCudaMalloc)) {
						av_free_packet(&packet);
						return true;
					}
				}
			}
		} else if (this->videoFileType == VFT_YUV) {
			if (this->videoFileInputStream.peek() == EOF) {
				allFrameRead = true;
				return false;
			}

			static std::streampos pos = this->frameHeight * this->frameWidth * 3 / 2;
			this->videoFileInputStream.read((char *)rawBuffer, pos);
			this->videoFileInputStream.seekg(pos, std::ios_base::cur);
			return true;
		} else {
			return false;
		}
	}

	/**
	* 主渲染循环
	*/
	void Player::renderLoop() {
		assert(projectionMode != PM_NOT_SPECIFIED);
		assert(drawMode != DM_NOT_SPECIFIED);
		bool bQuit = false;
		SDL_StartTextInput();
		int frameIndex = 0;
		timeMeasurer->Start();
		while (!bQuit && !allFrameRead) {
			bQuit = handleInput();
			if (decodeOneFrame()) {
				this->drawFrame();
				frameIndex++;
			}
            bQuit = handleInput();
		}
		__int64 time = timeMeasurer->elapsedMillionSecondsSinceStart();
		double average = 1.0 * time / frameIndex;

		std::string projectionMode;
		switch (this->projectionMode) {
		case PM_CPP_OBSOLETE:
			projectionMode = "Craster Parabolic Projection";
			break;
		case PM_CPP:
			projectionMode = "Equal Distance Projection";
			break;
		case PM_ERP:
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

			while (pointerForLevel < vertCountForLevel - 1 && pointerForNextLevel < vertCountForNextLevel - 1) {
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


