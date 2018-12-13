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
VideoFileType videoFileType = VFT_Encoded;
DecodeType decodeType = DT_SOFTWARE;
int patchNum;
int frameWidth;
int frameHeight;
bool renderYUV = false;
bool repeatRendering = false;



int main(int argc, char** argv) {
    

	Player::Player *player = new Player::Player(argc,argv);
    
	player->openVideo();

	player->setupThread();
	player->renderLoopThread();

	delete player;
	return 0;
}



