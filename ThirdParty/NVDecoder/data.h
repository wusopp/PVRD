#ifndef DATA_H_
#define DATA_H_
#include "stdint.h"
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "dynlink_nvcuvid.h"
//#include "SampleDecoder.h"

#define INBUF_SIZE 1024*1024

typedef struct inputBuffer {
    uint8_t			*data[INBUF_SIZE];
    int				length;
    volatile bool	used;
    int				flags;
    uint32_t		mWidth;
    uint32_t		mHeight;
    int				index;
    long			timestamp;
    //new add
    uint32_t		tileMsk;
    AVPacket		pkt;
    CUVIDSOURCEDATAPACKET cupkt;

}InputBuffer;

typedef struct outputBuffer {
    uint8_t			*data;
    int				length;
    volatile bool   used;
    int				flags;
    uint8_t			yStride;
    uint8_t			uStride;
    uint8_t			vStride;
    uint32_t		mWidth;
    uint32_t		mHeight;
    uint32_t		timestamp;
    int				index;
    int				err_code;
    int				decodeTime;
    //new add
    AVFrame			*pic;
    uint32_t		tileMsk;
    int frameCnt;

}OutputBuffer;

#endif