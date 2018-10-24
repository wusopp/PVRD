#ifndef AUDIOENCODER_H
#define AUDIOENCODER_H
#include "AVFrameManager.h"
class AudioEncoder
{
public:
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx;
    AVFormatContext* pFormatCtx;
    AVOutputFormat* fmt;
    AVStream* audio_st;
    AVFrame* pframe;
    int frameCnt;
    int size;
    struct EncNodeParam encParam;
    AudioEncoder(struct EncNodeParam encNodeParam):encParam(encNodeParam) {};
    int init();
    int encode(AVFrameList* psourceAList,HANDLE* mtx);
    void destroy();

};

#endif