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

int main(int argv, char** args) {

	int patches = 128;
	if (argv == 1) {
		patches = 128;
	} else {
		patches = atoi(args[1]);
		if (patches == 0) {
			std::cout << "Error: patches number not valid!" << std::endl;
			return 0;
		}
	}

	int frameWidth = 1920;
	int frameHeight = 960;

	Player::Player *player = new Player::Player(frameWidth, frameHeight, patches);

	std::string filePath = "D:\\WangZewei\\360Video\\VRTest_1920_960.mp4";
	//std::string filePath = "D:\\WangZewei\\YUV\\road.yuv";
    //std::string filePath = "D:\\WangZewei\\360Video\\Cubemap_1536_1024.mp4";

    player->setRepeatRendering(false);
    player->setRenderYUV(false);
	player->setupMode(PM_ERP, DM_USE_INDEX, DT_SOFTWARE, VFT_Encoded);
	player->openVideoFile(filePath);

	player->setupThread();
	player->renderLoopThread();
	//player->renderLoop();

	delete player;
	return 0;
}



