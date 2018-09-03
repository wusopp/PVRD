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

/*void rgb24ToBMP(unsigned char *rgbBuffer, int width, int height, const char *bmppath) {
    typedef struct {
        long imageSize;
        long blank;
        long startPosition;
    } BmpHead;

    typedef struct {
        long  Length;
        long  width;
        long  height;
        unsigned short  colorPlane;
        unsigned short  bitColor;
        long  zipFormat;
        long  realSize;
        long  xPels;
        long  yPels;
        long  colorUse;
        long  colorImportant;
    } InfoHead;

    int i = 0, j = 0;
    BmpHead m_BMPHeader = { 0 };
    InfoHead  m_BMPInfoHeader = { 0 };
    char bfType[2] = { 'B','M' };
    int header_size = sizeof(bfType) + sizeof(BmpHead) + sizeof(InfoHead);
    FILE *fp_bmp = NULL;

    if((fp_bmp = fopen(bmppath, "wb")) == NULL) {
        printf("Error: Cannot open output BMP file.\n");
        return;
    }

    m_BMPHeader.imageSize = 3 * width*height + header_size;
    m_BMPHeader.startPosition = header_size;

    m_BMPInfoHeader.Length = sizeof(InfoHead);
    m_BMPInfoHeader.width = width;
    //BMP storage pixel data in opposite direction of Y-axis (from bottom to top).
    m_BMPInfoHeader.height = -height;
    m_BMPInfoHeader.colorPlane = 1;
    m_BMPInfoHeader.bitColor = 24;
    m_BMPInfoHeader.realSize = 3 * width*height;

    fwrite(bfType, 1, sizeof(bfType), fp_bmp);
    fwrite(&m_BMPHeader, 1, sizeof(m_BMPHeader), fp_bmp);
    fwrite(&m_BMPInfoHeader, 1, sizeof(m_BMPInfoHeader), fp_bmp);

    for(j = 0; j < height; j++) {
        for(i = 0; i < width; i++) {
            char temp = rgbBuffer[(j*width + i) * 3 + 2];
            rgbBuffer[(j*width + i) * 3 + 2] = rgbBuffer[(j*width + i) * 3 + 0];
            rgbBuffer[(j*width + i) * 3 + 0] = temp;
        }
    }
    fwrite(rgbBuffer, 3 * width*height, 1, fp_bmp);
    fclose(fp_bmp);
    printf("Finish generate %s!\n", bmppath);
    return;
}*/

void YV12ToBGR24_Native(uint8_t* pYUV, uint8_t* pBGR24, int width, int height) {
    if(width < 1 || height < 1 || pYUV == NULL || pBGR24 == NULL)
        return;
    const long len = width * height;
    unsigned char* yData = pYUV;
    unsigned char* vData = &yData[len];
    unsigned char* uData = &vData[len >> 2];

    int bgr[3];
    int yIdx, uIdx, vIdx, idx;
    for(int i = 0; i < height; i++) {
        for(int j = 0; j < width; j++) {
            yIdx = i * width + j;
            vIdx = (i / 2) * (width / 2) + (j / 2);
            uIdx = vIdx;
            bgr[0] = (int)(yData[yIdx] + (uData[vIdx] - 128) + (((uData[vIdx] - 128) * 198) >> 8));                                    // b分量
            bgr[1] = (int)(yData[yIdx] - (((uData[vIdx] - 128) * 88) >> 8) - (((vData[vIdx] - 128) * 183) >> 8));    // g分量
            bgr[2] = (int)(yData[yIdx] + (vData[uIdx] - 128) + (((vData[uIdx] - 128) * 103) >> 8));

            for(int k = 0; k < 3; k++) {
                idx = (i * width + j) * 3 + k;
                if(bgr[k] >= 0 && bgr[k] <= 255)
                    pBGR24[idx] = bgr[k];
                else
                    pBGR24[idx] = (bgr[k] < 0) ? 0 : 255;
            }
        }
    }
    return;
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
        patches = 512;
    } else {
        patches = atoi(args[1]);
        if(patches == 0) {
            std::cout << "Error: patches number not valid!" << std::endl;
            return 0;
        }
    }

    //ShowDecoderCapability();

    int frameHeight = 1920;
    int frameWidth = 3840;

    /*int iGpu = 0;
    char *fileName = args[2];
    CheckInputFile(fileName);
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    CUdevice cuDevice = 0;
    ck(cuDeviceGet(&cuDevice, iGpu));*/

    Player *player = new Player(frameWidth, frameHeight, patches);
    uint8_t *yuvData = new uint8_t[frameHeight*frameWidth * 3 / 2];
    if(yuvData == NULL) {
        std::cout << __FUNCTION__ << " - Failed to allocate memory for YUV data." << std::endl;
        return 0;
    }
    uint8_t *rgbData = new uint8_t[frameHeight*frameWidth * 3];
    if(rgbData == NULL) {
        std::cout << __FUNCTION__ << " - Failed to allocate memory for RGB data." << std::endl;
        return 0;
    }

    readDataFromFile("ZSP_CPP.yuv", yuvData, frameWidth, frameHeight);
    convertYUV2RGB(yuvData, rgbData, frameWidth, frameHeight);
    player->setupMode(EQUAL_AREA, USE_INDEX);
    player->setupTextureData(rgbData);
    player->renderLoop();

    readDataFromFile("ZSP.yuv", yuvData, frameWidth, frameHeight);
    convertYUV2RGB(yuvData, rgbData, frameWidth, frameHeight);
    player->setupMode(EQUIRECTANGULAR, USE_INDEX);
    player->setupTextureData(rgbData);
    player->renderLoop();


    delete[] yuvData;
    delete[] rgbData;
    delete player;
    return 0;
}



