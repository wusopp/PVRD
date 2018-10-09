#include <fstream>
#include "Player.h"
#include "yuvConverter.h"
#include <cuda.h>
#include "../NvCodec/NvDecoder/NvDecoder.h"
#include "FFmpegDemuxer.h"
#include "../Utils/NvCodecUtils.h"


simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void readDataFromFile(const char *fileName, unsigned char *yuvData, int frameWidth, int frameHeight) {
    if(yuvData == NULL) {
        return;
    }
    size_t size = frameWidth * frameHeight * 3 / 2;
    FILE *pf = fopen(fileName, "rb+");
    fread(yuvData, 1, size, pf);
    fclose(pf);
    return;
}

void convertYUV2RGB(unsigned char *yuvData, unsigned char *rgbData, int frameWidth, int frameHeight) {
    if(frameWidth < 1 || frameHeight < 1 || yuvData == NULL || rgbData == NULL) {
        return;
    }
    yuv420p_to_rgb24(yuvData, rgbData, frameWidth, frameHeight);
}

void ShowDecoderCapability() {
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    std::cout << "Decoder Capability" << std::endl << std::endl;
    const char *aszCodecName[] = { "JPEG", "MPEG1", "MPEG2", "MPEG4", "H264", "HEVC", "HEVC", "HEVC", "VC1", "VP8", "VP9", "VP9", "VP9" };
    cudaVideoCodec aeCodec[] = { cudaVideoCodec_JPEG, cudaVideoCodec_MPEG1, cudaVideoCodec_MPEG2, cudaVideoCodec_MPEG4,
        cudaVideoCodec_H264, cudaVideoCodec_HEVC, cudaVideoCodec_HEVC, cudaVideoCodec_HEVC, cudaVideoCodec_VC1,
        cudaVideoCodec_VP8, cudaVideoCodec_VP9, cudaVideoCodec_VP9, cudaVideoCodec_VP9 };
    int anBitDepthMinus8[] = { 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 0, 2, 4 };
    for (int iGpu = 0; iGpu < nGpu; iGpu++) {
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        std::cout << "GPU " << iGpu << " - " << szDeviceName << std::endl << std::endl;
        for (int i = 0; i < sizeof(aeCodec) / sizeof(aeCodec[0]); i++) {
            CUVIDDECODECAPS decodeCaps = {};
            decodeCaps.eCodecType = aeCodec[i];
            decodeCaps.eChromaFormat = cudaVideoChromaFormat_420;
            decodeCaps.nBitDepthMinus8 = anBitDepthMinus8[i];

            cuvidGetDecoderCaps(&decodeCaps);
            std::cout << "Codec" << "  " << aszCodecName[i] << '\t' <<
                "BitDepth" << "  " << decodeCaps.nBitDepthMinus8 + 8 << '\t' <<
                "Supported" << "  " << (int)decodeCaps.bIsSupported << '\t' <<
                "MaxWidth" << "  " << decodeCaps.nMaxWidth << '\t' <<
                "MaxHeight" << "  " << decodeCaps.nMaxHeight << '\t' <<
                "MaxMBCount" << "  " << decodeCaps.nMaxMBCount << '\t' <<
                "MinWidth" << "  " << decodeCaps.nMinWidth << '\t' <<
                "MinHeight" << "  " << decodeCaps.nMinHeight << std::endl;
        }

        std::cout << std::endl;


        ck(cuCtxDestroy(cuContext));
    }
}

int main(int argv, char** args) {

    int patches;
    if(argv == 1) {
        patches = 128;
    } else {
        patches = atoi(args[1]);
        if(patches == 0) {
            std::cout << "Error: patches number not valid!" << std::endl;
            return 0;
        }
    }

    //ShowDecoderCapability();
/*
    int frameWidth = 3840;
    int frameHeight = 1920;*/

    //int frameWidth = 1280;
    //int frameHeight = 640;

    int frameWidth = 1920;
    int frameHeight = 960;


    /*int iGpu = 0;
    char *fileName = args[2];
    CheckInputFile(fileName);
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    CUdevice cuDevice = 0;
    ck(cuDeviceGet(&cuDevice, iGpu));*/

    Player::Player *player = new Player::Player(frameWidth, frameHeight, patches);

    std::string filePath = "D:\\WangZewei\\360Video\\shark.mp4";

    player->openVideoFile(filePath);
    player->setupMode(EQUIRECTANGULAR, USE_INDEX);
    player->renderLoop();

    /*uint8_t *yuvData = new uint8_t[frameHeight*frameWidth * 3 / 2];
    if(yuvData == NULL) {
        std::cout << __FUNCTION__ << " - Failed to allocate memory for YUV data." << std::endl;
        return 0; 
    }
    uint8_t *rgbData = new uint8_t[frameHeight*frameWidth * 3];
    if(rgbData == NULL) {
        std::cout << __FUNCTION__ << " - Failed to allocate memory for RGB data." << std::endl;
        return 0;
    }

    readDataFromFile("D:\\WangZewei\\YUV\\kite.yuv", yuvData, frameWidth, frameHeight);
    convertYUV2RGB(yuvData, rgbData, frameWidth, frameHeight);
    player->setupMode(EQUIRECTANGULAR, DONT_USE_INDEX);
    player->setupTextureData(rgbData);
    player->renderLoop();

    readDataFromFile("D:\\WangZewei\\YUV\\kite_cpp.yuv", yuvData, frameWidth, frameHeight);
    convertYUV2RGB(yuvData, rgbData, frameWidth, frameHeight);
    player->setupMode(EQUAL_DISTANCE, DONT_USE_INDEX);
    player->setupTextureData(rgbData);
    player->renderLoop();

    delete[] yuvData;
    delete[] rgbData;*/
    delete player;
    return 0;
}



