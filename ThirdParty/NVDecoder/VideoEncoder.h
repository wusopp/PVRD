#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H
#include "AVFrameManager.h"
class VideoEncoder
{
public:
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx;
    AVFrame *pFrame;
    AVPacket pkt;
    AVCodecID codec_id;
    FILE *fp_out;
    int frameCnt;
    int ysize;
    struct EncNodeParam encParam;
    VideoEncoder(struct EncNodeParam encNodeParam):encParam(encNodeParam) {};
    int init();
    int encode(AVFrameList* psourceVList,HANDLE& mtx);
    void destroy();
    int flush();
};

#endif