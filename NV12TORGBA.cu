// NV12ToRGBA
// cuda
#include "cuda.h"   
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "device_functions.h" // 此头文件包含 __syncthreads ()函数
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <math.h>

__global__ void YCrCb2RGBConver(unsigned char *pYdata, unsigned char *pUVdata, int stepY, int stepUV, 
	unsigned char *pImgData, int width, int height, int channels)
{
	const int tidx = blockIdx.x * blockDim.x + threadIdx.x;
	const int tidy = blockIdx.y * blockDim.y + threadIdx.y;

	if (tidx < width && tidy < height)
	{
		int indexY, indexU, indexV;
		unsigned char Y, U, V;
		indexY = tidy * stepY + tidx;
		Y = pYdata[indexY];
		
		if (tidx % 2 == 0)
		{
			indexU = tidy / 2 * stepUV + tidx;
			indexV = tidy / 2 * stepUV + tidx + 1;
			U = pUVdata[indexU];
			V = pUVdata[indexV];
		}
		else if (tidx % 2 == 1)
		{
			indexV = tidy / 2 * stepUV + tidx;
			indexU = tidy / 2 * stepUV + tidx - 1;
			U = pUVdata[indexU];
			V = pUVdata[indexV];
		}

		int index = (tidy*width + tidx) * channels;
		pImgData[index + 0] = unsigned char(Y + 1.402 * (V - 128));
		pImgData[index + 1] = unsigned char(Y - 0.34413 * (U - 128) - 0.71414*(V - 128));
		pImgData[index + 2] = unsigned char(Y + 1.772*(U - 128));
		pImgData[index + 3] = unsigned char(255);
	}
}

extern "C" void NV12TORGBA(unsigned char *pYdata, unsigned char *pUVdata, int stepY, int stepUV,
	unsigned char *pImgData, int width, int height, int channels) {

	int uint = 32;
	dim3 block(uint, uint);
	int wblock = (width + block.x - 1) / block.x;
	int hblock = (height + block.y - 1) / block.y;
	//std::cout << wblock << std::ends << hblock << std::endl;
	dim3 grid(wblock, hblock);

	YCrCb2RGBConver << <grid, block >> >(pYdata, pUVdata, stepY, stepUV, pImgData, width, height, 4);
}




