#include <stdlib.h>
#include <string.h>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
};
#include "audioplayer.h"

static  uint8_t  *audio_chunk; 
static  uint32_t  audio_len; 
static  uint8_t  *audio_pos; 


int decode_audio(char* no_use)
{
	AVFormatContext	*pFormatCtx;
	int				i, audioStream;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;

	char url[300]={0};
	strcpy(url,no_use);

	av_register_all();
	//支持网络流输入
	avformat_network_init();
	//初始化
	pFormatCtx = avformat_alloc_context();
	//打开
	if(avformat_open_input(&pFormatCtx,url,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx,0)<0)
	{
		printf("Couldn't find stream information.\n");
		return -1;
	}
	// Dump valid information onto standard error
	av_dump_format(pFormatCtx, 0, url, false);

	// Find the first audio stream
	audioStream=-1;
	for(i=0; i < pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
		{
			audioStream=i;
			break;
		}

	if(audioStream==-1)
	{
		printf("Didn't find a audio stream.\n");
		return -1;
	}

	// Get a pointer to the codec context for the audio stream
	pCodecCtx=pFormatCtx->streams[audioStream]->codec;

	// Find the decoder for the audio stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}

	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
	{
		printf("Could not open codec.\n");
		return -1;
	}

	FILE *pFile;
	pFile=fopen("tdjm.pcm", "wb");

	AVPacket *packet=(AVPacket *)malloc(sizeof(AVPacket));
	av_init_packet(packet);

	AVFrame	*pFrame;
	pFrame = av_frame_alloc();
	//输出音频数据大小，一定小于输出内存。
	int out_linesize;
	//输出内存大小
	int out_buffer_size=av_samples_get_buffer_size(&out_linesize, pCodecCtx->channels,pCodecCtx->frame_size,pCodecCtx->sample_fmt, 1);
	uint8_t *out_buffer=new uint8_t[out_buffer_size];

	uint32_t ret,len = 0;
	int got_picture;
	int index = 0;
	struct SwrContext *au_convert_ctx;
	au_convert_ctx = swr_alloc();
	au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
		pCodecCtx->channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);
	swr_init(au_convert_ctx);
	while(av_read_frame(pFormatCtx, packet)>=0)
	{
		if(packet->stream_index==audioStream)
		{

			ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, packet);
			if ( ret < 0 ) 
			{
                printf("Error in decoding audio frame.\n");
                exit(0);
            }
			if ( got_picture > 0 )
			{
				swr_convert(au_convert_ctx,&out_buffer, out_linesize,(const uint8_t **)pFrame->data , pFrame->nb_samples);
				fwrite(out_buffer, 1, out_linesize, pFile);
				index++;
			}
		}
		av_free_packet(packet);
	}

	fclose(pFile);
	// Close the codec
	avcodec_close(pCodecCtx);
	// Close the video file
	//av_close_input_file(pFormatCtx);

	return 0;
}


