#include "NvDecoder.h"
#include "cuda.h"   
#include "cuda_runtime.h"
#include "device_functions.h" // 此头文件包含 __syncthreads ()函数
#include <cuda_gl_interop.h>    
#include <device_launch_parameters.h>
#include <driver_types.h>
#include <cufft.h>
#include "../../../DisplayCPP/NV12TORGBA.h"
#include "../../../DisplayCPP/glErrorChecker.h"

NvDecoder::NvDecoder() {
	frame_timer = NULL;
	global_timer = NULL;

	g_bDeviceLost = false;
	g_bDone = false;
	g_bRunning = false;
	g_bAutoQuit = false;
	g_bUseVsync = false;
	g_bFrameRepeat = false;
	g_bFrameStep = false;
	g_bQAReadback = false;
	g_bFirstFrame = true;
	g_bLoop = false;
	g_bUseDisplay = false; // this flag enables/disables video on the window
	g_bUseInterop = false;
	g_bReadback = true; // this flag enables/disables reading back of a video from a window
	g_bWriteFile = true; // this flag enables/disables writing of a file
	g_bIsProgressive = true; // assume it is progressive, unless otherwise noted
	g_bException = false;
	g_bWaived = false;
	g_iRepeatFactor = 1; // 1:1 assumes no frame repeats
	g_nFrameStart = -1;
	g_nFrameEnd = -1;

	fpWriteYUV = NULL;
	fpRefYUV = NULL;

	g_eVideoCreateFlags = cudaVideoCreate_Default;
	g_CtxLock = NULL;

	cuModNV12toARGB = 0;
	g_kernelNV12toARGB = 0;
	g_kernelPassThru = 0;

	g_oContext = 0;
	g_oDevice = 0;

	g_ReadbackSID = 0, g_KernelSID = 0;

	//g_pFrameYUV[6] = { 0, 0, 0, 0, 0, 0 };
	g_pFrameQueue = 0;
	g_pVideoSource = 0;
	g_pVideoParser = 0;
	g_pVideoDecoder = 0;

	for (int i = 0; i < 6; i++)
		g_pFrameYUV[i] = 0;

	g_nWindowWidth = 0;
	g_nWindowHeight = 0;

	g_nVideoWidth = 0;
	g_nVideoHeight = 0;

	g_FrameCount = 0;
	g_DecodeFrameCount = 0;
	g_fpsCount = 0;      // FPS count for averaging
	g_fpsLimit = 500;     // FPS limit for sampling timer;

	//strcpy(exec_path, argv[0]);
}

bool NvDecoder::init(AVFormatContext *pFormatCtx, int bUseInterop, int bTCC, int w, int h) {
	// Initialize the CUDA and NVDECODE
	typedef HMODULE CUDADRIVER;
	CUDADRIVER hHandleDriver = 0;
	CUresult cuResult;
	cuResult = cuInit(0, __CUDA_API_VERSION, hHandleDriver);
	cuResult = cuvidInit(0);

	// Find out the video size
	bool bLoad = loadVideoSource(pFormatCtx, w, h);
	if (!bLoad) {
		printf("数据源加载失败！\n");
		system("pause");
		return false;
	}

	// Initialize CUDA/D3D9 context and other video memory resources
	if (initCudaResources(g_bUseInterop, bTCC) == E_FAIL) {
		g_bAutoQuit = true;
		g_bException = true;
		g_bWaived = true;
		return false;
	}

	return true;
}


HRESULT NvDecoder::initCudaResources(int bUseInterop, int bTCC) {
	HRESULT hr = S_OK;

	CUdevice cuda_device;


	cuda_device = gpuGetMaxGflopsDeviceIdDRV();
	checkCudaErrors(cuDeviceGet(&g_oDevice, cuda_device));

	// get compute capabilities and the devicename
	int major, minor;
	size_t totalGlobalMem;
	char deviceName[256];
	checkCudaErrors(cuDeviceComputeCapability(&major, &minor, g_oDevice));
	checkCudaErrors(cuDeviceGetName(deviceName, 256, g_oDevice));
	printf("> Using GPU Device %d: %s has SM %d.%d compute capability\n", cuda_device, deviceName, major, minor);

	checkCudaErrors(cuDeviceTotalMem(&totalGlobalMem, g_oDevice));
	printf("  Total amount of global memory:     %4.4f MB\n", (float)totalGlobalMem / (1024 * 1024));


	checkCudaErrors(cuCtxCreate(&g_oContext, CU_CTX_BLOCKING_SYNC, g_oDevice));

	try {
		char FilePath[MAX_PATH + 1] = { 0 };
		char *p = NULL;
		GetModuleFileName(NULL, FilePath, sizeof(FilePath)); //获取程序当前执行文件名

		p = strrchr(FilePath, '\\');
		*p = '\0';
	} catch (char const *p_file) {
		// If the CUmoduleManager constructor fails to load the PTX file, it will throw an exception
		printf("\n>> CUmoduleManager::Exception!  %s not found!\n", p_file);
		printf(">> Please rebuild NV12ToARGB_drvapi.cu or re-install this sample.\n");
		return E_FAIL;
	}

	/////////////////Change///////////////////////////
	// Now we create the CUDA resources and the CUDA decoder context
	initCudaVideo();

	CUcontext cuCurrent = NULL;
	CUresult result = cuCtxPopCurrent(&cuCurrent);

	if (result != CUDA_SUCCESS) {
		printf("cuCtxPopCurrent: %d\n", result);
		assert(0);
	}

	return ((g_pVideoDecoder) ? S_OK : E_FAIL);
}

void NvDecoder::parseCommandLineArguments() {
	const char *sAppFilename = "NVDecodeD3D9";
	char *video_file, *yuv_file, *ref_yuv;

	//video_file = "rtsp://admin:admin123@192.168.101.54:554";
	video_file = "";

	yuv_file = "";
	g_bReadback = true;
	g_bWriteFile = true;

	// Search all command file parameters for video files with extensions:
	// mp4, avc, mkv, 264, h264. vc1, wmv, mp2, mpeg2, mpg
	ref_yuv = "";
	//g_bPSNR = false; 

	g_eVideoCreateFlags = cudaVideoCreate_PreferCUVID;

	g_bUseVsync = false;

	g_bFrameRepeat = false;

	if (g_bUseDisplay == false) {
		g_bQAReadback = true;
		g_bUseInterop = false;
	}

	if (g_bLoop == false) {
		g_bAutoQuit = true;
	}

	// Now verify the video file is legit
	FILE *fp = NULL;
	FOPEN(fp, video_file, "r");
	if (video_file == NULL && fp == NULL) {
		exit(EXIT_FAILURE);
	}

	if (fp) {
		fclose(fp);
	}

	// Now verify the input reference YUV file is legit
	FOPEN(fpRefYUV, ref_yuv, "r");
	if (ref_yuv == NULL && fpRefYUV == NULL) {
		printf("[%s]: unable to find file: [%s]\nExiting...\n", sAppFilename, ref_yuv);
		exit(EXIT_FAILURE);
	}

	// default video file loaded by this sample
	sFileName = video_file;

	if (g_bWriteFile && strlen(yuv_file) > 0) {
		printf("[%s]: output file: [%s]\n", sAppFilename, yuv_file);

		FOPEN(fpWriteYUV, yuv_file, "wb");
		if (fpWriteYUV == NULL) {
			printf("Error opening file [%s]\n", yuv_file);
		}
	}
	printf("[%s]: input file:  [%s]\n", sAppFilename, video_file);
}




bool NvDecoder::loadVideoSource(AVFormatContext *pFormatCtx, int w, int h) {
	std::auto_ptr<FrameQueue> apFrameQueue(new FrameQueue());
	std::auto_ptr<VideoSource> apVideoSource(new VideoSource());

	// retrieve the video source (width,height)
	if (!apVideoSource->init(pFormatCtx, apFrameQueue.get(), w, h)) {
		return false;
	};

	memset(&g_stFormat, 0, sizeof(CUVIDEOFORMAT));
	std::cout << (g_stFormat = apVideoSource->format()) << std::endl;

	if (g_bFrameRepeat) {
		if (apVideoSource->format().frame_rate.denominator > 0) {
			g_iRepeatFactor = (int)(60.0f / ceil((float)apVideoSource->format().frame_rate.numerator / (float)apVideoSource->format().frame_rate.denominator));
		}
	}

	printf("Frame Rate Playback Speed = %d fps\n", 60 / g_iRepeatFactor);

	g_pFrameQueue = apFrameQueue.release();
	g_pVideoSource = apVideoSource.release();

	if (g_pVideoSource->format().codec == cudaVideoCodec_JPEG) {
		g_eVideoCreateFlags = cudaVideoCreate_PreferCUDA;
	}

	return true;
}

void NvDecoder::initCudaVideo() {
	// bind the context lock to the CUDA context
	CUresult result = cuvidCtxLockCreate(&g_CtxLock, g_oContext);
	CUVIDEOFORMATEX oFormatEx;
	memset(&oFormatEx, 0, sizeof(CUVIDEOFORMATEX));
	oFormatEx.format = g_stFormat;

	if (result != CUDA_SUCCESS) {
		printf("cuvidCtxLockCreate failed: %d\n", result);
		assert(0);
	}

	std::auto_ptr<VideoDecoder> apVideoDecoder(new VideoDecoder(g_pVideoSource->format(), g_oContext, g_eVideoCreateFlags, g_CtxLock));
	std::auto_ptr<VideoParser> apVideoParser(new VideoParser(apVideoDecoder.get(), g_pFrameQueue, &oFormatEx));
	g_pVideoSource->setParser(*apVideoParser.get(), g_oContext);

	g_pVideoParser = apVideoParser.release();
	g_pVideoDecoder = apVideoDecoder.release();

	// Create a Stream ID for handling Readback
	if (g_bReadback) {
		checkCudaErrors(cuStreamCreate(&g_ReadbackSID, 0));
		checkCudaErrors(cuStreamCreate(&g_KernelSID, 0));
		printf("> initCudaVideo()\n");
		printf("  CUDA Streams (%s) <g_ReadbackSID = %p>\n", ((g_ReadbackSID == 0) ? "Disabled" : "Enabled"), g_ReadbackSID);
		printf("  CUDA Streams (%s) <g_KernelSID   = %p>\n", ((g_KernelSID == 0) ? "Disabled" : "Enabled"), g_KernelSID);
	}

	g_pVideoSource->start();
}



/*
* \return
* ::cudaSuccess,
* ::cudaErrorInvalidDevice,
* ::cudaErrorInvalidValue,
* ::cudaErrorInvalidResourceHandle,
* ::cudaErrorUnknown
* \notefnerr
*/
void CheckCudaReturn(cudaError_t err) {
	if (err == cudaSuccess) {
		// pass
	} else if (err == cudaErrorInvalidDevice) {
		std::cout << "cudaErrorInvalidDevice: " << err << " Line: " << __LINE__ << std::endl;
	} else if (err == cudaErrorInvalidValue) {
		std::cout << "cudaErrorInvalidValue: " << err << " Line: " << __LINE__ << std::endl;
	} else if (err == cudaErrorInvalidResourceHandle) {
		std::cout << "cudaErrorInvalidResourceHandle: " << err << " Line: " << __LINE__ << std::endl;
	} else if (err == cudaErrorUnknown) {
		std::cout << "cudaErrorUnknown: " << err << " Line: " << __LINE__ << std::endl;
	} else {
		std::cout << "err: " << err << std::endl;
	}
}

bool NvDecoder::copyDecodedFrameToTexture(unsigned char** ppImgData, int height, int width, GLuint &cudaTextureID, HDC &mainDC, HGLRC &mainGLRC, bool &needCudaMalloc) {

	glCheckError();
	CUVIDPARSERDISPINFO oDisplayInfo;
	cudaGraphicsResource_t cudaResources = NULL;
	if (g_pFrameQueue->dequeue(&oDisplayInfo)) {
		CCtxAutoLock lck(g_CtxLock);
		// Push the current CUDA context (only if we are using CUDA decoding path)
		CUresult result = cuCtxPushCurrent(g_oContext);

		CUdeviceptr  pDecodedFrame[3] = { 0, 0, 0 };

		int num_fields = 1;
		if (g_bUseVsync) {
			num_fields = std::min(2 + oDisplayInfo.repeat_first_field, 3);
		}

		CUVIDPROCPARAMS oVideoProcessingParameters;
		memset(&oVideoProcessingParameters, 0, sizeof(CUVIDPROCPARAMS));

		oVideoProcessingParameters.progressive_frame = oDisplayInfo.progressive_frame;
		oVideoProcessingParameters.top_field_first = oDisplayInfo.top_field_first;
		oVideoProcessingParameters.unpaired_field = (oDisplayInfo.repeat_first_field < 0);
		int imgSize = (height)*(width)* 4 * sizeof(unsigned char);
		if (needCudaMalloc) {

			cudaError_t err = cudaMalloc((void**)ppImgData, imgSize * sizeof(unsigned char));
			if (err != cudaSuccess) {
				std::cout << "--- cudaMalloc pImgData: " << err << std::ends << "Line: " << __LINE__ << std::endl;
			} else {
				std::cout << "--- cudaMalloc pImgData success " << std::endl;
			}
			needCudaMalloc = false;
		}
		unsigned char* pImgData = (unsigned char *)(*ppImgData);
		for (int active_field = 0; active_field < num_fields; active_field++) {

			unsigned int nDecodedPitch = 0;
			unsigned int nWidth = 0;
			unsigned int nHeight = 0;

			oVideoProcessingParameters.second_field = active_field;

			// map decoded video frame to CUDA surfae
			if (g_pVideoDecoder->mapFrame(oDisplayInfo.picture_index, &pDecodedFrame[active_field], &nDecodedPitch, &oVideoProcessingParameters) != CUDA_SUCCESS) {
				// release the frame, so it can be re-used in decoder
				g_pFrameQueue->releaseFrame(&oDisplayInfo);
				// Detach from the Current thread
				checkCudaErrors(cuCtxPopCurrent(NULL));
				return false;
			}

			nWidth = g_pVideoDecoder->targetWidth();
			nHeight = g_pVideoDecoder->targetHeight();

			size_t nTexturePitch = 0;


			// 设置GL环境
			
			BOOL make = wglMakeCurrent(mainDC, mainGLRC);
			if (make) {
				std::cout << "DecoderThread wglMakeCurrent ok" << std::endl;
			} else {
				std::cout << "DecoderThread wglMakeCurrent error" << std::endl;
			}
			// 在CUDA中注册Texture 
			cudaError_t err;
			err = cudaGraphicsGLRegisterImage(&cudaResources, cudaTextureID, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
			if (err != cudaSuccess) {
				std::cout << "cudaGraphicsGLRegisterImage: " << err << std::ends << "Line: " << __LINE__ << std::endl;
			}

			// 在CUDA中锁定资源,获得操作Texture的指针,这里是CudaArray*类型(Device)
			cudaArray_t cuArray;
			err = cudaGraphicsMapResources(1, &cudaResources, 0);
			if (err != cudaSuccess) {
				std::cout << "cudaGraphicsMapResources: " << err << std::ends << "Line: " << __LINE__ << std::endl;
			}
			err = cudaGraphicsSubResourceGetMappedArray(&cuArray, cudaResources, 0, 0);
			if (err != cudaSuccess) {
				std::cout << "cudaGraphicsSubResourceGetMappedArray: " << err << std::ends << "Line: " << __LINE__ << std::endl;
			}
			// Copy 
			// pDecodedFrame[active_field] 该值就是地址
			unsigned char* textureDataY = (unsigned char*)(pDecodedFrame[active_field]);
			unsigned char* textureDataUV = textureDataY + height*width;

			/*
			 NV12TORGBA(unsigned char *pYdata, unsigned char *pUVdata, int stepY, int stepUV,
			 unsigned char *pImgData, int width, int height, int channels)
			 */
			//NV12TORGBA(textureDataY, textureDataUV, width, width, pImgData, width, height, 4);
			err = cudaMemcpyToArray(cuArray, 0, 0, pImgData, imgSize, cudaMemcpyDeviceToDevice); //GL_RED
			if (err != cudaSuccess) {
				std::cout << "--- cudaMemcpyToArray error!: " << err << std::endl;
			}
			// 处理完即可解除资源锁定. OpenGL利用得的Texture进行纹理贴图操作
			err = cudaGraphicsUnmapResources(1, &cudaResources, 0);
			if (err != cudaSuccess) {
				std::cout << "cudaGraphicsUnmapResources: " << err << std::ends << "Line: " << __LINE__ << std::endl;
			}
			// 置零环境
			//make = wglMakeCurrent(NULL, NULL);
			g_pVideoDecoder->unmapFrame(pDecodedFrame[active_field]);
			g_DecodeFrameCount++;
			glCheckError();
		}
		// Detach from the Current thread
		checkCudaErrors(cuCtxPopCurrent(NULL));
		// release the frame, so it can be re-used in decoder
		g_pFrameQueue->releaseFrame(&oDisplayInfo);
	} else {
		// Frame Queue has no frames, we don't compute FPS until we start
		printf("==> Frame Queue has no frames\n");
		return false;
	}

	// check if decoding has come to an end.
	// if yes, signal the app to shut down.
	if (g_pFrameQueue->isEndOfDecode() && g_pFrameQueue->isEmpty()) {
		// Let's free the Frame Data
		if (g_ReadbackSID && g_pFrameYUV) {
			cuMemFreeHost((void *)g_pFrameYUV[0]);
			cuMemFreeHost((void *)g_pFrameYUV[1]);
			cuMemFreeHost((void *)g_pFrameYUV[2]);
			cuMemFreeHost((void *)g_pFrameYUV[3]);
			cuMemFreeHost((void *)g_pFrameYUV[4]);
			cuMemFreeHost((void *)g_pFrameYUV[5]);
			g_pFrameYUV[0] = NULL;
			g_pFrameYUV[1] = NULL;
			g_pFrameYUV[2] = NULL;
			g_pFrameYUV[3] = NULL;
			g_pFrameYUV[4] = NULL;
			g_pFrameYUV[5] = NULL;
		}

		// Let's just stop, and allow the user to quit, so they can at least see the results
		g_pVideoSource->stop();

		// If we want to loop reload the video file and restart
		if (g_bLoop && !g_bAutoQuit) {
			HRESULT hr = reinitCudaResources();
			if (SUCCEEDED(hr)) {
				g_FrameCount = 0;
				g_DecodeFrameCount = 0;
				//g_pVideoSource->start();
			}
		}

		if (g_bAutoQuit) {
			g_bDone = true;
		}
	}
	glCheckError();
	return true;
}



bool NvDecoder::copyDecodedFrameToTexture(unsigned char* buf) {
	CUVIDPARSERDISPINFO oDisplayInfo;

	//cudaGraphicsResource_t cudaResources = nullptr;

	if (g_pFrameQueue->dequeue(&oDisplayInfo)) {
		CCtxAutoLock lck(g_CtxLock);
		// Push the current CUDA context (only if we are using CUDA decoding path)
		CUresult result = cuCtxPushCurrent(g_oContext);

		CUdeviceptr  pDecodedFrame[3] = { 0, 0, 0 };

		int num_fields = 1;
		if (g_bUseVsync) {
			num_fields = std::min(2 + oDisplayInfo.repeat_first_field, 3);
		}

		CUVIDPROCPARAMS oVideoProcessingParameters;
		memset(&oVideoProcessingParameters, 0, sizeof(CUVIDPROCPARAMS));

		oVideoProcessingParameters.progressive_frame = oDisplayInfo.progressive_frame;
		oVideoProcessingParameters.top_field_first = oDisplayInfo.top_field_first;
		oVideoProcessingParameters.unpaired_field = (oDisplayInfo.repeat_first_field < 0);

		for (int active_field = 0; active_field < num_fields; active_field++) {

			unsigned int nDecodedPitch = 0;
			unsigned int nWidth = 0;
			unsigned int nHeight = 0;

			oVideoProcessingParameters.second_field = active_field;

			// map decoded video frame to CUDA surfae
			if (g_pVideoDecoder->mapFrame(oDisplayInfo.picture_index, &pDecodedFrame[active_field], &nDecodedPitch, &oVideoProcessingParameters) != CUDA_SUCCESS) {
				// release the frame, so it can be re-used in decoder
				g_pFrameQueue->releaseFrame(&oDisplayInfo);

				// Detach from the Current thread
				checkCudaErrors(cuCtxPopCurrent(NULL));

				return false;
			}


			nWidth = g_pVideoDecoder->targetWidth();
			nHeight = g_pVideoDecoder->targetHeight();
			// map DirectX texture to CUDA surface
			size_t nTexturePitch = 0;

			// If we are Encoding and this is the 1st Frame, we make sure we allocate system memory for readbacks
			if (g_bFirstFrame) {
				CUresult result;
				checkCudaErrors(result = cuMemAllocHost((void **)&g_pFrameYUV[0], (nDecodedPitch * nHeight + nDecodedPitch*nHeight / 2)));
				checkCudaErrors(result = cuMemAllocHost((void **)&g_pFrameYUV[1], (nDecodedPitch * nHeight + nDecodedPitch*nHeight / 2)));
				checkCudaErrors(result = cuMemAllocHost((void **)&g_pFrameYUV[2], (nDecodedPitch * nHeight + nDecodedPitch*nHeight / 2)));
				checkCudaErrors(result = cuMemAllocHost((void **)&g_pFrameYUV[3], (nDecodedPitch * nHeight + nDecodedPitch*nHeight / 2)));
				checkCudaErrors(result = cuMemAllocHost((void **)&g_pFrameYUV[4], (nDecodedPitch * nHeight + nDecodedPitch*nHeight / 2)));
				checkCudaErrors(result = cuMemAllocHost((void **)&g_pFrameYUV[5], (nDecodedPitch * nHeight + nDecodedPitch*nHeight / 2)));

				g_bFirstFrame = false;

				if (result != CUDA_SUCCESS) {
					printf("cuMemAllocHost returned %d\n", (int)result);
					checkCudaErrors(result);
				}
			}
			CUresult result = cuMemcpyDtoHAsync(g_pFrameYUV[active_field], pDecodedFrame[active_field], (nDecodedPitch * nHeight * 3 / 2), g_ReadbackSID);

			if (result != CUDA_SUCCESS) {
				printf("cuMemAllocHost returned %d\n", (int)result);
				checkCudaErrors(result);
			}

			{
				nTexturePitch = g_pVideoDecoder->targetWidth() * 2;
			}

			g_pVideoDecoder->unmapFrame(pDecodedFrame[active_field]);
			g_DecodeFrameCount++;

			if (g_bWriteFile) {
				checkCudaErrors(cuStreamSynchronize(g_ReadbackSID));
				saveFrameAsYUV(buf,//g_pFrameYUV[active_field + 3],
					g_pFrameYUV[active_field],
					nWidth, nHeight, nDecodedPitch);
			}
		}

		// Detach from the Current thread
		checkCudaErrors(cuCtxPopCurrent(NULL));
		// release the frame, so it can be re-used in decoder
		g_pFrameQueue->releaseFrame(&oDisplayInfo);
	} else {
		// Frame Queue has no frames, we don't compute FPS until we start
		return false;
	}

	// check if decoding has come to an end.
	// if yes, signal the app to shut down.
	if (g_pFrameQueue->isEndOfDecode() && g_pFrameQueue->isEmpty()) {
		// Let's free the Frame Data
		if (g_ReadbackSID && g_pFrameYUV) {
			cuMemFreeHost((void *)g_pFrameYUV[0]);
			cuMemFreeHost((void *)g_pFrameYUV[1]);
			cuMemFreeHost((void *)g_pFrameYUV[2]);
			cuMemFreeHost((void *)g_pFrameYUV[3]);
			cuMemFreeHost((void *)g_pFrameYUV[4]);
			cuMemFreeHost((void *)g_pFrameYUV[5]);
			g_pFrameYUV[0] = NULL;
			g_pFrameYUV[1] = NULL;
			g_pFrameYUV[2] = NULL;
			g_pFrameYUV[3] = NULL;
			g_pFrameYUV[4] = NULL;
			g_pFrameYUV[5] = NULL;
		}

		// Let's just stop, and allow the user to quit, so they can at least see the results
		g_pVideoSource->stop();

		// If we want to loop reload the video file and restart
		if (g_bLoop && !g_bAutoQuit) {
			HRESULT hr = reinitCudaResources();
			if (SUCCEEDED(hr)) {
				g_FrameCount = 0;
				g_DecodeFrameCount = 0;
				//g_pVideoSource->start();
			}
		}

		if (g_bAutoQuit) {
			g_bDone = true;
		}
	}
	return true;
}

HRESULT NvDecoder::reinitCudaResources() {
	// Free resources
	cleanup(false);

	// Reinit VideoSource and Frame Queue
	bool bLoad = loadVideoSource(NULL, 0, 0);
	if (!bLoad) {
		printf("数据源加载失败！\n");
		return S_FALSE;
	}

	initCudaVideo();

	return S_OK;
}


void NvDecoder::saveFrameAsYUV(unsigned char *pdst, const unsigned char *psrc, int width, int height, int pitch) {

	int x, y, width_2, height_2;
	int xy_offset = width*height;
	int uvoffs = (width / 2)*(height / 2);
	const unsigned char *py = psrc;
	const unsigned char *puv = psrc + height*pitch;

	// luma
	for (y = 0; y < height; y++) {
		memcpy(&pdst[y*width], py, width);
		py += pitch;
	}

	// De-interleave chroma
	width_2 = width >> 1;
	height_2 = height >> 1;
	for (y = 0; y < height_2; y++) {
		for (x = 0; x < width_2; x++) {
			pdst[xy_offset + y*(width_2)+x] = puv[x * 2];
			pdst[xy_offset + uvoffs + y*(width_2)+x] = puv[x * 2 + 1];
		}
		puv += pitch;
	}
}


void NvDecoder::freeCudaResources(bool bDestroyContext) {
	if (g_pVideoParser) {
		delete g_pVideoParser;
	}

	if (g_pVideoDecoder) {
		delete g_pVideoDecoder;
	}

	if (g_pVideoSource) {
		delete g_pVideoSource;
	}

	if (g_pFrameQueue) {
		delete g_pFrameQueue;
	}

	if (g_ReadbackSID) {
		cuStreamDestroy(g_ReadbackSID);
	}

	if (g_KernelSID) {
		cuStreamDestroy(g_KernelSID);
	}

	if (g_CtxLock) {
		checkCudaErrors(cuvidCtxLockDestroy(g_CtxLock));
	}

	if (g_oContext && bDestroyContext) {
		checkCudaErrors(cuCtxDestroy(g_oContext));
		g_oContext = NULL;
	}
}

HRESULT NvDecoder::cleanup(bool bDestroyContext) {
	if (fpWriteYUV != NULL) {
		fflush(fpWriteYUV);
		fclose(fpWriteYUV);
		fpWriteYUV = NULL;
	}

	if (bDestroyContext) {
		// Attach the CUDA Context (so we may properly free memroy)
		checkCudaErrors(cuCtxPushCurrent(g_oContext));
		// Detach from the Current thread
		checkCudaErrors(cuCtxPopCurrent(NULL));
	}

	if (g_ReadbackSID && g_pFrameYUV) {
		cuMemFreeHost((void *)g_pFrameYUV[0]);
		cuMemFreeHost((void *)g_pFrameYUV[1]);
		cuMemFreeHost((void *)g_pFrameYUV[2]);
		cuMemFreeHost((void *)g_pFrameYUV[3]);
		cuMemFreeHost((void *)g_pFrameYUV[4]);
		cuMemFreeHost((void *)g_pFrameYUV[5]);
		g_pFrameYUV[0] = NULL;
		g_pFrameYUV[1] = NULL;
		g_pFrameYUV[2] = NULL;
		g_pFrameYUV[3] = NULL;
		g_pFrameYUV[4] = NULL;
		g_pFrameYUV[5] = NULL;
	}


	freeCudaResources(bDestroyContext);

	return S_OK;
}