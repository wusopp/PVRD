extern "C"
{
#include "libavcodec\avcodec.h"
#include "libavformat\avformat.h"
#include "libswscale\swscale.h"
};
#include "AudioEncoder.h"
int AudioEncoder::init()
{
    char* filename_out = "out.aac";
    frameCnt = 0;

    av_register_all();
    //����1.���ʹ�ü�������
    pFormatCtx = avformat_alloc_context();
    //�¸�ʽ
    fmt = av_guess_format(NULL, filename_out, NULL);
    pFormatCtx->oformat = fmt;

    //����2.�����Զ���һЩ
    //avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
    //fmt = pFormatCtx->oformat;
    
    if (avio_open(&pFormatCtx->pb,filename_out, AVIO_FLAG_READ_WRITE) < 0)
    {
        printf("����ļ���ʧ�ܣ�\n");
        return -1;
    }

    audio_st = avformat_new_stream(pFormatCtx, 0);
    if (audio_st==NULL){
        return -1;
    }

    pCodecCtx = audio_st->codec;
    pCodecCtx->codec_id = fmt->audio_codec;
    pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
    pCodecCtx->sample_rate= 44100;
    pCodecCtx->channel_layout=AV_CH_LAYOUT_STEREO;
    pCodecCtx->channels = av_get_channel_layout_nb_channels(pCodecCtx->channel_layout);
    pCodecCtx->bit_rate = 64000;  
    //�����ʽ��Ϣ
    av_dump_format(pFormatCtx, 0, filename_out, 1);

    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec)
    {
        printf("û���ҵ����ʵı�������\n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec,NULL) < 0)
    {
        printf("��������ʧ�ܣ�\n");
        return -1;
    }
    pframe = av_frame_alloc();
    pframe->nb_samples= pCodecCtx->frame_size;
    pframe->format= pCodecCtx->sample_fmt;

    size = av_samples_get_buffer_size(NULL, pCodecCtx->channels,pCodecCtx->frame_size,pCodecCtx->sample_fmt, 1);

    //д�ļ�ͷ
    avformat_write_header(pFormatCtx,NULL);

    return 0;
}
int AudioEncoder::encode(AVFrameList* psourceAList,HANDLE* mtx)
{
    int ret;
    int got_frame=0;
    AVFrameNode *pNode = NULL;
    if(get_AVFrameNode(pNode,psourceAList)==0){
        AVPacket pkt;
        av_new_packet(&pkt,size);

        pframe->data[0] = pNode->Yframe;  //�����ź�

        pframe->pts=frameCnt*100;
       
        //����
        int ret = avcodec_encode_audio2(pCodecCtx, &pkt,pframe, &got_frame);
        if(ret < 0)
        {
            printf("�������\n");
            return -1;
        }
        if (got_frame==1)
        {
            printf("����ɹ���%d֡��\n",frameCnt);
            frameCnt++;
            pNode->used_flag = 1;
            pkt.stream_index = audio_st->index;
            ret = av_write_frame(pFormatCtx, &pkt);
            av_free_packet(&pkt);
            //update_AVFrameList(psourceAList,pNode->used_flag,mtx); ɾ��֮����Ƶ�����ò�
        }
    }
    return 0;
}


void AudioEncoder::destroy()
{
    if (audio_st)
    {
        avcodec_close(audio_st->codec);
		av_free(pframe);
    }
    avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);
}
