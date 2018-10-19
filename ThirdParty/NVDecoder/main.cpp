#include<iostream>
#include <string>
#include "AVFrameManager.h"
#include "AVPacketManager.h"
#include "NvDecoder.h"
#include "NvEncoder.h"
#include "audioplayer.h"
#include "audioEncoder.h"
#include "VideoEncoder.h"
#include "readtoMemory.h"
using namespace std;
#define FRAMENUM 150 //有几帧在缓存里没有输出成yuv 需要重新刷新。目前一个ts共150帧
//class Task{
//public:
//    //VideoEncoder* ve;
//    CNvEncoder* nvEncoder;
//    AVFrameList* list;
//    HANDLE mtx;
//    HANDLE runHandle;
//    void run(){
//        while((WaitForSingleObject(runHandle, 10L)) == WAIT_TIMEOUT)
//        {
//            if(list->present != NULL)
//            {
//                WaitForSingleObject(mtx,INFINITE);
//                while(list->present != NULL)
//                {
//                   // ve->encode(list,mtx);
//                     nvEncoder->EncodeMain(list,mtx);
//                }
//                ReleaseMutex(mtx);
//            }
//        }
//        _endthreadex( 0 );
//    }
//} task;
//
//void run(void* task){
//    ((Task*)task)->run();
//}


int main(int argc, char *argv[]) {
    CUVIDSOURCEDATAPACKET inputpkt;
    AVPacket pkt;
    AVFrame *pFrame = NULL;
    AVCodecContext	*pCodecCtx = NULL;
    AVCodec			*pCodec =NULL;
    NvDecoder *pNvDecoder = NULL;
    pNvDecoder = new NvDecoder();
  //  struct SwrContext *au_convert_ctx;///
  //  int out_linesize;//输出音频数据大小，一定小于输出内存。///
  //  int out_buffer_size;//输出内存大小///
  //  uint8_t *out_buffer = NULL;///
    uint32_t ret,len = 0;
    int got_picture = 0;
    int videoStream = -1;
    int m_flush = 0;
    int pos = -1;

    AVFrameManager *AVFrameMag = NULL;
    int subpicNum = 1;//tile num
    create_AVFrameManager(AVFrameMag,subpicNum);
    AVPacketManager *AVPacketMag;
    int streamNum = 1;//subpicNum?
    create_AVPacketManager(AVPacketMag,streamNum);

    EncNodeParam encParam;

    HANDLE mtx = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex
    
    HANDLE runHandle = CreateMutex(NULL,TRUE,NULL);      

    //video
    encParam.bitrate = 400000;
    encParam.fps = 30;
    encParam.GOP = 1;
    encParam.width = 8192;
    encParam.height = 4096;
    encParam.vcodec = CT_H265;
    
    //CNvEncoder nvEncoder;

    {
        //ffmpeg初始化
        av_register_all();
        avformat_network_init();
        AVFormatContext *ifmt_ctx = NULL;
        ifmt_ctx = avformat_alloc_context(); //必不可少！
        
        //if ( avformat_open_input(&ifmt_ctx, tsurl, 0, 0) <0)
		
        if ( avformat_open_input(&ifmt_ctx, "D:\\ByteChen\\videos\\Harbor8192x4096.h265", 0, 0) <0)
            return -1;
        if (avformat_find_stream_info(ifmt_ctx, 0) < 0)                           
            return -2;
//        av_dump_format(ifmt_ctx, 0, url, 0);
        pFrame = av_frame_alloc();

        //初始化硬解码器
        pNvDecoder->parseCommandLineArguments();
        if (pNvDecoder->init(ifmt_ctx, false, 0,encParam.width,encParam.height) == false) {
            printf("NvDecoder init failed\n");
            system("pause");
        }
         //初始化硬编码器
        /*if (nvEncoder.init()!=0)
        {
            printf("NvEncoder init failed\n");
            system("pause");
        }*/

        //videostream index
        for(int i=0; i < ifmt_ctx->nb_streams; i++)
        {
            if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
            {
                videoStream=i;
                break;
            }
        }
        if(videoStream==-1)
        {
            printf("Didn't find a video stream.\n");
            return -1;
        }

		int size = encParam.height * encParam.width * 3 / 2;
		unsigned char* yuv_buf = new unsigned char[size];
		string yuvfile = "D:\\ByteChen\\videos\\out_";
		__int64 mwidth = encParam.width;
		__int64 mheight = encParam.height;
		yuvfile += to_string(mwidth);
		yuvfile += "x";
		yuvfile += to_string(mheight);
		yuvfile += ".yuv";
		FILE* yuvfp = fopen(yuvfile.c_str(), "wb");
		if (yuvfp == NULL) return -1;
        int numberOfFrameDecode = 0;
        while (!av_read_frame(ifmt_ctx, &pkt)) {//按帧解析
            if (pkt.stream_index == videoStream)
            {
				memset(yuv_buf, 0, size);

                 unsigned long long lStart, lEnd, lFreq;
                //inputpkt.timestamp = pkt.pts;
                inputpkt.payload = pkt.data;
                inputpkt.payload_size = pkt.size;
                inputpkt.flags = CUVID_PKT_TIMESTAMP;

                //start时间
                NvQueryPerformanceCounter(&lStart);
                NvQueryPerformanceFrequency(&lFreq);
                cuvidParseVideoData(pNvDecoder->g_pVideoSource->oSourceData_.hVideoParser, &inputpkt);//硬解码
                NvQueryPerformanceCounter(&lEnd);
                
                double elapsedTime = (double)(lEnd - lStart);
                //printf("Decoded %d frames in %6.2fms\n", nvEncoder.numFramesEncoded, (elapsedTime*1000.0)/lFreq);
                printf("Decoded %d frames in %6.2fms\n",++numberOfFrameDecode, (elapsedTime*1000.0)/lFreq);
                //end时间

                //解码输出帧添加到AVFrameMag->psourceVList中
                //pNvDecoder->copyDecodedFrameToTexture(AVFrameMag->psourceVList,encParam.width,encParam.height,mtx);
				//if (pNvDecoder->myCopyDecodedFrameToTexture(yuv_buf, AVFrameMag->psourceVList, encParam.width, encParam.height, mtx))
					//->myCopyDecodedFrameToTexture(yuv_buf);
				if (pNvDecoder->myCopyDecodedFrameToTexture(yuv_buf))
				{
					fwrite(yuv_buf, 1, size, yuvfp);
					//fflush(yuvfp);
				}
                //nvEncoder.EncodeMain(AVFrameMag->psourceVList,mtx);

				//提前退出
				if (numberOfFrameDecode > 120)
					break;
            }
        }
		fclose(yuvfp);
        //nvEncoder.Deinitialize(NV_ENC_CUDA);
        avformat_free_context(ifmt_ctx);
    }

   // ReleaseMutex(runHandle);
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    pNvDecoder->cleanup(true);
 //   free(pBuf);
    return 0;
}


