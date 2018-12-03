#include "Player.h"
#include <cmath>
#include <iostream>
#include <string>
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"
#include "glErrorChecker.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static const char* TEXTURE_UNIFORMS[] = { "y_tex", "u_tex", "v_tex" };

void addShader(int type, const char * source, int program) {
	int shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled != GL_TRUE) {
		std::cout << "Failed to add shader: " << (type == GL_VERTEX_SHADER?"VERTEX_SHADER":"FRAGMENT_SHADER") << std::endl;
	}
	glAttachShader(program, shader);
	glDeleteShader(shader);
}

namespace Player {
	Player::Player(int numberOfPatches) :
		pWindow(NULL),
		glContext(NULL),
		projectionMode(PM_NOT_SPECIFIED),
		drawMode(DM_NOT_SPECIFIED),
		timeMeasurer(new TimeMeasurer()),
		vertexArray(NULL),
		uvArray(NULL),
		indexArray(NULL) {
		this->patchNumber = numberOfPatches;
		init();
	}

	Player::Player(int width, int height, int numberOfPatches) :
		pWindow(NULL),
		glContext(NULL),
		projectionMode(PM_NOT_SPECIFIED),
		drawMode(DM_NOT_SPECIFIED),
		patchNumber(numberOfPatches),
		timeMeasurer(new TimeMeasurer()),
		vertexArray(NULL),
		uvArray(NULL),
		indexArray(NULL),
		videoFrameWidth(width),
		videoFrameHeight(height) {
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
		if (decodedYUVBuffer != NULL) {
			av_free(decodedYUVBuffer) ;
			decodedYUVBuffer = NULL;
		}

		if (videoFileInputStream.is_open()) {
			videoFileInputStream.close();
		}

        if (compressedTextureBuffer != NULL) {
            delete[] compressedTextureBuffer;
            compressedTextureBuffer = NULL;
        }

        if (decodedRGBABuffer != NULL) {
            delete[] decodedRGBABuffer;
            decodedRGBABuffer = NULL;
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
		destoryThread();
		SDL_Quit();
	}

    void Player::saveViewport(const char *fileName) {
        if (this->projectionMode != PM_CUBEMAP) {
            glBindTexture(GL_TEXTURE_2D, sceneTextureID);

            unsigned char *tmp = new unsigned char[windowWidth*windowHeight * 3];
            unsigned char *data = new unsigned char[windowHeight*windowWidth * 3];
            glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, tmp);

            for (int i = 0; i < windowHeight; i++) {
                memcpy(&data[i*windowWidth * 3], &tmp[(windowHeight - i-1)*windowWidth * 3], windowWidth * 3 * sizeof(unsigned char));
            }

            stbi_write_png(fileName, windowWidth, windowHeight, 3, data, windowWidth * 3);

            delete[] tmp;
            delete[] data;
            glBindTexture(GL_TEXTURE_2D, 0);
        }
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
		windowWidth = 1280;
		windowHeight = 640;

		Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

		pWindow = SDL_CreateWindow("Panoramic Video Player", windowPosX, windowPosY, windowWidth, windowHeight, windowFlags);

		if (pWindow == NULL) {
			std::cout << __FUNCTION__ << "- Window could not be created! SDL Error: " << SDL_GetError() << std::endl;
			return false;
		}

		glContext = SDL_GL_CreateContext(pWindow);
		if (glContext == NULL) {
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
		glCheckError();
		assert(this->videoFileType != VFT_NOT_SPECIFIED && this->decodeType != DT_NOT_SPECIFIED && this->drawMode != DM_NOT_SPECIFIED);

		if (videoFileType == VFT_Encoded && decodeType == DT_SOFTWARE) {
			if (filePath.length() == 0) {
				return false;
			}

			if (avformat_open_input(&pFormatContext, filePath.c_str(), NULL, NULL) != 0) {
				return false;
			}

			if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
				return false;
			}

			av_dump_format(pFormatContext, 0, filePath.c_str(), 0);

			videoStreamIndex = -1;

			for (int i = 0; i < pFormatContext->nb_streams; i++) {
				if (pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
					videoStreamIndex = i;
					break;
				}
			}
			if (videoStreamIndex == -1) {
				return false;
			}

            if (this->renderYUV) {
                pCodecContext = pFormatContext->streams[videoStreamIndex]->codec;

                pCodec = avcodec_find_decoder(pFormatContext->streams[videoStreamIndex]->codec->codec_id);
                if (pCodec == NULL) {
                    return false; // Codec not found
                }

                if (avcodec_open2(pCodecContext, pCodec, NULL) < 0) {
                    return false;
                }

                pFrame = av_frame_alloc();
                if (pFrame == NULL) {
                    return false;
                }

                av_init_packet(&packet);

                numberOfBytesPerFrame = avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecContext->width, pCodecContext->height);
                decodedYUVBuffer = (uint8_t *)av_malloc(numberOfBytesPerFrame * sizeof(uint8_t));
                if (decodedYUVBuffer == NULL) {
                    std::cout << "Failed to malloc for decodedYUVBuffer" << std::endl;
                    return false;
                }

                avpicture_fill((AVPicture *)pFrame, decodedYUVBuffer, AV_PIX_FMT_YUV420P, pCodecContext->width, pCodecContext->height);

            } else {
                pCodecContextOriginal = pFormatContext->streams[videoStreamIndex]->codec;

                pCodec = avcodec_find_decoder(pCodecContextOriginal->codec_id);

                if (pCodec == NULL) {
                    return false; // Codec not found
                }

                pCodecContext = avcodec_alloc_context3(pCodec);
                if (avcodec_copy_context(pCodecContext, pCodecContextOriginal) != 0) {
                    return false; // Error copying codec context
                }

                // Open codec
                if (avcodec_open2(pCodecContext, pCodec, NULL) < 0) {
                    return false; // Could not open codec
                }


                // Allocate video frame
                pFrame = av_frame_alloc();
                if (pFrame == NULL) {
                    return false;
                }

                pFrameRGB = av_frame_alloc();



                av_init_packet(&packet);

                //numberOfBytesPerFrame = avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecContext->width, pCodecContext->height);

                numberOfBytesPerFrame = avpicture_get_size(AV_PIX_FMT_RGB24,
                    pCodecContext->width, pCodecContext->height);
                decodedRGB24Buffer = (uint8_t *)av_malloc(numberOfBytesPerFrame * sizeof(uint8_t));
                if (decodedRGB24Buffer == NULL) {
                    std::cout << "Failed to malloc for decodedRGB24Buffer" << std::endl;
                    return false;
                }


                avpicture_fill((AVPicture *)pFrameRGB, decodedRGB24Buffer, AV_PIX_FMT_RGB24, pCodecContext->width, pCodecContext->height);

                swsContext = sws_getContext(pCodecContext->width, pCodecContext->height, pCodecContext->pix_fmt, pCodecContext->width, pCodecContext->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
            }
			return true;
		} else if (videoFileType == VFT_Encoded && decodeType == DT_HARDWARE) {
			this->pNVDecoder = new NvDecoder();

			if (filePath.length() == 0) {
				return false;
			}
			pFormatContext = avformat_alloc_context();
			if (avformat_open_input(&pFormatContext, filePath.c_str(), NULL, NULL) < 0) {
				return false;
			}

			if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
				return false;
			}

			av_dump_format(pFormatContext, 0, "", 0);

			videoStreamIndex = -1;

			for (int i = 0; i < pFormatContext->nb_streams; i++) {
				if (pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
					videoStreamIndex = i;
					break;
				}
			}
			if (videoStreamIndex == -1) {
				return false;
			}

			pNVDecoder->parseCommandLineArguments();

			if (pNVDecoder->init(pFormatContext, false, 0, videoFrameWidth, videoFrameHeight) == false) {
				std::cout << "Failed to init nvDecoder" << std::endl;
				return false;
			}

			return true;
		} else if (videoFileType == VFT_YUV) {
			this->videoFileInputStream.open(filePath, std::ios::binary | std::ios::in);

			assert(this->videoFrameWidth != 0 && this->videoFrameHeight != 0);
			//this->decodedYUVBuffer = new uint8_t[this->videoFrameWidth*this->videoFrameHeight * 3 / 2];
            decodedYUVBuffer = (uint8_t *)av_malloc(this->videoFrameWidth*this->videoFrameHeight*3/2 * sizeof(uint8_t));
			if (this->decodedYUVBuffer == NULL) {
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
		int distanceX = currentXposition - previousXposition;
		int distanceY = currentYposition - previousYposition;

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
		float aspect = windowWidth * 1.0f / windowHeight;
		projectMatrix = glm::perspective(45.0f, aspect, 0.1f, 40.0f);
	}

	/**
	* 根据视频的投影格式和绘制格式来设置球体模型的顶点坐标与纹理坐标，默认是无索引，ERP
	*/
	bool Player::setupCoordinates() {
		bool result;
		if (this->projectionMode == PM_CPP_OBSOLETE) {
			if (this->drawMode == DM_USE_INDEX) {
				result = setupCPPCoordinatesWithIndex_Obsolete();
			} else {
				result = setupCPPCoordinatesWithoutIndex_Obsolete();
			}
		} else if (this->projectionMode == PM_ERP) {
			if (this->drawMode == DM_USE_INDEX) {
				result = setupERPCoordinatesWithIndex();
			} else {
				result = setupERPCoordinatesWithoutIndex();
			}
		} else if (this->projectionMode == PM_CPP) {
			setupCppEqualDistanceCoordinates();
			result = true;
        } else if (this->projectionMode == PM_CUBEMAP || this->projectionMode == PM_EAC) {
            setupCubeMapCoordinates();
            result = true;
        }
		return result;
	}

    bool Player::setupCubeMapCoordinates() {
        this->vertexCount = 36;
        float skyboxVertices[] = {
            // positions          
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
        };

        glGenVertexArrays(1, &sceneVAO);
        glBindVertexArray(sceneVAO);

        glGenBuffers(1, &sceneVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glBindVertexArray(0);
        return true;
    }


	bool Player::setupERPCoordinatesWithIndex() {
		glCheckError();

		int radius = 10;
		int pieces = this->patchNumber;
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

		glGenBuffers(1, &sceneVertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
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
	bool Player::setupERPCoordinatesWithoutIndex() {
		glCheckError();
		this->vertexCount = this->patchNumber * this->patchNumber * 3;

		if (this->vertexArray) {
			delete[] this->vertexArray;
		}
		if (this->uvArray) {
			delete[] this->uvArray;
		}
		this->vertexArray = new float[this->vertexCount * 3];
		this->uvArray = new float[this->vertexCount * 2];


		int radius = 10;
		int pieces = this->patchNumber;
		int halfPieces = this->patchNumber / 2;
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

		glGenBuffers(1, &sceneVertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
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
	bool Player::setupCPPCoordinatesWithoutIndex_Obsolete() {
		glCheckError();
		this->vertexCount = (this->patchNumber) * (this->patchNumber / 2) * 6;
		if (this->vertexArray) {
			delete[] this->vertexArray;
		}
		this->vertexArray = new float[this->vertexCount * 3];
		if (this->uvArray) {
			delete[] this->uvArray;
		}
		this->uvArray = new float[this->vertexCount * 2];

		int radius = 10;
		int pieces = this->patchNumber;
		int halfPieces = this->patchNumber / 2;
		float verticalInterval = M_PI / (halfPieces);
		float horizontalInterval = verticalInterval;
		float latitude;
		float longitude;
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
				computeCppUVCoordinates_Obsolete(latitude, longitude, u[0], v[0]);

				z[1] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude));
				x[1] = (float)(radius*sin(latitude + verticalInterval)*sin(longitude));
				y[1] = (float)(radius*cos(latitude + verticalInterval));
				computeCppUVCoordinates_Obsolete(latitude + verticalInterval, longitude, u[1], v[1]);

				z[2] = (float)(radius*sin(latitude + verticalInterval)*cos(longitude + horizontalInterval));
				x[2] = (float)(radius *sin(latitude + verticalInterval)*sin(longitude + horizontalInterval));
				y[2] = (float)(radius*cos(latitude + verticalInterval));
				computeCppUVCoordinates_Obsolete(latitude + verticalInterval, longitude + horizontalInterval, u[2], v[2]);


				z[3] = (float)(radius*sin(latitude)*cos(longitude + horizontalInterval));
				x[3] = (float)(radius*sin(latitude)*sin(longitude + horizontalInterval));
				y[3] = (float)(radius*cos(latitude));
				computeCppUVCoordinates_Obsolete(latitude, longitude + horizontalInterval, u[3], v[3]);


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

		glGenBuffers(1, &sceneVertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
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
	bool Player::setupCPPCoordinatesWithIndex_Obsolete() {
		glCheckError();

		int radius = 10;
		int pieces = this->patchNumber;
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


		float verticalInterval = M_PI / halfPieces;
		float horizontalInterval = verticalInterval;

		float latitude, longitude;
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

				computeCppUVCoordinates_Obsolete(latitude, longitude, ut, vt);
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

		glGenBuffers(1, &sceneVertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
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
	void Player::computeCppUVCoordinates_Obsolete(float latitude, float longitude, float &s, float &t) {
		float x, y;
		float H = videoFrameHeight / 2;
		float R = videoFrameHeight / sqrt(3 * M_PI);

		latitude -= M_PI / 2;
		longitude -= M_PI;

		x = sqrt(3 / M_PI) * R * longitude * (2 * cos(2 * latitude / 3) - 1);
		y = sqrt(3 * M_PI) * R * sin(latitude / 3);

		x += videoFrameWidth / 2;
		y += videoFrameHeight / 2;

		s = x / videoFrameWidth;
		t = y / videoFrameHeight;
	}

	/**
	* 根据投影格式来调用不同的渲染方法
	*/
	void Player::drawFrame() {
        sem_wait(&(this->decodeOneFrameFinishedSemaphore));
        if (this->videoFileType == VFT_YUV) {

            pthread_mutex_lock(&this->lock);
            this->setupTextureData(decodedYUVBuffer);
            pthread_mutex_unlock(&this->lock);

        } else if (this->videoFileType == VFT_Encoded) {
            if (this->decodeType == DT_HARDWARE) {
                glBindTexture(GL_TEXTURE_2D, cudaTextureID);

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
            } else if (this->decodeType == DT_SOFTWARE) {
                pthread_mutex_lock(&this->lock);

                if (this->renderYUV) {
                    this->setupTextureData(decodedYUVBuffer);
                } else {
                    this->setupTextureData(decodedRGB24Buffer);
                }

                pthread_mutex_unlock(&this->lock);
            }
        }


		if (this->projectionMode == PM_ERP) {

			if (drawMode == DM_USE_INDEX) {
				drawFrameERPWithIndex();
			} else {
				drawFrameERPWithoutIndex();
			}

		} else if (this->projectionMode == PM_CPP_OBSOLETE) {

			if (drawMode == DM_USE_INDEX) {
				drawFrameCPPWithIndex_Obsolete();
			} else {
				drawFrameCPPWithoutIndex_Obsolete();
			}

		} else if (this->projectionMode == PM_CPP) {

			drawFrameCppEqualDistance();

        } else if (this->projectionMode == PM_CUBEMAP || this->projectionMode == PM_EAC) {

            drawFrameCubeMap();

        }

        /*if (this->frameIndex == 0) {
            saveViewport(this->viewportImageFileName.c_str());
        }*/

        SDL_GL_SwapWindow(pWindow);

        sem_post(&this->renderFinishedSemaphore);
	}

    void Player::drawFrameCubeMap() {
        glCheckError();
        glViewport(0, 0, windowWidth, windowHeight);
        glDisable(GL_DEPTH_TEST);

        computeMVPMatrix();
        glUseProgram(sceneProgramID);

        glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);
        glBindVertexArray(sceneVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

        glDrawArrays(GL_TRIANGLES, 0, this->vertexCount);

        glBindVertexArray(0);
        glCheckError();
    }

	void Player::drawFrameERPWithIndex() {
		glViewport(0, 0, windowWidth, windowHeight);
		glDisable(GL_DEPTH_TEST);
		glCheckError();
		computeMVPMatrix();
		glUseProgram(sceneProgramID);

		glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);
		glBindVertexArray(sceneVAO);

		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sceneIndexBuffer);
		glDrawElements(GL_TRIANGLES, this->indexArraySize, GL_UNSIGNED_INT, (const void *)0);
		glBindVertexArray(0);

	}



	/**
	* 绘制投影格式为ERP的视频帧
	*/
	void Player::drawFrameERPWithoutIndex() {
		/*glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
		glViewport(0, 0, windowWidth, windowHeight);
		glDisable(GL_DEPTH_TEST);

		computeMVPMatrix();
		glUseProgram(sceneProgramID);

		glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);

		glBindVertexArray(sceneVAO);

		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glDrawArrays(GL_TRIANGLES, 0, this->vertexCount);
		glBindVertexArray(0);

	}

	/**
	* 绘制投影格式为CPP的视频帧
	*/
	void Player::drawFrameCPPWithIndex_Obsolete() {
		/*glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
		glViewport(0, 0, windowWidth, windowHeight);
		glDisable(GL_DEPTH_TEST);

		computeMVPMatrix();
		glUseProgram(sceneProgramID);


		glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);

		glBindVertexArray(sceneVAO);

		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sceneIndexBuffer);

		glDrawElements(GL_TRIANGLES, this->indexArraySize, GL_UNSIGNED_INT, (const void *)0);

		glBindVertexArray(0);
		
		glCheckError();
	}

	void Player::drawFrameCPPWithoutIndex_Obsolete() {
		/*glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/
		glViewport(0, 0, windowWidth, windowHeight);
		glDisable(GL_DEPTH_TEST);

		computeMVPMatrix();
		glUseProgram(sceneProgramID);

		glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);

		glBindVertexArray(sceneVAO);

		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glDrawArrays(GL_TRIANGLES, 0, this->vertexCount);

		glBindVertexArray(0);

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

        glCheckError();
		static bool firstTime = true;
		assert(videoFrameHeight > 0 && videoFrameWidth > 0);
		glUseProgram(sceneProgramID);

        if (this->projectionMode == PM_CUBEMAP) {
            glCheckError();
            
            glBindTexture(GL_TEXTURE_CUBE_MAP, sceneTextureID);
            glCheckError();
            // Common
            glPixelStorei(GL_UNPACK_ROW_LENGTH, videoFrameWidth);

            // Facebook Transform转换出来的是3*2的，从左到右从上到下，依次是：右、左、上、下、前、后
            // GL_UNPACK_SKIP_PIXELS指定跳过该行的多少个像素
            // GL_UNPACK_SKIP_ROWS指定跳过多少行(从上往下数)

            // 左
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth/3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

            // 上
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth/3*2);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

            // 下
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, videoFrameHeight / 2);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

            // 前
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth / 3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, videoFrameHeight / 2);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

            // 右
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

            // 后
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth / 3 * 2);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, videoFrameHeight / 2);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

        } else if (this->projectionMode == PM_EAC) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, sceneTextureID);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, videoFrameWidth);
            glCheckError();
            // EAC格式也是3*2的，从左到右从上到下依次是：左、前、右、下、后、上
            
            // 左
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glCheckError();
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glCheckError();
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, videoFrameWidth / 3, videoFrameHeight / 2, 0, GL_RGB, GL_UNSIGNED_BYTE,textureData);
            glCheckError();
            // 前
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth / 3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, videoFrameWidth / 3, videoFrameHeight / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

            // 右
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth / 3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, videoFrameWidth / 3, videoFrameHeight / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
            
            // 下
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth / 3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, videoFrameWidth / 3, videoFrameHeight / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
            
            // 后
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth / 3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, videoFrameWidth / 3, videoFrameHeight / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
            
            // 上
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, videoFrameWidth / 3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, videoFrameWidth / 3, videoFrameHeight / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
            
        } else {
            if (this->renderYUV) {
                unsigned char *yuvPlanes[3];
                yuvPlanes[0] = textureData;
                yuvPlanes[1] = textureData + this->videoFrameWidth * this->videoFrameHeight;
                yuvPlanes[2] = textureData + this->videoFrameWidth * this->videoFrameHeight / 4 * 5;

                for (int i = 0; i < 3; i++) {
                    int w = (i == 0 ? this->videoFrameWidth : this->videoFrameWidth / 2);
                    int h = (i == 0 ? this->videoFrameHeight : this->videoFrameHeight / 2);
                    glActiveTexture(GL_TEXTURE0 + i);
                    glBindTexture(GL_TEXTURE_2D, yuvTexturesID[i]);
                    if (firstTime) {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(yuvPlanes[i]));
                        firstTime = true;
                    } else {
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(yuvPlanes[i]));
                    }
                }
            } else {
                glBindTexture(GL_TEXTURE_2D, sceneTextureID);

                /*glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, videoFrameWidth, videoFrameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);*/

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, videoFrameWidth, videoFrameHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

                /*unsigned int size = ((videoFrameWidth + 3) / 4)*((videoFrameHeight + 3) / 4) * 8;
                glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, videoFrameWidth, videoFrameHeight, 0, size, compressedTextureBuffer);*/
            }
        }

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

        if (this->videoFileType == VFT_YUV) {
            this->setRenderYUV(true);
        }
        setupShaders();
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
					SDL_GetMouseState(&currentXposition, &currentYposition);
					computeViewMatrix();
					computeMVPMatrix();
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				isMouseSelected = true;
				SDL_GetMouseState(&previousXposition, &previousYposition);
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
		windowWidth = event.window.data1;
		windowHeight = event.window.data2;
		glViewport(0, 0, windowWidth, windowHeight);
		setupProjectionMatrix();
		computeMVPMatrix();
	}


	bool Player::setupCodec() {
		av_register_all();
		return true;
	}

	void Player::destroyCodec() {
        if (decodedRGB24Buffer != NULL) {
            av_free(decodedRGB24Buffer);
            decodedRGB24Buffer = NULL;
        }
        

		if (&pFrame != NULL) {
			av_frame_free(&pFrame);
		}

		if (pCodecContext != NULL) {
			avcodec_close(pCodecContext);
			pCodecContext = NULL;
		}

		if (pCodecContextOriginal != NULL) {
			avcodec_close(pCodecContextOriginal);
			pCodecContextOriginal = NULL;
		}

		if (&pFormatContext != NULL) {
			avformat_close_input(&pFormatContext);
		}

		if (pNVDecoder != NULL) {
			pNVDecoder->cleanup(true);
			pNVDecoder = NULL;
		}

        if (decodedYUVBuffer != NULL) {
            av_free(decodedYUVBuffer);
            decodedYUVBuffer = NULL;
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

        char *VERTEX_SHADER = NULL, *FRAGMENT_SHADER = NULL;

        if (this->projectionMode == PM_CUBEMAP || this->projectionMode == PM_EAC) {
            VERTEX_SHADER =
                "#version 410 core\n"
                "uniform mat4 matrix;\n"
                "out vec3 TexCoords;\n"
                "layout(location = 0) in vec4 position;\n"
                "void main() {\n"
                "   TexCoords = position.xyz;\n"
                "   gl_Position = matrix * position;\n"
                "}\n";
            // texture(mytexture, TexCoords)
            FRAGMENT_SHADER =
                "#version 410 core\n"
                "varying vec3 TexCoords;\n"
                "uniform samplerCube mytexture;\n"
                "void main() {\n"
                "   gl_FragColor = texture(mytexture, TexCoords);\n"
                "}\n";
        } else {
            VERTEX_SHADER =
                "#version 410 core\n"
                "uniform mat4 matrix;\n"
                "layout(location = 0) in vec4 position;\n"
                "layout(location = 1) in vec2 uvCoords;\n"
                "out vec2 uvCoordsOut;\n"
                "void main() {\n"
                "	uvCoordsOut = uvCoords;\n"
                "	gl_Position = matrix * position;\n"
                "}\n";

            if (this->renderYUV == true) {
                FRAGMENT_SHADER =
                    "#version 410 core\n"
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
            } else {
                FRAGMENT_SHADER =
                    "#version 410 core\n"
                    "uniform sampler2D mytexture;\n"
                    "in vec2 uvCoordsOut;\n"
                    "void main() {\n"
                    "	gl_FragColor = texture(mytexture,uvCoordsOut);\n"
                    "}\n";
            }
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
        if (this->projectionMode == PM_CUBEMAP || this->projectionMode == PM_EAC) {
            glGenTextures(1, &sceneTextureID);

            glBindTexture(GL_TEXTURE_CUBE_MAP, sceneTextureID);
            glUniform1i(glGetUniformLocation(sceneProgramID, "mytexture"), 0);

            glTexParameterf(GL_TEXTURE_CUBE_MAP,
                GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_CUBE_MAP,
                GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_CUBE_MAP,
                GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_CUBE_MAP,
                GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_CUBE_MAP, 
                GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        } else {
            if (this->decodeType == DT_SOFTWARE) {

                if (this->renderYUV) {
                    glGenTextures(3, yuvTexturesID);
                    for (int i = 0; i < 3; i++) {
                        glUniform1i(glGetUniformLocation(sceneProgramID, TEXTURE_UNIFORMS[i]), i);
                        glActiveTexture(GL_TEXTURE0 + i);
                        glBindTexture(GL_TEXTURE_2D, yuvTexturesID[i]);
                        glTexParameterf(GL_TEXTURE_2D,
                            GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameterf(GL_TEXTURE_2D,
                            GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameterf(GL_TEXTURE_2D,
                            GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameterf(GL_TEXTURE_2D,
                            GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        
                    }
                } else {
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
                }
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

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, videoFrameWidth, videoFrameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

                //glBindTexture(GL_TEXTURE_2D, 0);
            }
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
			int readSuccess = av_read_frame(pFormatContext, &packet);
			if (readSuccess < 0) {
				allFrameRead = true;
				return false;
			}

			if (packet.stream_index == videoStreamIndex) {
				avcodec_decode_video2(pCodecContext, pFrame, &frameFinished, &packet);
				if (frameFinished) {
                    sws_scale(swsContext, (uint8_t const *const *)pFrame->data,
                        pFrame->linesize, 0, pCodecContext->height,
                        pFrameRGB->data, pFrameRGB->linesize);
                    avpicture_layout((AVPicture *)pFrameRGB, AV_PIX_FMT_RGB24, pCodecContext->width, pCodecContext->height, decodedRGB24Buffer, numberOfBytesPerFrame);

                    
                    fast_unpack((char *)decodedRGBABuffer, (const char *)decodedRGB24Buffer, pCodecContext->width * pCodecContext->height);

                    rygCompress(compressedTextureBuffer, decodedRGBABuffer, pCodecContext->width, pCodecContext->height,0);
                    

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
				int readSuccess = av_read_frame(pFormatContext, &packet);
				if (readSuccess < 0) {
					allFrameRead = true;
					return false;
				}
				if (packet.stream_index == videoStreamIndex) {
					inputPacket.payload = packet.data;
					inputPacket.payload_size = packet.size;
					inputPacket.flags = CUVID_PKT_TIMESTAMP;
					cuvidParseVideoData(pNVDecoder->g_pVideoSource->oSourceData_.hVideoParser, &inputPacket);
					bool needsCudaMalloc = true;
					if (pNVDecoder->copyDecodedFrameToTexture(&cudaRGBABuffer, videoFrameHeight, videoFrameWidth, this->cudaTextureID, this->mainDeviceContext, this->mainGLRenderContext, needsCudaMalloc)) {
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

			static std::streampos pos = this->videoFrameHeight * this->videoFrameWidth * 3 / 2;
			this->videoFileInputStream.read((char *)decodedYUVBuffer, pos);
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
		timeMeasurer->Start();
		while (!bQuit && !allFrameRead) {
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


// CPP 渲染相关
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

		int H = this->videoFrameHeight / 2;

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

		glGenBuffers(1, &sceneVertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, vertexDataSize, &this->vertexVector[0], GL_STATIC_DRAW);

		glGenBuffers(1, &sceneUVBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
		glBufferData(GL_ARRAY_BUFFER, uvDataSize, &this->uvVector[0], GL_STATIC_DRAW);

		glBindVertexArray(0);
		glCheckError();
		return true;
	}

	void Player::drawFrameCppEqualDistance() {
		glViewport(0, 0, windowWidth, windowHeight);
		glDisable(GL_DEPTH_TEST);

		computeMVPMatrix();
		glUseProgram(sceneProgramID);
		glCheckError();

		glUniformMatrix4fv(sceneMVPMatrixPointer, 1, GL_FALSE, &mvpMatrix[0][0]);
		glBindVertexArray(sceneVAO);

		glBindBuffer(GL_ARRAY_BUFFER, sceneVertexBuffer);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glBindBuffer(GL_ARRAY_BUFFER, sceneUVBuffer);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);

		glDrawArrays(GL_TRIANGLES, 0, this->vertexVector.size() / 3);
		glBindVertexArray(0);

	}

	void Player::computeCppEqualDistanceUVCoordinates(float x, float y, float &u, float &v) {
		x += this->videoFrameWidth / 2;
		y += this->videoFrameHeight / 2;

		u = x / this->videoFrameWidth;
		v = y / this->videoFrameHeight;

		v = 1.0f - v;
	}
}

namespace Player {
	void Player::setupThread()
{
		int ret = pthread_create(&decodeThread, NULL, decodeFunc,&(*this));
		if (ret != 0) {
			std::cout << "Pthread_create error" << std::endl;
		}

		sem_init(&decodeOneFrameFinishedSemaphore, 0, 0);
		sem_init(&decodeAllFramesFinishedSemaphore, 0, 0);
		sem_init(&renderFinishedSemaphore, 0, 1);

		lock = PTHREAD_MUTEX_INITIALIZER;
	}

	void Player::destoryThread() {
		sem_destroy(&decodeOneFrameFinishedSemaphore);
		sem_destroy(&decodeAllFramesFinishedSemaphore);
		sem_destroy(&renderFinishedSemaphore);

		pthread_mutex_destroy(&lock);
	}


	void * Player::decodeFunc(void *args) {
		std::cout << "decodeFunc" << std::endl;
		Player *player = (Player *)args;
		if (player->videoFileType == VFT_Encoded && player->decodeType == DT_SOFTWARE) {
			while (av_read_frame(player->pFormatContext, &player->packet) >= 0) {
				if (player->packet.stream_index == player->videoStreamIndex) {
					avcodec_decode_video2(player->pCodecContext, player->pFrame, &player->frameFinished, &player->packet);
					if (player->frameFinished) {
                        if (player->renderYUV) {
                            sem_wait(&player->renderFinishedSemaphore);

                            pthread_mutex_lock(&player->lock);
                            avpicture_layout((AVPicture *)player->pFrame, AV_PIX_FMT_YUV420P, player->pCodecContext->width, player->pCodecContext->height, player->decodedYUVBuffer, player->numberOfBytesPerFrame);
                            pthread_mutex_unlock(&player->lock);

                        } else {
                            sws_scale(player->swsContext, (uint8_t const* const *)player->pFrame->data, player->pFrame->linesize, 0, player->pCodecContext->height, player->pFrameRGB->data, player->pFrameRGB->linesize);

                            sem_wait(&player->renderFinishedSemaphore);

                            pthread_mutex_lock(&player->lock);
                            //avpicture_layout((AVPicture *)player->pFrame, AV_PIX_FMT_YUV420P, player->pCodecContext->width, player->pCodecContext->height, player->decodedYUVBuffer, player->numberOfBytesPerFrame);
                            avpicture_layout((AVPicture *)player->pFrameRGB, AV_PIX_FMT_RGB24, player->pCodecContext->width, player->pCodecContext->height, player->decodedRGB24Buffer, player->numberOfBytesPerFrame);

                            pthread_mutex_unlock(&player->lock);
                        }
                        
						av_free_packet(&player->packet);

						sem_post(&player->decodeOneFrameFinishedSemaphore);
					}
				}
			} 
			player->allFrameRead = true;
			sem_post(&player->decodeAllFramesFinishedSemaphore);
			std::cout << "decodeAllFramesFinished" << std::endl;
			pthread_exit(NULL);
		} else if (player->videoFileType == VFT_Encoded && player->decodeType == DT_HARDWARE) {
			while (true) {
				int readSuccess = av_read_frame(player->pFormatContext, &player->packet);
				if (readSuccess < 0) {
					player->allFrameRead = true;
					sem_post(&player->decodeAllFramesFinishedSemaphore);
					pthread_exit(NULL);
				}
				if (player->packet.stream_index == player->videoStreamIndex) {
					player->inputPacket.payload = player->packet.data;
					player->inputPacket.payload_size = player->packet.size;
					player->inputPacket.flags = CUVID_PKT_TIMESTAMP;
					cuvidParseVideoData(player->pNVDecoder->g_pVideoSource->oSourceData_.hVideoParser, &player->inputPacket);
					bool needsCudaMalloc = true;
					sem_wait(&player->renderFinishedSemaphore);
					pthread_mutex_lock(&player->lock);
					if (player->pNVDecoder->copyDecodedFrameToTexture(&player->cudaRGBABuffer, player->videoFrameHeight, player->videoFrameWidth, player->cudaTextureID, player->mainDeviceContext, player->mainGLRenderContext, needsCudaMalloc)) {
						pthread_mutex_unlock(&player->lock);
						av_free_packet(&player->packet);
						sem_post(&player->decodeOneFrameFinishedSemaphore);
					} else {
						pthread_mutex_unlock(&player->lock);
						continue;
					}
				}
			}
		} else if (player->videoFileType == VFT_YUV) {
			while (true) {
                if (player->repeatRendering) {
                    static std::streampos pos = player->videoFrameHeight * player->videoFrameWidth * 3 / 2;
                    if (player->videoFileInputStream.peek() == EOF) {
                        player->videoFileInputStream.seekg(0, std::ios_base::beg);
                    }
                    sem_wait(&player->renderFinishedSemaphore);
                    pthread_mutex_lock(&player->lock);
                    player->videoFileInputStream.read((char *)player->decodedYUVBuffer, pos);
                    player->videoFileInputStream.seekg(pos, std::ios_base::cur);
                    pthread_mutex_unlock(&player->lock);
                    sem_post(&player->decodeOneFrameFinishedSemaphore);
                } else {
                    if (player->videoFileInputStream.peek() == EOF) {
                        player->allFrameRead = true;
                        sem_post(&player->decodeAllFramesFinishedSemaphore);
                        pthread_exit(NULL);
                    }
                    static std::streampos pos = player->videoFrameHeight * player->videoFrameWidth * 3 / 2;
                    sem_wait(&player->renderFinishedSemaphore);
                    pthread_mutex_lock(&player->lock);
                    player->videoFileInputStream.read((char *)player->decodedYUVBuffer, pos);
                    player->videoFileInputStream.seekg(pos, std::ios_base::cur);
                    pthread_mutex_unlock(&player->lock);
                    sem_post(&player->decodeOneFrameFinishedSemaphore);
                }
			}
		}
		return NULL;
	}


	void Player::renderLoopThread() {
		bool bQuit = false;
		timeMeasurer->Start();
        
        if (this->repeatRendering) {
            while (true) {
                while (!bQuit && !this->allFrameRead) {
                    bQuit = this->handleInput();
                    this->drawFrame();
                    frameIndex++;
                }
                if (bQuit) {
                    break;
                }
            }
        } else {
            while (!bQuit && !this->allFrameRead) {
                bQuit = this->handleInput();
                this->drawFrame();
                frameIndex++;
            }
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
	}
}