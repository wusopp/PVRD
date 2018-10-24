#pragma once
extern "C" {
	void NV12TORGBA(unsigned char *pYdata, unsigned char *pUVdata, int stepY, int stepUV,
		unsigned char *pImgData, int width, int height, int channels);
}

