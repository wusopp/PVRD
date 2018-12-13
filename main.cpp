#include <fstream>
#include "Player.h"
#include "yuvConverter.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "../NVDecoder/NvDecoder.h"

int main(int argc, char** argv) {
    

	Player::Player *player = new Player::Player(argc,argv);
    
	player->openVideo();

	player->setupThread();
	player->renderLoopThread();

	delete player;
	return 0;
}



