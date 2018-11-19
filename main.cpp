#include <fstream>
#include "Player.h"
#include "yuvConverter.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "../NVDecoder/NvDecoder.h"

void readDataFromFile(const char *fileName, unsigned char *yuvData, int frameWidth, int frameHeight) {
	if (yuvData == NULL) {
		return;
	}
	size_t size = frameWidth * frameHeight * 3 / 2;
	FILE *pf = fopen(fileName, "rb+");
	fread(yuvData, 1, size, pf);
	fclose(pf);
	return;
}

void convertYUV2RGB(unsigned char *yuvData, unsigned char *rgbData, int frameWidth, int frameHeight) {
	if (frameWidth < 1 || frameHeight < 1 || yuvData == NULL || rgbData == NULL) {
		return;
	}
	yuv420p_to_rgb24(yuvData, rgbData, frameWidth, frameHeight);
}

std::string videoFileName;
std::string outputImageName;
ProjectionMode projectionMode = PM_ERP;
DrawMode drawMode = DM_USE_INDEX;
VideoFileType videoFileType;
DecodeType decodeType = DT_SOFTWARE;
int patchNum;
int frameWidth;
int frameHeight;
bool renderYUV = false;
bool repeatRendering = false;

// proj: 0-ERP, 1-CPP_Obsolete, 2-Cubemap, 3-Cpp, 4-Notspecial
// draw: 0-useIndex, 1-dontUseIndex
// type: 0-yuv, 1-encoded
// decode: 0-software, 1-hardware
// -patch 200 -video D:\\WangZewei\\360Video\\VRTest_1920_960.mp4 -output 200.png -proj 0 -draw 0 -decode 0 -type 1 -w 1920 -h 960 -repeat 0 -yuv 0
void parseArguments(int argc, char *argv[]) {
    if (!stricmp(argv[1], "-h") || !stricmp(argv[1], "-help")) {
        std::cout << "Arguments Format:\n-patch 200 -video D:\\WangZewei\\360Video\\VRTest_1920_960.mp4 -output 200.png -proj 0 -draw 0 -decode 0 -type 0 -w 1920 -h 960 -repeat 0 -yuv 0\n";
    } else {
        {
            for (int i = 1; i < argc; i += 2) {
                if (!stricmp(argv[i], "-patch")) {
                    patchNum = atoi(argv[i + 1]);
                } else if (!stricmp(argv[i], "-video")) {
                    videoFileName = argv[i + 1];
                } else if (!stricmp(argv[i], "-output")) {
                    outputImageName = argv[i + 1];
                } else if (!stricmp(argv[i], "-proj")) {
                    int value = atoi(argv[i + 1]);
                    switch (value) {
                    case PM_ERP:
                        projectionMode = PM_ERP;
                        break;
                    case PM_CUBEMAP:
                        projectionMode = PM_CUBEMAP;
                        break;
                    case PM_CPP:
                        projectionMode = PM_CPP;
                        break;
                    case PM_CPP_OBSOLETE:
                        projectionMode = PM_CPP_OBSOLETE;
                        break;
                    default:
                        break;
                    }
                } else if (!stricmp(argv[i], "-draw")) {
                    int draw = atoi(argv[i + 1]);
                    switch (draw) {
                    case DM_USE_INDEX:
                        drawMode = DM_USE_INDEX;
                        break;
                    case DM_DONT_USE_INDEX:
                        drawMode = DM_DONT_USE_INDEX;
                        break;
                    default:
                        break;
                    }
                } else if (!stricmp(argv[i], "-decode")) {
                    int value = atoi(argv[i + 1]);
                    switch (value) {
                    case DT_SOFTWARE:
                        decodeType = DT_SOFTWARE;
                        break;
                    case DT_HARDWARE:
                        decodeType = DT_HARDWARE;
                        break;
                    default:
                        break;
                    }
                } else if (!stricmp(argv[i], "-type")) {
                    int type = atoi(argv[i+1]);
                    switch (type) {
                    case VFT_Encoded:
                        videoFileType = VFT_Encoded;
                        break;
                    case VFT_YUV:
                        videoFileType = VFT_YUV;
                        break;
                    default:
                        break;
                    }
                } else if (!stricmp(argv[i], "-w")) {
                    frameWidth = atoi(argv[i + 1]);
                } else if (!stricmp(argv[i], "-h")) {
                    frameHeight = atoi(argv[i + 1]);
                } else if (!stricmp(argv[i], "-repeat")) {
                    repeatRendering = (atoi(argv[i + 1]) == 0 ? false : true);
                } else if (!stricmp(argv[i], "-yuv")) {
                    renderYUV = (atoi(argv[i + 1]) == 0 ? false : true);
                }
            }
        }
    }
}

//void ShowDecoderCapability() {
//    ck(cuInit(0));
//    int nGpu = 0;
//    ck(cuDeviceGetCount(&nGpu));
//    std::cout << "Decoder Capability" << std::endl << std::endl;
//    const char *aszCodecName[] = { "JPEG", "MPEG1", "MPEG2", "MPEG4", "H264", "HEVC", "HEVC", "HEVC", "VC1", "VP8", "VP9", "VP9", "VP9" };
//    cudaVideoCodec aeCodec[] = { cudaVideoCodec_JPEG, cudaVideoCodec_MPEG1, cudaVideoCodec_MPEG2, cudaVideoCodec_MPEG4,
//        cudaVideoCodec_H264, cudaVideoCodec_HEVC, cudaVideoCodec_HEVC, cudaVideoCodec_HEVC, cudaVideoCodec_VC1,
//        cudaVideoCodec_VP8, cudaVideoCodec_VP9, cudaVideoCodec_VP9, cudaVideoCodec_VP9 };
//    int anBitDepthMinus8[] = { 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 0, 2, 4 };
//    for (int iGpu = 0; iGpu < nGpu; iGpu++) {
//        CUdevice cuDevice = 0;
//        ck(cuDeviceGet(&cuDevice, iGpu));
//        char szDeviceName[80];
//        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
//        CUcontext cuContext = NULL;
//        ck(cuCtxCreate(&cuContext, 0, cuDevice));
//
//        std::cout << "GPU " << iGpu << " - " << szDeviceName << std::endl << std::endl;
//        for (int i = 0; i < sizeof(aeCodec) / sizeof(aeCodec[0]); i++) {
//            CUVIDDECODECAPS decodeCaps = {};
//            decodeCaps.eCodecType = aeCodec[i];
//            decodeCaps.eChromaFormat = cudaVideoChromaFormat_420;
//            decodeCaps.nBitDepthMinus8 = anBitDepthMinus8[i];
//
//            cuvidGetDecoderCaps(&decodeCaps);
//            std::cout << "Codec" << "  " << aszCodecName[i] << '\t' <<
//                "BitDepth" << "  " << decodeCaps.nBitDepthMinus8 + 8 << '\t' <<
//                "Supported" << "  " << (int)decodeCaps.bIsSupported << '\t' <<
//                "MaxWidth" << "  " << decodeCaps.nMaxWidth << '\t' <<
//                "MaxHeight" << "  " << decodeCaps.nMaxHeight << '\t' <<
//                "MaxMBCount" << "  " << decodeCaps.nMaxMBCount << '\t' <<
//                "MinWidth" << "  " << decodeCaps.nMinWidth << '\t' <<
//                "MinHeight" << "  " << decodeCaps.nMinHeight << std::endl;
//        }
//
//        std::cout << std::endl;
//
//
//        ck(cuCtxDestroy(cuContext));
//    }
//}

int main(int argc, char** argv) {
    parseArguments(argc, argv);

	Player::Player *player = new Player::Player(frameWidth, frameHeight, patchNum);
    player->setViewportImageFileName(outputImageName);

	//std::string filePath = "D:\\WangZewei\\360Video\\VRTest_1920_960.mp4";
	//std::string filePath = "D:\\WangZewei\\YUV\\road.yuv";
    //std::string filePath = "D:\\WangZewei\\360Video\\Cubemap_1536_1024.mp4";

    player->setRepeatRendering(repeatRendering);
    player->setRenderYUV(renderYUV);
	player->setupMode(projectionMode, drawMode, decodeType, videoFileType);
	player->openVideoFile(videoFileName);

	player->setupThread();
	player->renderLoopThread();

	delete player;
	return 0;
}



