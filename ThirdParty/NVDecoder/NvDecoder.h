#ifndef NV_DECODER_H_
#define NV_DECODER_H_


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#pragma comment(lib, "version.lib")
#endif

#include "d3d9.h"
#include "d3dx9.h"

#pragma comment(lib, "d3d9.lib")
//#pragma comment(lib, "d3dx9.lib")

// CUDA Header includes
#include "dynlink_nvcuvid.h"  // <nvcuvid.h>
#include "dynlink_cuda.h"     // <cuda.h>
#include "dynlink_cudaD3D9.h" // <cudaD3D9.h>


#if 0 // 和cuda环境冲突了,所以去掉了
#include "dynlink_builtin_types.h"      // <builtin_types.h>
#else
#include "cuda.h"   
#include "cuda_runtime.h"
#include "device_functions.h" // 此头文件包含 __syncthreads ()函数
#include <cuda_gl_interop.h>    
#include <device_launch_parameters.h>
#endif


// CUDA utilities and system includes
#include "helper_functions.h"
#include "helper_cuda_drvapi.h"

// cudaDecodeD3D9 related helper functions
#include "FrameQueue.h"
#include "VideoSource.h"
#include "VideoParser.h"
#include "VideoDecoder.h"

#include "cudaModuleMgr.h"
extern "C" {
#include "../../ffmpeg/include/libavformat/avformat.h"
}



// Include files
#include <math.h>
#include <memory>
#include <iostream>
#include <cassert>

#ifdef _DEBUG
#define ENABLE_DEBUG_OUT    0
#else
#define ENABLE_DEBUG_OUT    0
#endif


class NvDecoder {

public:
    NvDecoder();
    bool            init(AVFormatContext *pFormatCtx, int bUseInterop, int bTCC, int w, int h);
    // Forward declarations
    bool            loadVideoSource(AVFormatContext *pFormatCtx, int w, int h);
    void            initCudaVideo();

    void            freeCudaResources(bool bDestroyContext);

    HRESULT         cleanup(bool bDestroyContext);
    HRESULT         initCudaResources(int bUseInterop, int bTCC);

    void            parseCommandLineArguments();
    void            saveFrameAsYUV(unsigned char *pdst, const unsigned char *psrc, int width, int height, int pitch);
    HRESULT         reinitCudaResources();

    std::string         sFileName;

    FrameQueue          *g_pFrameQueue;
    VideoSource         *g_pVideoSource;
    VideoParser         *g_pVideoParser;
    VideoDecoder        *g_pVideoDecoder;

    CUVIDEOFORMAT       g_stFormat;
    bool                g_bFrameRepeat;
    int                 g_iRepeatFactor;
    cudaVideoCreateFlags g_eVideoCreateFlags;

    bool                g_bWaived;


    bool copyDecodedFrameToTexture(unsigned char* buf);

    bool copyDecodedFrameToTexture(unsigned char** ppImgData, int height, int width, GLuint &cudaTextureID, HDC &mainDC,HGLRC &mainGLRC, bool &needCudaMalloc);

private:

    StopWatchInterface *frame_timer;
    StopWatchInterface *global_timer;

    bool                g_bDeviceLost;
    bool                g_bDone;
    bool                g_bRunning;
    bool                g_bAutoQuit;
    bool                g_bUseVsync;
    //bool                g_bFrameRepeat;
    bool                g_bFrameStep;
    bool                g_bQAReadback;
    bool                g_bFirstFrame;
    bool                g_bLoop;
    bool                g_bUseDisplay;
    bool                g_bUseInterop;
    bool                g_bReadback;
    bool                g_bWriteFile;
    bool                g_bIsProgressive;
    bool                g_bException;

    //int                 g_iRepeatFactor;
    long                g_nFrameStart;
    long                g_nFrameEnd;

    FILE *fpWriteYUV;
    FILE *fpRefYUV;

    //cudaVideoCreateFlags g_eVideoCreateFlags;
    CUvideoctxlock       g_CtxLock;

    float present_fps, decoded_fps, total_time;

    IDirect3DDevice9    *g_pD3DDevice;

    CUmodule            cuModNV12toARGB;
    CUfunction          g_kernelNV12toARGB;
    CUfunction          g_kernelPassThru;

    CUcontext           g_oContext;
    CUdevice            g_oDevice;

    CUstream            g_ReadbackSID, g_KernelSID;


    // System Memory surface we want to readback to
    BYTE                *g_pFrameYUV[6] /*= { 0, 0, 0, 0, 0, 0 }*/;

    // CUVIDEOFORMAT       g_stFormat;

    char                exec_path[256];

    unsigned int        g_nWindowWidth;
    unsigned int        g_nWindowHeight;

    unsigned int        g_nVideoWidth;
    unsigned int        g_nVideoHeight;

    unsigned int        g_FrameCount;
    unsigned int        g_DecodeFrameCount;
    unsigned int        g_fpsCount;      // FPS count for averaging
    unsigned int        g_fpsLimit;     // FPS limit for sampling timer;

    const char *sAppName;
    const char *sAppFilename;
    const char *sSDKname;

};

#endif