#include "GLHelper.h"
#include "yuvConverter.h"
#include <fstream>
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

void rgb24ToBMP(unsigned char *rgbBuffer, int width, int height, const char *bmppath) {
	typedef struct
	{
		long imageSize;
		long blank;
		long startPosition;
	}BmpHead;

	typedef struct
	{
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
	}InfoHead;

	int i = 0, j = 0;
	BmpHead m_BMPHeader = { 0 };
	InfoHead  m_BMPInfoHeader = { 0 };
	char bfType[2] = { 'B','M' };
	int header_size = sizeof(bfType) + sizeof(BmpHead) + sizeof(InfoHead);
	FILE *fp_bmp = NULL;

	if ((fp_bmp = fopen(bmppath, "wb")) == NULL) {
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

	//BMP save R1|G1|B1,R2|G2|B2 as B1|G1|R1,B2|G2|R2  
	//It saves pixel data in Little Endian  
	//So we change 'R' and 'B'  
	for (j = 0; j<height; j++) {
		for (i = 0; i<width; i++) {
			char temp = rgbBuffer[(j*width + i) * 3 + 2];
			rgbBuffer[(j*width + i) * 3 + 2] = rgbBuffer[(j*width + i) * 3 + 0];
			rgbBuffer[(j*width + i) * 3 + 0] = temp;
		}
	}
	fwrite(rgbBuffer, 3 * width*height, 1, fp_bmp);
	fclose(fp_bmp);
	printf("Finish generate %s!\n", bmppath);
	return;
}


void YV12ToBGR24_Native(uint8_t* pYUV, uint8_t* pBGR24, int width, int height) {
    if (width < 1 || height < 1 || pYUV == NULL || pBGR24 == NULL)
        return;
    const long len = width * height;
    unsigned char* yData = pYUV;
    unsigned char* vData = &yData[len];
    unsigned char* uData = &vData[len >> 2];

    int bgr[3];
    int yIdx, uIdx, vIdx, idx;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            yIdx = i * width + j;
            vIdx = (i / 2) * (width / 2) + (j / 2);
            uIdx = vIdx;
            bgr[0] = (int)(yData[yIdx] + (uData[vIdx] - 128) + (((uData[vIdx] - 128) * 198) >> 8));                                    // b分量
            bgr[1] = (int)(yData[yIdx] - (((uData[vIdx] - 128) * 88) >> 8) - (((vData[vIdx] - 128) * 183) >> 8));    // g分量
            bgr[2] = (int)(yData[yIdx] + (vData[uIdx] - 128) + (((vData[uIdx] - 128) * 103) >> 8));

            for (int k = 0; k < 3; k++) {
                idx = (i * width + j) * 3 + k;
                if (bgr[k] >= 0 && bgr[k] <= 255)
                    pBGR24[idx] = bgr[k];
                else
                    pBGR24[idx] = (bgr[k] < 0) ? 0 : 255;
            }
        }
    }
    return;
}

int main(int argv, char** args) {
	GLHelper *helper = new GLHelper;
	helper->init();
	
	int frameHeight = 1920;
	int frameWidth = 3840;
    uint8_t *yuvData = (uint8_t *)malloc(sizeof(uint8_t)*frameHeight*frameWidth * 3 / 2);
	if (yuvData == NULL) {
		std::cout << __FUNCTION__ << " - Failed to allocate memory fro YUV data." << std::endl;
		return 0;
	}
    uint8_t *rgbData = (uint8_t *)malloc(sizeof(uint8_t)*frameHeight*frameWidth * 3);
	if (rgbData == NULL) {
		std::cout << __FUNCTION__ << " - Failed to allocate memory fro RGB data." << std::endl;
		return 0;
	}
	readDataFromFile("line.yuv", yuvData,frameWidth, frameHeight);
	convertYUV2RGB(yuvData, rgbData, frameWidth, frameHeight);
	helper->setupTextureData(rgbData, frameWidth, frameHeight);
	helper->renderLoop();
	free(yuvData);
	free(rgbData);
    delete helper;
	return 0;
}

