#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <conio.h>
#ifdef	__cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/avassert.h"
#include "libavutil/audio_fifo.h"
#include "libswresample/swresample.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h" 

#ifdef __cplusplus
};
#endif
/* The output bit rate in bit/s */
#define OUTPUT_BIT_RATE 96000
/* The number of output channels */
#define OUTPUT_CHANNELS 2

//formatContext
AVFormatContext* pFormatCtx_Video = NULL, * pFormatCtx_Audio = NULL, * pFormatCtx_Out = NULL;
//in codecContext
AVCodecContext* pCodecCtx_Video;
AVCodecContext* pCodecCtx_Audio;
//out codecContext
AVCodecContext* videoCodecCtx;
AVCodecContext* pOutputCodecCtx;
//buffer
AVFifoBuffer* fifo_video = NULL;
AVAudioFifo* fifo_audio = NULL;
//outstream index
int VideoIndex, AudioIndex;
//lock
CRITICAL_SECTION AudioSection, VideoSection;



SwsContext* img_convert_ctx;
struct SwrContext* au_convert_ctx;
int frame_size = 0;


uint8_t* picture_buf = NULL, * frame_buf = NULL;

bool bCap = true;

DWORD WINAPI ScreenCapThreadProc(LPVOID lpParam);
DWORD WINAPI AudioCapThreadProc(LPVOID lpParam);
static char* dup_wchar_to_utf8(const wchar_t* w)
{
	char* s = NULL;
	int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
	s = (char*)av_malloc(l);
	if (s)
		WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);
	return s;
}
int OpenVideoCapture()
{
	AVInputFormat* ifmt = av_find_input_format("dshow");

	//Set own video device's name
	char* psCameraName = dup_wchar_to_utf8(L"video=USB Video Device");


	AVDictionary* options = NULL;
	av_dict_set(&options, "rtbufsize", "30412800", 0);
	av_dict_set_int(&options, "framerate", 25, 0);
	if (avformat_open_input(&pFormatCtx_Video, psCameraName, ifmt, &options) != 0)
	{
		printf("Couldn't open input stream.（无法打开视频输入流）\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx_Video, NULL) < 0)
	{
		printf("Couldn't find stream information.（无法获取视频流信息）\n");
		return -1;
	}
	if (pFormatCtx_Video->streams[0]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
	{
		printf("Couldn't find video stream information.（无法获取视频流信息）\n");
		return -1;
	}
	//pCodecCtx_Video = pFormatCtx_Video->streams[0]->codec;
	pCodecCtx_Video = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(pCodecCtx_Video, pFormatCtx_Video->streams[0]->codecpar);

	//pCodecCtx_Video->codec_id与pFormatCtx_Video->streams[0]->codecpar->codec_id是否有区别
	const AVCodec* tmpCodec = avcodec_find_decoder(pCodecCtx_Video->codec_id);
	if (tmpCodec == NULL)
	{
		printf("Codec not found.（没有找到解码器）\n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx_Video, tmpCodec, NULL) < 0)
	{
		printf("Could not open codec.（无法打开解码器）\n");
		return -1;
	}

	img_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, pCodecCtx_Video->pix_fmt,
		pCodecCtx_Video->width, pCodecCtx_Video->height, AV_PIX_FMT_YUV422P, SWS_BICUBIC, NULL, NULL, NULL);

	//frame_size = avpicture_get_size(pCodecCtx_Video->pix_fmt, pCodecCtx_Video->width, pCodecCtx_Video->height);
	frame_size = av_image_get_buffer_size(pCodecCtx_Video->pix_fmt, pCodecCtx_Video->width, pCodecCtx_Video->height, 1);
	//申请30帧缓存
	fifo_video = av_fifo_alloc(30 * av_image_get_buffer_size(AV_PIX_FMT_YUV422P, pCodecCtx_Video->width, pCodecCtx_Video->height, 1));

	av_dump_format(pFormatCtx_Video, 0, "video=USB2.0 PC CAMERA", 0);
	return 0;
}

int OpenAudioCapture()
{
	//查找输入方式
	AVInputFormat* pAudioInputFmt = av_find_input_format("dshow");

	//以Direct Show的方式打开设备，并将 输入方式 关联到格式上下文
	//char * psDevName = dup_wchar_to_utf8(L"audio=麦克风 (Realtek High Definition Au");
	char* psDevName = dup_wchar_to_utf8(L"audio=麦克风 (USB Audio Device)");
	//char * psDevName = dup_wchar_to_utf8(L"audio=Line 1 (Virtual Audio Cable)");

	if (avformat_open_input(&pFormatCtx_Audio, psDevName, pAudioInputFmt, NULL) < 0)
	{
		printf("Couldn't open input stream.（无法打开音频输入流）\n");
		return -1;
	}

	if (avformat_find_stream_info(pFormatCtx_Audio, NULL) < 0)
		return -1;

	if (pFormatCtx_Audio->streams[0]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
	{
		printf("Couldn't find video stream information.（无法获取音频流信息）\n");
		return -1;
	}
	pCodecCtx_Audio = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(pCodecCtx_Audio, pFormatCtx_Audio->streams[0]->codecpar);
	//pCodecCtx_Audio->codec_id与pFormatCtx_Audio->streams[0]->codecpar->codec_id是否有区别
	const AVCodec* tmpCodec = avcodec_find_decoder(pFormatCtx_Audio->streams[0]->codecpar->codec_id);
	if (0 > avcodec_open2(pCodecCtx_Audio, tmpCodec, NULL))
	{
		printf("can not find or open audio decoder!\n");
	}
	//Swr

	av_dump_format(pFormatCtx_Audio, 0, "audio=mic (Realtek High Definition Au", 0);

	return 0;
}

int OpenOutPut()
{
	AVStream* pVideoStream = NULL, * pAudioStream = NULL;
	const char* outFileName = "rtmp://192.168.56.101/videotest";
	//const char *outFileName = "d:/test222.flv";
	avformat_alloc_output_context2(&pFormatCtx_Out, NULL, "flv", outFileName);

	//const char *outFileName = "d:/test.flv";
	//avformat_alloc_output_context2(&pFormatCtx_Out, NULL, NULL, outFileName);

	if (pFormatCtx_Video->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
	{
		/*
		VideoIndex = 0;
		pVideoStream = avformat_new_stream(pFormatCtx_Out, NULL);
		if (!pVideoStream)
		{
			printf("can not new stream for output!\n");
			return -1;
		}
		pVideoStream = pFormatCtx_Out->streams[0];
		AVCodec *tmpCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
		videoCodecCtx = avcodec_alloc_context3(tmpCodec);
		avcodec_parameters_to_context(videoCodecCtx, pVideoStream->codecpar);
		//avcodec_parameters_copy(pVideoStream->codecpar, pFormatCtx_Video->streams[0]->codecpar);
		//set codec context param
		//videoCodecCtx->pix_fmt = tmpCodec->pix_fmts[0];
		videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV422P;
		videoCodecCtx->height = pFormatCtx_Video->streams[0]->codecpar->height;
		videoCodecCtx->width = pFormatCtx_Video->streams[0]->codecpar->width;
		videoCodecCtx->sample_aspect_ratio = pCodecCtx_Video->sample_aspect_ratio;
		videoCodecCtx->time_base.num = 1;
		videoCodecCtx->time_base.den = 25;
		videoCodecCtx->bit_rate = 400000;
		videoCodecCtx->gop_size = 250;
		if (pFormatCtx_Out->oformat->flags & AVFMT_GLOBALHEADER)
			videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		videoCodecCtx->qmin = 10;
		videoCodecCtx->qmax = 51;
		videoCodecCtx->max_b_frames = 3;

		AVDictionary *param = NULL;
		av_dict_set(&param, "preset", "ultrafast", 0);
		//解码时花屏，加上有花屏，去掉有延时
		av_dict_set(&param, "tune", "zerolatency", 0);
		//av_dict_set(&param, "rtbufsize", 3041280 , 0);
		if ((avcodec_open2(videoCodecCtx, tmpCodec,&param)) < 0)
		{
			printf("can not open the encoder\n");
			return -1;
		}
		avcodec_parameters_from_context(pVideoStream->codecpar, videoCodecCtx);

		//pVideoStream->time_base.num = 1;
		//pVideoStream->time_base.den = 25;
		//video_st->codec = oCodecCtx;
		*/
		//set codec context param
		//const AVCodec* tmpCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
		const AVCodec* tmpCodec = avcodec_find_encoder_by_name("libx264");
		if (!tmpCodec) {
			printf("can not find encoder !\n");
			return -1;
		}
		videoCodecCtx = avcodec_alloc_context3(tmpCodec);
		if (!videoCodecCtx) {
			printf("can not alloc context!\n");
			return -1;
		}
		// take first format from list of supported formats
		videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV422P;
		videoCodecCtx->height = pFormatCtx_Video->streams[0]->codecpar->height;
		videoCodecCtx->width = pFormatCtx_Video->streams[0]->codecpar->width;
		videoCodecCtx->sample_aspect_ratio = pCodecCtx_Video->sample_aspect_ratio; // 采样宽高比：像素宽/像素高
		videoCodecCtx->time_base.num = 1;
		videoCodecCtx->time_base.den = 25;
		videoCodecCtx->framerate = pCodecCtx_Video->framerate;
		videoCodecCtx->bit_rate = 400000;
		videoCodecCtx->gop_size = 25;

		if (pFormatCtx_Out->oformat->flags & AVFMT_GLOBALHEADER)
			videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		videoCodecCtx->qmin = 10;
		videoCodecCtx->qmax = 51;
		videoCodecCtx->max_b_frames = 0;

		//Optional Param  
		AVDictionary* param = 0;
		//av_dict_set(&param, "preset", "ultrafast", 0);
		//解码时花屏，加上有花屏，去掉有延时
		//av_dict_set(&param, "tune", "zerolatency", 0);
		//av_dict_set_int(&param, "rtbufsize", 30412800 , 0);
		//av_dict_set_int(&param, "framerate", 15, 0);
		if (avcodec_open2(videoCodecCtx, tmpCodec, &param) < 0) {
			printf("Failed to open encoder! (编码器打开失败！)\n");
			return -1;
		}
		pVideoStream = avformat_new_stream(pFormatCtx_Out, tmpCodec);

		if (!pVideoStream)
		{
			printf("can not new stream for output!\n");
			return -1;
		}
		pVideoStream->time_base.num = 1;
		pVideoStream->time_base.den = 25;
		//video_st->codec = oCodecCtx;
		avcodec_parameters_from_context(pVideoStream->codecpar, videoCodecCtx);

	}

	if (pFormatCtx_Audio->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
	{

		AudioIndex = 1;


		pAudioStream = avformat_new_stream(pFormatCtx_Out, NULL);
		//pAudioStream->codec->codec = avcodec_find_encoder(pFormatCtx_Out->oformat->audio_codec);
		//pAudioStream->codec->codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
		//const AVCodec* tmpCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
		//AVCodec *tmpCodec = avcodec_find_encoder(pFormatCtx_Out->oformat->audio_codec);
		const AVCodec *tmpCodec = avcodec_find_encoder_by_name("libfdk_aac");
		//AVCodec *tmpCodec = avcodec_find_encoder_by_name("libvo_aacenc");
		if (!tmpCodec)
		{
			printf("can not new stream for output!\n");
			return -1;
		}
		pOutputCodecCtx = avcodec_alloc_context3(NULL);
		if (!pOutputCodecCtx) {
			printf("can not alloc context!\n");
			return -1;
		}

		pOutputCodecCtx->channels = OUTPUT_CHANNELS;
		pOutputCodecCtx->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
		pOutputCodecCtx->sample_rate = pCodecCtx_Audio->sample_rate;
		//pOutputCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16P;
		pOutputCodecCtx->sample_fmt = tmpCodec->sample_fmts[0];
		pOutputCodecCtx->bit_rate = OUTPUT_BIT_RATE;

		pOutputCodecCtx->profile = FF_PROFILE_AAC_LD;
		pOutputCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

		AVRational time_base = { 1, pOutputCodecCtx->sample_rate };
		//pAudioStream->time_base = time_base;
		pOutputCodecCtx->time_base = time_base;
		//pOutputCodecCtx->codec_tag = 0;
		if (pFormatCtx_Out->oformat->flags & AVFMT_GLOBALHEADER)
			pOutputCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		if (avcodec_open2(pOutputCodecCtx, tmpCodec, 0) < 0)
		{
			//编码器打开失败，退出程序
			return -1;
		}
		//pAudioStream->codecpar->codec_tag = 0;		
		avcodec_parameters_from_context(pAudioStream->codecpar, pOutputCodecCtx);
	}

	if (!(pFormatCtx_Out->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&pFormatCtx_Out->pb, outFileName, AVIO_FLAG_WRITE) < 0)
		{
			printf("can not open output file handle!\n");
			return -1;
		}
	}
	//Show some information
	av_dump_format(pFormatCtx_Out, 0, outFileName, 1);
	if (avformat_write_header(pFormatCtx_Out, NULL) < 0)
	{
		printf("can not write the header of the output file!\n");
		return -1;
	}

	return 0;
}

static int init_resampler(AVCodecContext* input_codec_context,
	AVCodecContext* output_codec_context,
	SwrContext** resample_context)
{
	int error;

	/*
	* Create a resampler context for the conversion.
	* Set the conversion parameters.
	* Default channel layouts based on the number of channels
	* are assumed for simplicity (they are sometimes not detected
	* properly by the demuxer and/or decoder).
	*/
	*resample_context = swr_alloc_set_opts(NULL,
		av_get_default_channel_layout(output_codec_context->channels),
		output_codec_context->sample_fmt,
		output_codec_context->sample_rate,
		av_get_default_channel_layout(input_codec_context->channels),
		input_codec_context->sample_fmt,
		input_codec_context->sample_rate,
		0, NULL);
	if (!*resample_context) {
		fprintf(stderr, "Could not allocate resample context\n");
		return AVERROR(ENOMEM);
	}
	/*
	* Perform a sanity check so that the number of converted samples is
	* not greater than the number of samples to be converted.
	* If the sample rates differ, this case has to be handled differently
	*/
	av_assert0(output_codec_context->sample_rate == input_codec_context->sample_rate);

	/* Open the resampler with the specified parameters. */
	if ((error = swr_init(*resample_context)) < 0) {
		fprintf(stderr, "Could not open resample context\n");
		swr_free(resample_context);
		return error;
	}
	return 0;
}

int main()
{
	//	av_register_all();
	avdevice_register_all();
	avformat_network_init();
	if (OpenVideoCapture() < 0)
	{
		return -1;
	}
	if (OpenAudioCapture() < 0)
	{
		return -1;
	}
	if (OpenOutPut() < 0)
	{
		return -1;
	}

	InitializeCriticalSection(&VideoSection);
	InitializeCriticalSection(&AudioSection);

	AVFrame* picture = av_frame_alloc();
	//int size = avpicture_get_size(pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt,
	//	pFormatCtx_Out->streams[VideoIndex]->codec->width, pFormatCtx_Out->streams[VideoIndex]->codec->height);
	//the width height in codecContext and stream's codecpar maybe different?
	int size = av_image_get_buffer_size(AV_PIX_FMT_YUV422P,
		pFormatCtx_Out->streams[VideoIndex]->codecpar->width, pFormatCtx_Out->streams[VideoIndex]->codecpar->height, 1);
	picture_buf = new uint8_t[size];
	//avpicture_fill((AVPicture *)picture, picture_buf,
	//	videoCodecCtx->pix_fmt,
	//	pFormatCtx_Out->streams[VideoIndex]->codecpar->width,
	//	pFormatCtx_Out->streams[VideoIndex]->codecpar->height);
	av_image_fill_arrays(picture->data, picture->linesize, picture_buf,
		videoCodecCtx->pix_fmt,
		pFormatCtx_Out->streams[VideoIndex]->codecpar->width,
		pFormatCtx_Out->streams[VideoIndex]->codecpar->height, 1);


	/* 初始化音频转换 from:transcode_aac.c */
	if (init_resampler(pCodecCtx_Audio, pOutputCodecCtx,
		&au_convert_ctx))
	{
		return -1;
	}




	//star cap screen thread
	CreateThread(NULL, 0, ScreenCapThreadProc, 0, 0, NULL);
	//star cap audio thread
	CreateThread(NULL, 0, AudioCapThreadProc, 0, 0, NULL);
	int64_t cur_pts_v = 0, cur_pts_a = 0;
	int64_t VideoFrameIndex = 0, AudioFrameIndex = 0;
	int64_t start_time = av_gettime();

	while (1)
	{
		if (_kbhit() != 0 && bCap)
		{
			bCap = false;
			Sleep(2000);//简单的用sleep等待采集线程关闭
		}
		if (fifo_audio && fifo_video)
		{
			int sizeAudio = av_audio_fifo_size(fifo_audio);
			int sizeVideo = av_fifo_size(fifo_video);
			//缓存数据写完就结束循环
			if (sizeAudio <= pFormatCtx_Out->streams[AudioIndex]->codecpar->frame_size &&
				sizeVideo <= frame_size && !bCap)
			{
				break;
			}
		}

		if (av_compare_ts(cur_pts_v, pFormatCtx_Out->streams[VideoIndex]->time_base,
			cur_pts_a, pFormatCtx_Out->streams[AudioIndex]->time_base) <= 0)
		{
			printf("write video!!!!!!!!!!!!!!!!!!!,pts %lld, index %lld\n", cur_pts_v, VideoFrameIndex);
			//read data from fifo
			if (av_fifo_size(fifo_video) < frame_size && !bCap)
			{
				cur_pts_v = 0x7fffffffffffffff;
			}
			if (av_fifo_size(fifo_video) >= size)
			{
				EnterCriticalSection(&VideoSection);
				av_fifo_generic_read(fifo_video, picture_buf, size, NULL);
				LeaveCriticalSection(&VideoSection);

				//avpicture_fill((AVPicture *)picture, picture_buf,
				//	pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt,
				//	pFormatCtx_Out->streams[VideoIndex]->codec->width,
				//	pFormatCtx_Out->streams[VideoIndex]->codec->height);
				av_image_fill_arrays(picture->data, picture->linesize, picture_buf,
					videoCodecCtx->pix_fmt,
					pFormatCtx_Out->streams[VideoIndex]->codecpar->width,
					pFormatCtx_Out->streams[VideoIndex]->codecpar->height, 1);


				//pts = n * (（1 / timbase）/ fps);
				picture->pts = VideoFrameIndex * AV_TIME_BASE / av_q2d(pFormatCtx_Video->streams[0]->r_frame_rate);
				//picture->pts = VideoFrameIndex * ((pFormatCtx_Video->streams[0]->time_base.den / pFormatCtx_Video->streams[0]->time_base.num) / 15);

				picture->format = videoCodecCtx->pix_fmt;
				picture->width = pFormatCtx_Out->streams[VideoIndex]->codecpar->width;
				picture->height = pFormatCtx_Out->streams[VideoIndex]->codecpar->height;

				int got_picture = 0;
				AVPacket *pkt;
				pkt = av_packet_alloc();
				//av_init_packet(&pkt);
				//pkt->data = NULL;
				//pkt->size = 0;

				//int ret = avcodec_encode_video2(pFormatCtx_Out->streams[VideoIndex]->codec, &pkt, picture, &got_picture);
				int ret = avcodec_send_frame(videoCodecCtx, picture);

				if (ret < 0)
				{
					//编码错误,不理会此帧
					printf("vidoe encode fail!!\n");
					continue;
				}
				got_picture = avcodec_receive_packet(videoCodecCtx, pkt);
				if (got_picture == 0)
				{

					int cal_duration = AV_TIME_BASE / av_q2d(pFormatCtx_Video->streams[0]->r_frame_rate);
					AVRational time_base_q = { 1,AV_TIME_BASE };
					pkt->stream_index = VideoIndex;
					pkt->pts = av_rescale_q_rnd(picture->pts, time_base_q,
						pFormatCtx_Out->streams[VideoIndex]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
					pkt->dts = av_rescale_q_rnd(pkt->dts, time_base_q,
						pFormatCtx_Out->streams[VideoIndex]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)); ;
					pkt->duration = av_rescale_q_rnd(cal_duration, time_base_q,
						pFormatCtx_Out->streams[VideoIndex]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));;

					AVRational time_base = pFormatCtx_Video->streams[0]->time_base;
					//AVRational time_base_q = { 1,AV_TIME_BASE };
					int64_t pts_time = av_rescale_q(pkt->pts, time_base, time_base_q);
					int64_t now_time = av_gettime() - start_time;
					if (pts_time > now_time)
						av_usleep(pts_time - now_time);


					cur_pts_v = pkt->pts;

					ret = av_interleaved_write_frame(pFormatCtx_Out, pkt);
					//delete[] pkt->data;
					av_packet_unref(pkt);
					VideoFrameIndex++;
				}
				else
				{
					printf("video got_picture fail!!\n");
				}

			}
		}
		else
		{
			printf("write Audio, pts= %lld !\n", cur_pts_a);
			if (NULL == fifo_audio)
			{
				continue;//还未初始化fifo
			}
			if (av_audio_fifo_size(fifo_audio) < pFormatCtx_Out->streams[AudioIndex]->codecpar->frame_size && !bCap)
			{
				cur_pts_a = 0x7fffffffffffffff;
			}

			int frame_sizet = (pFormatCtx_Out->streams[AudioIndex]->codecpar->frame_size > 0 ? pFormatCtx_Out->streams[AudioIndex]->codecpar->frame_size : 1024);
			if (av_audio_fifo_size(fifo_audio) >= frame_sizet)
			{
				AVFrame* frame;
				frame = av_frame_alloc();
				frame->nb_samples = pFormatCtx_Out->streams[AudioIndex]->codecpar->frame_size > 0 ? pFormatCtx_Out->streams[AudioIndex]->codecpar->frame_size : 1024;
				frame->channel_layout = pFormatCtx_Out->streams[AudioIndex]->codecpar->channel_layout;
				frame->format = pOutputCodecCtx->sample_fmt;
				frame->sample_rate = pFormatCtx_Out->streams[AudioIndex]->codecpar->sample_rate;

				av_frame_get_buffer(frame, 0);

				EnterCriticalSection(&AudioSection);
				av_audio_fifo_read(fifo_audio, (void**)frame->data,
					frame_sizet);
				LeaveCriticalSection(&AudioSection);

				//int audiofifosize = av_audio_fifo_size(fifo_audio);
				//printf("%d\n",audiofifosize);
				int audio_framesize = frame->nb_samples;

				AVPacket* pkt_out;
				pkt_out = av_packet_alloc();
				int got_picture = -1;

				frame->pts = AudioFrameIndex * pFormatCtx_Out->streams[AudioIndex]->codecpar->frame_size;

				//if (avcodec_encode_audio2(pFormatCtx_Out->streams[AudioIndex]->codec, &pkt_out, frame, &got_picture) < 0)
				//{
				//	printf("can not decoder a frame");
				//}
				int ret = avcodec_send_frame(pOutputCodecCtx, frame);
				if (ret < 0)
				{
					printf("can not decoder a frame");
				}
				got_picture = avcodec_receive_packet(pOutputCodecCtx, pkt_out);
				av_frame_free(&frame);
				if (got_picture == 0)
				{

					pkt_out->stream_index = AudioIndex;
					//pkt_out->pts = AudioFrameIndex * pFormatCtx_Out->streams[AudioIndex]->codec->frame_size;
					pkt_out->pts = av_rescale_q_rnd(AudioFrameIndex * audio_framesize, pOutputCodecCtx->time_base,
						pFormatCtx_Out->streams[AudioIndex]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
					pkt_out->dts = pkt_out->pts;
					pkt_out->duration = av_rescale_q_rnd(audio_framesize, pOutputCodecCtx->time_base,
						pFormatCtx_Out->streams[AudioIndex]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
					cur_pts_a = pkt_out->pts;

					AVRational time_base = pOutputCodecCtx->time_base;
					AVRational time_base_q = { 1,AV_TIME_BASE };
					int64_t pts_time = av_rescale_q(pkt_out->dts, time_base, time_base_q);
					int64_t now_time = av_gettime() - start_time;
					//printf("audio now is %lld, pts is %lld\n", now_time, pts_time);
					if (pts_time > now_time)
						av_usleep(pts_time - now_time);

					int ret = av_interleaved_write_frame(pFormatCtx_Out, pkt_out);
					av_packet_unref(pkt_out);

				}
				AudioFrameIndex++;
			}
		}
	}

	delete[] picture_buf;

	av_fifo_free(fifo_video);
	av_audio_fifo_free(fifo_audio);

	av_write_trailer(pFormatCtx_Out);

	avio_close(pFormatCtx_Out->pb);
	avformat_free_context(pFormatCtx_Out);

	if (pFormatCtx_Video != NULL)
	{
		avformat_close_input(&pFormatCtx_Video);
		pFormatCtx_Video = NULL;
	}
	if (pFormatCtx_Audio != NULL)
	{
		avformat_close_input(&pFormatCtx_Audio);
		pFormatCtx_Audio = NULL;
	}

	return 0;
}

DWORD WINAPI ScreenCapThreadProc(LPVOID lpParam)
{
	AVPacket* packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	int got_picture;
	AVFrame* pFrame;
	pFrame = av_frame_alloc();

	AVFrame* picture = av_frame_alloc();
	picture->format = videoCodecCtx->pix_fmt;
	picture->width = videoCodecCtx->width;
	picture->height = videoCodecCtx->height;

	//int size = avpicture_get_size(pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt,
	//	pFormatCtx_Out->streams[VideoIndex]->codec->width, pFormatCtx_Out->streams[VideoIndex]->codec->height);
	int size = av_image_get_buffer_size(videoCodecCtx->pix_fmt,
		pFormatCtx_Out->streams[VideoIndex]->codecpar->width, pFormatCtx_Out->streams[VideoIndex]->codecpar->height, 1);

	picture_buf = new uint8_t[size];

	//avpicture_fill((AVPicture *)picture, picture_buf,
	//	pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt,
	//	pFormatCtx_Out->streams[VideoIndex]->codec->width,
	//	pFormatCtx_Out->streams[VideoIndex]->codec->height);
	av_image_fill_arrays(picture->data, picture->linesize, picture_buf,
		videoCodecCtx->pix_fmt,
		pFormatCtx_Out->streams[VideoIndex]->codecpar->width,
		pFormatCtx_Out->streams[VideoIndex]->codecpar->height, 1);


	int height = pFormatCtx_Out->streams[VideoIndex]->codecpar->height;
	int width = pFormatCtx_Out->streams[VideoIndex]->codecpar->width;
	int y_size = height * width;
	while (bCap)
	{
		//av_init_packet(packet);
		packet->data = NULL;
		packet->size = 0;
		if (av_read_frame(pFormatCtx_Video, packet) < 0)
		{
			continue;
		}
		if (packet->stream_index == 0)
		{
			int ret = avcodec_send_packet(pCodecCtx_Video, packet);
			if (ret < 0)
			{
				printf("Decode Error.（解码错误）\n");
				continue;
			}
			got_picture = avcodec_receive_frame(pCodecCtx_Video, pFrame);

			if (got_picture == 0)
			{
				int ret = sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0,
					pFormatCtx_Out->streams[VideoIndex]->codecpar->height, picture->data, picture->linesize);

				if (av_fifo_space(fifo_video) >= size)
				{
					EnterCriticalSection(&VideoSection);
					av_fifo_generic_write(fifo_video, picture->data[0], y_size, NULL);
					av_fifo_generic_write(fifo_video, picture->data[1], y_size / 2, NULL);
					av_fifo_generic_write(fifo_video, picture->data[2], y_size / 2, NULL);
					LeaveCriticalSection(&VideoSection);
					//int data_size = av_fifo_space(fifo_video);
					//printf("vedio fifo size:%d \n", data_size);
				}
			}
		}
		av_packet_unref(packet);
		//Sleep(50);
	}
	av_frame_free(&pFrame);
	av_frame_free(&picture);
	//delete[] picture_buf;
	return 0;
}

DWORD WINAPI AudioCapThreadProc(LPVOID lpParam)
{
	AVPacket* pkt;
	AVFrame* frame;
	pkt = av_packet_alloc();
	frame = av_frame_alloc();
	int gotframe;
	while (bCap)
	{
		if (av_read_frame(pFormatCtx_Audio, pkt) < 0)
		{
			continue;
		}
		//if (avcodec_decode_audio4(pFormatCtx_Audio->streams[0]->codec, frame, &gotframe, &pkt) < 0)
		//{
		//	av_frame_free(&frame);
		//	printf("can not decoder a frame");
		//	break;
		//}
		int ret = avcodec_send_packet(pCodecCtx_Audio, pkt);
		if (ret < 0)
		{
			av_frame_free(&frame);
			printf("can not decoder a frame");
			break;
		}
		gotframe = avcodec_receive_frame(pCodecCtx_Audio, frame);

		av_packet_unref(pkt);

		if (gotframe != 0)
		{
			continue;//没有获取到数据，继续下一次
		}
		if (NULL == fifo_audio)
		{
			fifo_audio = av_audio_fifo_alloc(pOutputCodecCtx->sample_fmt,
				pFormatCtx_Audio->streams[0]->codecpar->channels, 300 * frame->nb_samples);
		}

		if (pOutputCodecCtx->sample_fmt != pCodecCtx_Audio->sample_fmt
			|| pFormatCtx_Out->streams[AudioIndex]->codecpar->channels != pFormatCtx_Audio->streams[0]->codecpar->channels
			|| pFormatCtx_Out->streams[AudioIndex]->codecpar->sample_rate != pFormatCtx_Audio->streams[0]->codecpar->sample_rate)
		{
			if (frame->channels > 0 && frame->channel_layout == 0)
				frame->channel_layout = av_get_default_channel_layout(frame->channels);
			else if (frame->channels == 0 && frame->channel_layout > 0)
				frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);




			int64_t dst_nb_samples = av_rescale_rnd(
				swr_get_delay(au_convert_ctx, frame->sample_rate) + frame->nb_samples,
				frame->sample_rate,
				frame->sample_rate,
				AV_ROUND_UP);

			//int upperBoundSamples = swr_get_out_samples(au_convert_ctx, frame->nb_samples * frame->channels);
		    //short* pBuffer = new short[upperBoundSamples];

				uint8_t** converted_samples;
			converted_samples = (uint8_t**)calloc(pFormatCtx_Out->streams[AudioIndex]->codecpar->channels,
				sizeof(converted_samples));
			av_samples_alloc(converted_samples, NULL,
				pFormatCtx_Out->streams[AudioIndex]->codecpar->channels,
				frame->nb_samples,
				pOutputCodecCtx->sample_fmt, 0);
			//swr_convert(au_convert_ctx, pFrameMP3->data, pFrameMP3->nb_samples, (const uint8_t**)m_ain, pFrame_audio->nb_samples);
			int nb = swr_convert(au_convert_ctx,
				converted_samples, dst_nb_samples,
				(const uint8_t**)frame->extended_data, frame->nb_samples);


			int data_size = av_audio_fifo_size(fifo_audio);
			printf("audio fifo size:%d \n", data_size);
			EnterCriticalSection(&AudioSection);
			av_audio_fifo_realloc(fifo_audio, av_audio_fifo_size(fifo_audio) + dst_nb_samples);
			av_audio_fifo_write(fifo_audio, (void**)converted_samples, dst_nb_samples);
			LeaveCriticalSection(&AudioSection);

			av_freep(&converted_samples[0]);
		}
		else
		{
			int buf_space = av_audio_fifo_space(fifo_audio);
			if (av_audio_fifo_space(fifo_audio) >= frame->nb_samples)
			{
				EnterCriticalSection(&AudioSection);
				av_audio_fifo_write(fifo_audio, (void**)frame->data, frame->nb_samples);
				LeaveCriticalSection(&AudioSection);
			}
		}


	}
	av_frame_free(&frame);
	return 0;
}