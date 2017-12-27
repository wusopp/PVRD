#include "GLHelper.h"
#include "yuvConverter.h"
#include <fstream>
void readDataFromFile(const char *fileName, unsigned char *yuvData, int frameWidth, int frameHeight) {
	if (yuvData == NULL) {
		return;
	}
	std::ifstream fin;
	fin.open(fileName);
	if (fin.fail()) {
		std::cout << __FUNCTION__ << " - Failed to open filename: " << fileName << std::endl;
		return;
	}
	fin.read((char *)yuvData, frameHeight * frameWidth * 1.5);
	return;
}

void convertYUV2RGB(unsigned char *yuvData, unsigned char *rgbData, int frameWidth, int frameHeight) {
	if (frameWidth < 1 || frameHeight < 1 || yuvData == NULL || rgbData == NULL) {
		return;
	}
	yuv420p_to_rgb24(yuvData, rgbData, frameWidth, frameHeight);
}

int main(int argv, char** args) {
	GLHelper *helper = new GLHelper;
	helper->init();
	
	int frameHeight = 1920;
	int frameWidth = 3840;
	unsigned char *yuvData = new unsigned char[frameHeight * frameWidth * 3 / 2];
	if (yuvData == NULL) {
		std::cout << __FUNCTION__ << " - Failed to allocate memory fro YUV data." << std::endl;
		return 0;
	}
	unsigned char *rgbData = new unsigned char[frameWidth * frameHeight * 3];
	if (rgbData == NULL) {
		std::cout << __FUNCTION__ << " - Failed to allocate memory fro RGB data." << std::endl;
		return 0;
	}
	readDataFromFile("cpp.yuv", yuvData,frameWidth, frameHeight);
	convertYUV2RGB(yuvData, rgbData, frameWidth, frameHeight);
	helper->setupTextureData(rgbData, frameWidth, frameHeight);
	helper->renderLoop();
	delete[] yuvData;
	delete[] rgbData;
	return 0;
}

