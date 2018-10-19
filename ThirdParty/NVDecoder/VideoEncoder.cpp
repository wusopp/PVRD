extern "C"
{
#include "libavutil\opt.h"
#include "libavcodec\avcodec.h"
#include "libavutil\imgutils.h"
};

#include "VideoEncoder.h"

int VideoEncoder::init()
{
    char* filename_out;

    int ret ;
    frameCnt = 0;
    
    avcodec_register_all();

    if(encParam.vcodec == 1)
    {
        codec_id = AV_CODEC_ID_H264;
        filename_out = "ds.h264";
    }
    else if(encParam.vcodec == 2)
    {
        codec_id = AV_CODEC_ID_HEVC;
        filename_out = "ds.hevc";
    }

    fp_out = fopen(filename_out, "wb");
	if (!fp_out) {
		printf("Could not open %s\n", filename_out);
		return -1;
	}

    pCodec = avcodec_find_encoder(codec_id);
    if (!pCodec) {
        printf("Codec not found\n");
        return -1;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        printf("Could not allocate video codec context\n");
        return -1;
    }

    pCodecCtx->bit_rate = encParam.bitrate;
    pCodecCtx->width = encParam.width;
    pCodecCtx->height = encParam.height;
    pCodecCtx->time_base.num=1;
    pCodecCtx->time_base.den=encParam.fps;
    pCodecCtx->gop_size = encParam.GOP;
    pCodecCtx->max_b_frames = 0;//0?
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    ysize = encParam.width*encParam.height;

    if (codec_id == AV_CODEC_ID_H264)
        av_opt_set(pCodecCtx->priv_data, "preset", "slow", 0);
    else
    {
        av_opt_set(pCodecCtx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(pCodecCtx->priv_data, "x265-params", "--bframes=0", 0);
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    if (!pFrame) {
        printf("Could not allocate video frame\n");
        return -1;
    }
    pFrame->format = pCodecCtx->pix_fmt;
    pFrame->width  = pCodecCtx->width;
    pFrame->height = pCodecCtx->height;

    ret = av_image_alloc(pFrame->data, pFrame->linesize, pCodecCtx->width, pCodecCtx->height,
        pCodecCtx->pix_fmt, 16);
    if (ret < 0) {
        printf("Could not allocate raw picture buffer\n");
        return -1;
    }
    return 0;
}

int VideoEncoder::encode(AVFrameList* psourceVList,HANDLE& mtx)
{
    int ret,got_output;
    AVFrameNode *pNode = NULL;
    if(get_AVFrameNode(pNode,psourceVList)==0)
    {
        av_init_packet(&pkt);
        pkt.data = NULL;    // packet data will be allocated by the encoder
        pkt.size = 0;
        memcpy(pFrame->data[0],pNode->Yframe,ysize);
       
        memcpy(pFrame->data[1],pNode->Uframe,ysize/4);
     
        memcpy(pFrame->data[2],pNode->Vframe,ysize/4);

        pFrame->pts = frameCnt;
        
        ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_output);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return -1;
        }
        if (got_output) {
            printf("Succeed to encode frame: %5d\tsize:%5d\n",frameCnt,pkt.size);
            frameCnt++;
            pNode->used_flag = 1;
            update_AVFrameList(psourceVList,pNode->used_flag,mtx);
            fwrite(pkt.data, 1, pkt.size, fp_out);
            av_free_packet(&pkt);
         }
    }

    return 0;
}

int VideoEncoder::flush()
{
    int got_output,ret;
    for (got_output = 1; got_output; ) {
        ret = avcodec_encode_video2(pCodecCtx, &pkt, NULL, &got_output);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return -1;
        }
        if (got_output) {
            printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",pkt.size);
            fwrite(pkt.data, 1, pkt.size, fp_out);
            av_free_packet(&pkt);
        }
    }
}

void VideoEncoder::destroy()
{
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);
    av_freep(&pFrame->data[0]);
    av_frame_free(&pFrame);
    fclose(fp_out);
}
