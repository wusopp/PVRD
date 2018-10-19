/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#include "VideoSource.h"

#include "FrameQueue.h"
#include "VideoParser.h"

#include <assert.h>

#include "helper_cuda_drvapi.h"

AVCodecContext	*ppCodecCtx;
AVFormatContext	*ppFormatCtx;
//int				videoindex;

VideoSource::VideoSource()
{
	bThreadExit = false;
	/*bStarted = false;*/
	play_thread_ptr = NULL;
    h264bsfc = NULL;
}

VideoSource::~VideoSource()
{
	stop();
}


bool VideoSource::init(AVFormatContext *pFormatCtx, FrameQueue *pFrameQueue)
{
	assert(0 != pFrameQueue);
	oSourceData_.hVideoParser = 0;
	oSourceData_.pFrameQueue = pFrameQueue;

    ppFormatCtx = pFormatCtx;

	int				i;
	AVCodec			*pCodec;

	AVCodecContext *pCodecCtx = pFormatCtx->streams[0]->codec;

	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL){
		printf("Codec not found.\n");
		return false;
	}

	//Output Info-----------------------------
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, "", 0);
	printf("-------------------------------------------------\n");

	memset(&g_stFormat, 0, sizeof(CUVIDEOFORMAT));

	switch (pCodecCtx->codec_id) {
	case AV_CODEC_ID_H263:
		g_stFormat.codec = cudaVideoCodec_MPEG4;
		break;

	case AV_CODEC_ID_H264:
		g_stFormat.codec = cudaVideoCodec_H264;
		break;

	case AV_CODEC_ID_HEVC:
		g_stFormat.codec = cudaVideoCodec_HEVC;
		break;

	case AV_CODEC_ID_MJPEG:
		g_stFormat.codec = cudaVideoCodec_JPEG;
		break;

	case AV_CODEC_ID_MPEG1VIDEO:
		g_stFormat.codec = cudaVideoCodec_MPEG1;
		break;

	case AV_CODEC_ID_MPEG2VIDEO:
		g_stFormat.codec = cudaVideoCodec_MPEG2;
		break;

	case AV_CODEC_ID_MPEG4:
		g_stFormat.codec = cudaVideoCodec_MPEG4;
		break;

	case AV_CODEC_ID_VP8:
		g_stFormat.codec = cudaVideoCodec_VP8;
		break;

	case AV_CODEC_ID_VP9:
		g_stFormat.codec = cudaVideoCodec_VP9;
		break;

	case AV_CODEC_ID_VC1:
		g_stFormat.codec = cudaVideoCodec_VC1;
		break;
	default:
		return false;
	}

	//这个地方的FFmoeg与cuvid的对应关系不是很确定，不过用这个参数似乎最靠谱
	switch (pCodecCtx->pix_fmt)
	{
	case AV_PIX_FMT_YUV420P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_420;
		break;
	case AV_PIX_FMT_YUV422P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_422;
		break;
	case AV_PIX_FMT_YUV444P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_444;
		break;
	default:
		g_stFormat.chroma_format = cudaVideoChromaFormat_420;
		break;
	}

	//找了好久，总算是找到了FFmpeg中标识场格式和帧格式的标识位
	//场格式是隔行扫描的，需要做去隔行处理
	switch (pCodecCtx->field_order)
	{
	case AV_FIELD_PROGRESSIVE:
	case AV_FIELD_UNKNOWN:
		g_stFormat.progressive_sequence = true;
		break;
	default:
		g_stFormat.progressive_sequence = false;
		break;
	}

	pCodecCtx->thread_safe_callbacks = 1;

	g_stFormat.coded_width = pCodecCtx->coded_width;
	g_stFormat.coded_height = pCodecCtx->coded_height;

	g_stFormat.display_area.right = pCodecCtx->width;
	g_stFormat.display_area.left = 0;
	g_stFormat.display_area.bottom = pCodecCtx->height;
	g_stFormat.display_area.top = 0;

	if (pCodecCtx->codec_id == AV_CODEC_ID_H264 || pCodecCtx->codec_id == AV_CODEC_ID_HEVC) {
		if (pCodecCtx->codec_id == AV_CODEC_ID_H264)
			h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
		else
			h264bsfc = av_bitstream_filter_init("hevc_mp4toannexb");
	}

	return true;
}

CUVIDEOFORMAT VideoSource::format()
{
	return g_stFormat;
}

void VideoSource::getSourceDimensions(unsigned int &width, unsigned int &height)
{
    CUVIDEOFORMAT rCudaVideoFormat=  format();

    width  = rCudaVideoFormat.coded_width;
    height = rCudaVideoFormat.coded_height;
}

void VideoSource::getDisplayDimensions(unsigned int &width, unsigned int &height)
{
    CUVIDEOFORMAT rCudaVideoFormat=  format();

    width  = abs(rCudaVideoFormat.display_area.right  - rCudaVideoFormat.display_area.left);
    height = abs(rCudaVideoFormat.display_area.bottom - rCudaVideoFormat.display_area.top);
}

void VideoSource::getProgressive(bool &progressive)
{
	CUVIDEOFORMAT rCudaVideoFormat = format();
	progressive = rCudaVideoFormat.progressive_sequence ? true : false;
}

void VideoSource::setParser(VideoParser &rVideoParser, CUcontext cuCtx)
{
    oSourceData_.hVideoParser = rVideoParser.hParser_;
	g_oContext = cuCtx;
}

void VideoSource::start()
{
	bThreadExit = TRUE;
	if (play_thread_ptr)
	{
		play_thread_ptr->join();
		play_thread_ptr = NULL;
	}

	LPVOID arg_ = NULL;
	Common::ThreadCallback cb = BIND_MEM_CB(&VideoSource::play_thread, this);
	play_thread_ptr = new Common::CThread(cb, TRUE);
	if (!play_thread_ptr)
	{
		return ;
	}

	bThreadExit = FALSE;

	play_thread_ptr->start(arg_);
}

void VideoSource::stop()
{
	bThreadExit = TRUE;
	if (play_thread_ptr)
	{
		play_thread_ptr->join();
		play_thread_ptr = NULL;
	}
}

//bool VideoSource::isStarted()
//{
//	return bStarted;
//}

int iPkt = 0;

void VideoSource::play_thread(LPVOID lpParam)
{
	AVPacket *avpkt;
	avpkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	CUVIDSOURCEDATAPACKET cupkt;
	int iPkt = 0;
	CUresult oResult;
    ppCodecCtx = ppFormatCtx->streams[0]->codec;
	while (av_read_frame(ppFormatCtx, avpkt) >= 0){
		if (bThreadExit){
			break;
		}
		//bStarted = true;
		if (avpkt->stream_index == 0){

			//cuCtxPushCurrent(g_oContext);

			if (avpkt && avpkt->size) {
 				if (h264bsfc)
				{
					av_bitstream_filter_filter(h264bsfc, ppFormatCtx->streams[0]->codec, NULL, &avpkt->data, &avpkt->size, avpkt->data, avpkt->size, 0);
 				}

				cupkt.payload_size = (unsigned long)avpkt->size;
				cupkt.payload = (const unsigned char*)avpkt->data;

				if (avpkt->pts != AV_NOPTS_VALUE) {
					cupkt.flags = CUVID_PKT_TIMESTAMP;
					if (ppCodecCtx->pkt_timebase.num && ppCodecCtx->pkt_timebase.den){
						AVRational tb;
						tb.num = 1;
						tb.den = AV_TIME_BASE;
						cupkt.timestamp = av_rescale_q(avpkt->pts, ppCodecCtx->pkt_timebase, tb);
					}
					else
						cupkt.timestamp = avpkt->pts;
				}
			}
			else {
				cupkt.flags = CUVID_PKT_ENDOFSTREAM;
			}

			oResult = cuvidParseVideoData(this->oSourceData_.hVideoParser, &cupkt);
			if ((cupkt.flags & CUVID_PKT_ENDOFSTREAM) || (oResult != CUDA_SUCCESS)){
				break;
			}
			iPkt++;
			//printf("Succeed to read avpkt %d !\n", iPkt);
			//checkCudaErrors(cuCtxPopCurrent(NULL));
		}
		av_free_packet(avpkt);
	}

	oSourceData_.pFrameQueue->endDecode();
	//bStarted = false;
}

std::ostream & operator << (std::ostream &rOutputStream, const CUVIDEOFORMAT &rCudaVideoFormat)
{
    rOutputStream << "\tVideoCodec      : ";

    if ((rCudaVideoFormat.codec <= cudaVideoCodec_UYVY) &&
        (rCudaVideoFormat.codec >= cudaVideoCodec_MPEG1) &&
        (rCudaVideoFormat.codec != cudaVideoCodec_NumCodecs))
    {
        rOutputStream << eVideoFormats[rCudaVideoFormat.codec].name << "\n";
    }
    else
    {
        rOutputStream << "unknown\n";
    }

    rOutputStream << "\tFrame rate      : " << rCudaVideoFormat.frame_rate.numerator << "/" << rCudaVideoFormat.frame_rate.denominator;
    rOutputStream << "fps ~ " << rCudaVideoFormat.frame_rate.numerator/static_cast<float>(rCudaVideoFormat.frame_rate.denominator) << "fps\n";
    rOutputStream << "\tSequence format : ";

    if (rCudaVideoFormat.progressive_sequence)
        rOutputStream << "Progressive\n";
    else
        rOutputStream << "Interlaced\n";

    rOutputStream << "\tCoded frame size: [" << rCudaVideoFormat.coded_width << ", " << rCudaVideoFormat.coded_height << "]\n";
    rOutputStream << "\tDisplay area    : [" << rCudaVideoFormat.display_area.left << ", " << rCudaVideoFormat.display_area.top;
    rOutputStream << ", " << rCudaVideoFormat.display_area.right << ", " << rCudaVideoFormat.display_area.bottom << "]\n";
    rOutputStream << "\tChroma format   : ";

    switch (rCudaVideoFormat.chroma_format)
    {
        case cudaVideoChromaFormat_Monochrome:
            rOutputStream << "Monochrome\n";
            break;

        case cudaVideoChromaFormat_420:
            rOutputStream << "4:2:0\n";
            break;

        case cudaVideoChromaFormat_422:
            rOutputStream << "4:2:2\n";
            break;

        case cudaVideoChromaFormat_444:
            rOutputStream << "4:4:4\n";
            break;

        default:
            rOutputStream << "unknown\n";
    }

    rOutputStream << "\tBitrate         : ";

    if (rCudaVideoFormat.bitrate == 0)
        rOutputStream << "unknown\n";
    else
        rOutputStream << rCudaVideoFormat.bitrate/1024 << "kBit/s\n";

    rOutputStream << "\tAspect ratio    : " << rCudaVideoFormat.display_aspect_ratio.x << ":" << rCudaVideoFormat.display_aspect_ratio.y << "\n";

    return rOutputStream;
}
