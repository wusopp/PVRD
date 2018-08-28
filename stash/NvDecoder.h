#ifndef NV_DECODER_H_

#define NV_DECODER_H_


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#pragma comment(lib, "version.lib")
#endif
//#include <glew.h>
//#include "d3d9.h"
//#include "d3dx9.h"
//
//#pragma comment(lib, "d3d9.lib")
//#pragma comment(lib, "d3dx9.lib")

// CUDA Header includes
//#include "dynlink_nvcuvid.h"
//#include "dynlink_cuda.h"
//#include "dynlink_cudaD3D9.h"
//#include "dynlink_cudaD3D10.h"
//#include "dynlink_builtin_types.h"

#include "dynlink_nvcuvid.h"

// CUDA utilities and system includes
#include "helper_functions.h"
#include "helper_cuda_drvapi.h"

// cudaDecodeD3D9 related helper functions
#include "FrameQueue.h"
#include "VideoSource.h"
#include "VideoParser.h"
#include "VideoDecoder.h"

#include "cudaModuleMgr.h"

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

#define VIDEO_FRAME_HEIGHT 1920
#define VIDEO_FRAME_WIDTH 3840


typedef struct OutputBuffer {
    uint8_t			*data;
    int				length;
    volatile bool   used;
    int				flags;
    int			yStride;
    int			uStride;
    int			vStride;
    int		    mWidth;
    int 		mHeight;
    uint32_t		timestamp;
    int				index;
    int				decodeTime;
    int				err_code;

    //new add
    AVFrame			*frame;

    int             frameCnt;

} OutputBuffer;
 
class NvDecoder {

public:
    NvDecoder();
    bool            init(AVFormatContext *pFormatCtx, int bUseInterop, int bTCC);
    // Forward declarations
    bool            loadVideoSource(AVFormatContext *pFormatCtx);
    void            initCudaVideo();

    void            freeCudaResources(bool bDestroyContext);

    bool            copyDecodedFrameToTexture(OutputBuffer *output);
    HRESULT         cleanup(bool bDestroyContext);
    HRESULT         initCudaResources(int bUseInterop, int bTCC);

    void            renderVideoFrame(bool bUseInterop);

    void            parseCommandLineArguments();
    void            SaveFrameAsYUV(unsigned char *pdst, const unsigned char *psrc, int width, int height, int pitch);
    HRESULT         reinitCudaResources();

    void                stop();
    std::string         sFileName;

    FrameQueue          *g_pFrameQueue;
    VideoSource         *g_pVideoSource;
    VideoParser         *g_pVideoParser;
    VideoDecoder        *g_pVideoDecoder;

    bool                g_bWaived;

private:

    StopWatchInterface *frame_timer;
    StopWatchInterface *global_timer;

    bool                g_bDeviceLost;
    bool                g_bDone;
    bool                g_bRunning;
    bool                g_bAutoQuit;
    bool                g_bUseVsync;
    bool                g_bFrameRepeat;
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

    int                 g_iRepeatFactor;
    long                g_nFrameStart;
    long                g_nFrameEnd;

    FILE *fpWriteYUV;
    FILE *fpRefYUV;

    cudaVideoCreateFlags g_eVideoCreateFlags;
    CUvideoctxlock       g_CtxLock;

    float present_fps, decoded_fps, total_time = 0.0f;

    //IDirect3DDevice9    *g_pD3DDevice;

    CUmodule            cuModNV12toARGB = 0;
    CUfunction          g_kernelNV12toARGB = 0;
    CUfunction          g_kernelPassThru = 0;

    CUcontext           g_oContext = 0;
    CUdevice            g_oDevice = 0;

    CUstream            g_ReadbackSID = 0, g_KernelSID = 0;


    // System Memory surface we want to readback to
    BYTE                *g_pFrameYUV[6] /*= { 0, 0, 0, 0, 0, 0 }*/;

    CUVIDEOFORMAT       g_stFormat;

    char                exec_path[256];

    unsigned int        g_nWindowWidth;
    unsigned int        g_nWindowHeight;

    unsigned int        g_nVideoWidth;
    unsigned int        g_nVideoHeight;

    unsigned int        g_FrameCount;
    unsigned int        g_DecodeFrameCount;
    unsigned int        g_fpsCount;      // FPS count for averaging
    unsigned int        g_fpsLimit;     // FPS limit for sampling timer;

    const char *sAppName = "NVDECODE/D3D9 Video Decoder";
    const char *sAppFilename = "NVDecodeD3D9";
    const char *sSDKname = "NVDecodeD3D9";

};

#endif