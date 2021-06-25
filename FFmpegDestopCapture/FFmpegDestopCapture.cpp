extern "C" {
#include "libavcodec/avcodec.h" 
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libavutil/imgutils.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/time.h"
#include "libavutil/mathematics.h"
#include "libavutil/channel_layout.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "SDL.h"
}
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string>
#include <locale>
#include <stdio.h>
#include "aacenc_lib.h"
#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>



wchar_t* ANSIToUnicode(const char* str)
{
	int textlen;
	wchar_t* result;
	textlen = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
	result = (wchar_t*)malloc((textlen + 1) * sizeof(wchar_t));
	memset(result, 0, (textlen + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, str, -1, (LPWSTR)result, textlen);
	return result;
}

char* UnicodeToANSI(const wchar_t* str)
{
	char* result;
	int textlen;
	textlen = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
	result = (char*)malloc((textlen + 1) * sizeof(char));
	memset(result, 0, sizeof(char) * (textlen + 1));
	WideCharToMultiByte(CP_ACP, 0, str, -1, result, textlen, NULL, NULL);
	return result;
}

wchar_t* UTF8ToUnicode(const char* str)
{
	int textlen;
	wchar_t* result;
	textlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	result = (wchar_t*)malloc((textlen + 1) * sizeof(wchar_t));
	memset(result, 0, (textlen + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, str, -1, (LPWSTR)result, textlen);
	return result;
}

char* UnicodeToUTF8(const wchar_t* str)
{
	char* result;
	int textlen;
	textlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	result = (char*)malloc((textlen + 1) * sizeof(char));
	memset(result, 0, sizeof(char) * (textlen + 1));
	WideCharToMultiByte(CP_UTF8, 0, str, -1, result, textlen, NULL, NULL);
	return result;
}
/*宽字符转换为多字符Unicode - ANSI*/
char* w2m(const wchar_t* wcs)
{
	int len;
	char* buf;
	len = wcstombs(NULL, wcs, 0);
	if (len == 0)
		return NULL;
	buf = (char*)malloc(sizeof(char) * (len + 1));
	memset(buf, 0, sizeof(char) * (len + 1));
	len = wcstombs(buf, wcs, len + 1);
	return buf;
}
/*多字符转换为宽字符ANSI - Unicode*/
wchar_t* m2w(const char* mbs)
{
	int len;
	wchar_t* buf;
	len = mbstowcs(NULL, mbs, 0);
	if (len == 0)
		return NULL;
	buf = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
	memset(buf, 0, sizeof(wchar_t) * (len + 1));
	len = mbstowcs(buf, mbs, len + 1);
	return buf;
}

char* ANSIToUTF8(const char* str)
{
	return UnicodeToUTF8(ANSIToUnicode(str));
}


char* dup_wchar_to_utf8(wchar_t* w)

{
	char* s = NULL;

	int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);

	s = (char*)av_malloc(l);

	if (s)

		WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);

	return s;

}


static int pcm2aac()
{
	HANDLE_AACENCODER aacEncHandle;

	int out_size = 2048;
	unsigned char aac_buf[2048];
	memset(aac_buf, 0, out_size);

	unsigned char pcm_data[2048];

	int channel = 1;
	/* 0x01 for AAC module */

	if (aacEncOpen(&aacEncHandle, 0, channel) != AACENC_OK)
	{
		return -1;
	}

	/* 设置为AAC LC模式 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_AOT, 2) != AACENC_OK)
	{
		return -1;
	}

	/* 设置采样率 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_SAMPLERATE, 8000) != AACENC_OK)
	{
		return -1;
	}

	/* 左右声道 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_CHANNELMODE, MODE_1) != AACENC_OK)
	{
		return -1;
	}

	/* ADTS输出 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_TRANSMUX, TT_MP4_ADTS) != AACENC_OK)
	{
		return -1;
	}

	if (aacEncoder_SetParam(aacEncHandle, AACENC_BITRATE, 38000) != AACENC_OK)
	{
		return -1;
	}
	//if(aacEncoder_SetParam(aacEncHandle, AACENC_CHANNELORDER, 0) != AACENC_OK )
	//{
	//    return -1;
	//}

	/* 使用NULL来初始化aacEncHandle实例 */
	if (aacEncEncode(aacEncHandle, NULL, NULL, NULL, NULL) != AACENC_OK)
	{
		return -1;
	}

	AACENC_InfoStruct info = { 0 };
	if (aacEncInfo(aacEncHandle, &info) != AACENC_OK)
	{
		return -1;
	}


	int input_size = channel * 2 * info.frameLength;

	char* input_buf = (char*)malloc(input_size);
	char* convert_buf = (char*)malloc(input_size);

	printf("------ input_size[%d] -----\n", input_size);

	int in_elem_size = 2; /* 16位采样位数时一个样本2 bytes */
	int in_identifier = IN_AUDIO_DATA;

	int pcm_buf_size = 0;
	void* in_ptr = convert_buf;
	//inargs.numInSamples = pcm_buf_size / 2 ;  /* 16bit 采样位数的样本数计算方法 */

	int out_identifier = OUT_BITSTREAM_DATA;
	void* out_ptr = aac_buf;
	int out_elem_size = 1;

	FILE* rfd = fopen("test16bitsingle.pcm", "rb+");
	if (!rfd)
	{
		return -1;
	}

	FILE* wfd = fopen("test16bitsingle.aac", "wb+");
	if (!wfd)
	{
		return -1;
	}

	int i = 0;

	printf("Begin to cnvert .......\n");
	//struct timeval adts_start, adts_end;
	//gettimeofday(&adts_start, NULL);

	while (!feof(rfd))
	{
		AACENC_BufDesc outBufDesc = { 0 };
		AACENC_BufDesc inBufDesc = { 0 };
		AACENC_InArgs  inargs = { 0 };
		AACENC_OutArgs  outargs = { 0 };

		memset(input_buf, 0, input_size);
		memset(aac_buf, 0, out_size);

		/* 如果输入不是nput_size(4096)byte，则会凑足4096byte才会有输出 */
		int ret = fread(input_buf, 1, 2048, rfd);

		/* 此处用的是16bit采样位数双通道的PCM数据，故将左右声道的低位及高位合并。详见PCM数据格式可知 */
		//for (i = 0; i < ret / 2; i++)
		//{
		//	const char* in = &input_buf[2 * i];
		//	convert_buf[i] = in[0] | (in[1] << 8);
		//}
		for (i = 0; i < ret; i++)
		{
			convert_buf[i] = input_buf[i];
		}

		inargs.numInSamples = ret;

		inBufDesc.numBufs = 1;
		inBufDesc.bufs = &in_ptr;
		inBufDesc.bufSizes = &ret;
		inBufDesc.bufElSizes = &in_elem_size;
		inBufDesc.bufferIdentifiers = &in_identifier;

		outBufDesc.numBufs = 1;
		outBufDesc.bufs = &out_ptr;
		outBufDesc.bufSizes = &out_size;
		outBufDesc.bufElSizes = &out_elem_size;
		outBufDesc.bufferIdentifiers = &out_identifier;

		int aac_ret = aacEncEncode(aacEncHandle, &inBufDesc, &outBufDesc, &inargs, &outargs);
		if (aac_ret != AACENC_OK)
		{
			//printf("######### Convert continue aac_ret[%x]#########\n", aac_ret);
			continue;
		}

		if (outargs.numOutBytes > 0)
		{
			/* success output */
			fwrite(out_ptr, 1, outargs.numOutBytes, wfd);
			//printf("######### Convert success #########\n");
		}
		//printf("######### Converting outargs.numOutBytes[%d] #########\n", outargs.numOutBytes);
	}
	//gettimeofday(&adts_end, NULL);
	//long adts_cost_ms = (adts_end.tv_sec - adts_start.tv_sec) * 1000 * 1000 + (adts_end.tv_usec - adts_start.tv_usec);
	//printf("----- cost time [%ld] us ------\n", adts_cost_ms);

	fclose(rfd);
	fclose(wfd);

	aacEncClose(&aacEncHandle);
	printf("############## Success ##############\n");

	return 0;
}

static int get_pcm_from_mic() {
	int ret = 0;
	char errors[1024] = { 0, };

	//ctx
	AVFormatContext* fmt_ctx = NULL;
	AVDictionary* options = NULL;


	av_dict_set(&options, "sample_size", "16", 0);
	av_dict_set(&options, "channels", "1", 0);
	av_dict_set(&options, "sample_rate", "44100", 0);
	av_dict_set(&options, "frame_rate", "25", 0);
	

	const char* out = "./audio.pcm";
	FILE* outfile = fopen(out, "wb+");
	//get format
	const AVInputFormat* iformat = av_find_input_format("dshow");
	const char* name = "audio=麦克风 (USB Audio Device)";
	char* device_name = ANSIToUTF8(name);


	if ((ret = avformat_open_input(&fmt_ctx, device_name, iformat, &options)) < 0)
	{
		av_strerror(ret, errors, 1024);
		fprintf(stderr, "Failed to open audio device, [%d]%s\n", ret, errors);
		return NULL;
	}


	HANDLE_AACENCODER aacEncHandle;

	int out_size = 2048;
	unsigned char aac_buf[2048];
	memset(aac_buf, 0, out_size);

	unsigned char pcm_data[2048];

	int channel = 1;
	/* 0x01 for AAC module */

	if (aacEncOpen(&aacEncHandle, 0, channel) != AACENC_OK)
	{
		return -1;
	}

	/* 设置为AAC LC模式 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_AOT, 2) != AACENC_OK)
	{
		return -1;
	}

	/* 设置采样率 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_SAMPLERATE, 44100) != AACENC_OK)
	{
		return -1;
	}

	/* 左右声道 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_CHANNELMODE, MODE_1) != AACENC_OK)
	{
		return -1;
	}

	/* ADTS输出 */
	if (aacEncoder_SetParam(aacEncHandle, AACENC_TRANSMUX, TT_MP4_ADTS) != AACENC_OK)
	{
		return -1;
	}

	if (aacEncoder_SetParam(aacEncHandle, AACENC_BITRATE, 38000) != AACENC_OK)
	{
		return -1;
	}
	//if(aacEncoder_SetParam(aacEncHandle, AACENC_CHANNELORDER, 0) != AACENC_OK )
	//{
	//    return -1;
	//}

	/* 使用NULL来初始化aacEncHandle实例 */
	if (aacEncEncode(aacEncHandle, NULL, NULL, NULL, NULL) != AACENC_OK)
	{
		return -1;
	}

	AACENC_InfoStruct info = { 0 };
	if (aacEncInfo(aacEncHandle, &info) != AACENC_OK)
	{
		return -1;
	}


	int input_size = channel * 2 * info.frameLength;

	char* input_buf = (char*)malloc(input_size);
	char* convert_buf = (char*)malloc(input_size);

	printf("------ input_size[%d] -----\n", input_size);

	int in_elem_size = 2; /* 16位采样位数时一个样本2 bytes */
	int in_identifier = IN_AUDIO_DATA;

	int pcm_buf_size = 0;
	void* in_ptr = convert_buf;
	//inargs.numInSamples = pcm_buf_size / 2 ;  /* 16bit 采样位数的样本数计算方法 */

	int out_identifier = OUT_BITSTREAM_DATA;
	void* out_ptr = aac_buf;
	int out_elem_size = 1;



	FILE* wfd = fopen("test16bitsingle.aac", "wb+");
	if (!wfd)
	{
		return -1;
	}



	AVPacket pkt;
	//av_init_packet(&pkt);
	int count = 0;
	AVAudioFifo* avFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16, 1, 1);
	//read data form audio
	while (ret = (av_read_frame(fmt_ctx, &pkt)) == 0 && count++ < 10) {
		av_log(NULL, AV_LOG_INFO, "pkt size is %d, count=%d\n",
			pkt.size, count);
		fwrite(pkt.data, 1, pkt.size, outfile);
		fflush(outfile);
		//fifo编码
		int result;
		result = av_audio_fifo_realloc(avFifo, 2 * pkt.size);
		printf("av_audio_fifo_realloc result = %d\n", result);
		int space = av_audio_fifo_space(avFifo);
		printf("space = %d\n", space);
		result = av_audio_fifo_write(avFifo, (void**)(&(pkt.data)), pkt.size/2);
		av_packet_unref(&pkt);//release pkt
		printf("av_audio_fifo_write result = %d\n", result);
		int size = av_audio_fifo_size(avFifo);
		space = av_audio_fifo_space(avFifo);
		int offset = 0;
		while (av_audio_fifo_size(avFifo) >= 1024)
		{
			AACENC_BufDesc outBufDesc = { 0 };
			AACENC_BufDesc inBufDesc = { 0 };
			AACENC_InArgs  inargs = { 0 };
			AACENC_OutArgs  outargs = { 0 };

			memset(input_buf, 0, input_size);
			memset(aac_buf, 0, out_size);
			int ret = av_audio_fifo_read(avFifo, (void**)(&convert_buf), 1024);
		    ret = ret * 2;

			//int nowSize = av_audio_fifo_size(avFifo);
			//for (int i = 0; i < 2048; i++)
			//{
			//	convert_buf[i] = input_buf[i];
			//}

			pkt.size -= 2048;
			offset = offset + 2048;
			printf("offset %d. size= %d\n", offset, pkt.size);
			inargs.numInSamples = ret;

			inBufDesc.numBufs = 1;
			inBufDesc.bufs = &in_ptr;
			inBufDesc.bufSizes = &ret;
			inBufDesc.bufElSizes = &in_elem_size;
			inBufDesc.bufferIdentifiers = &in_identifier;

			outBufDesc.numBufs = 1;
			outBufDesc.bufs = &out_ptr;
			outBufDesc.bufSizes = &out_size;
			outBufDesc.bufElSizes = &out_elem_size;
			outBufDesc.bufferIdentifiers = &out_identifier;

			int aac_ret = aacEncEncode(aacEncHandle, &inBufDesc, &outBufDesc, &inargs, &outargs);
			if (aac_ret != AACENC_OK)
			{
				//printf("######### Convert continue aac_ret[%x]#########\n", aac_ret);
				continue;
			}

			if (outargs.numOutBytes > 0)
			{
				/* success output */
				fwrite(out_ptr, 1, outargs.numOutBytes, wfd);
				//printf("######### Convert success #########\n");
			}
		}
	}
	av_packet_unref(&pkt);//release pkt
	fclose(wfd);
	fclose(outfile);
	avformat_close_input(&fmt_ctx);//releas ctx


	printf("OK OK\n");
}

//参考：https://blog.csdn.net/guoyunfei123/article/details/106177347
static int recordMp4() {
	AVFormatContext* ifmtCtx = NULL;
	AVFormatContext* ofmtCtx = NULL;
	AVPacket pkt;
	AVFrame* pFrame, * pFrameYUV;
	SwsContext* pImgConvertCtx;
	AVDictionary* params = NULL;
	const AVCodec* pCodec;
	AVCodecContext* pCodecCtx;
	unsigned char* outBuffer;
	AVCodecContext* pH264CodecCtx;
	const AVCodec* pH264Codec;
	AVDictionary* captureOptions = NULL;

	ULONGLONG lastTime;

	int ret = 0;
	unsigned int i = 0;
	int videoIndex = -1;
	int frameIndex = 0;

	const char* inFilename = "video=USB Video Device";//输入URL
	const char* outFilename = "output.mp4"; //输出URL
	const char* ofmtName = NULL;

	avdevice_register_all();
	avformat_network_init();

	const AVInputFormat* ifmt = av_find_input_format("dshow");

	if (!ifmt)
	{
		printf("can't find input device\n");
		goto end;
	}

	// 1. 打开输入
	// 1.1 打开输入文件，获取封装格式相关信息
	ifmtCtx = avformat_alloc_context();
	if (!ifmtCtx)
	{
		printf("can't alloc AVFormatContext\n");
		goto end;
	}

	av_dict_set_int(&captureOptions, "rtbufsize", 18432000, 0);
	av_dict_set_int(&captureOptions, "framerate", 25, 0);
	av_dict_set(&captureOptions, "video_size", "640x360", 0);
	if ((ret = avformat_open_input(&ifmtCtx, inFilename, ifmt, &captureOptions)) < 0)
	{
		printf("can't open input file: %s\n", inFilename);
		goto end;
	}

	// 1.2 解码一段数据，获取流相关信息
	if ((ret = avformat_find_stream_info(ifmtCtx, 0)) < 0)
	{
		printf("failed to retrieve input stream information\n");
		goto end;
	}

	// 1.3 获取输入ctx
	for (i = 0; i < ifmtCtx->nb_streams; ++i)
	{
		if (ifmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoIndex = i;
			break;
		}
	}

	printf("%s:%d, videoIndex = %d\n", __FUNCTION__, __LINE__, videoIndex);

	av_dump_format(ifmtCtx, 0, inFilename, 0);

	// 1.4 查找输入解码器
	pCodec = avcodec_find_decoder(ifmtCtx->streams[videoIndex]->codecpar->codec_id);
	if (!pCodec)
	{
		printf("can't find codec\n");
		goto end;
	}

	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (!pCodecCtx)
	{
		printf("can't alloc codec context\n");
		goto end;
	}

	avcodec_parameters_to_context(pCodecCtx, ifmtCtx->streams[videoIndex]->codecpar);

	//  1.5 打开输入解码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("can't open codec\n");
		goto end;
	}

	pH264Codec = avcodec_find_encoder(AV_CODEC_ID_H264);

	if (!pH264Codec)
	{
		printf("can't find h264 codec.\n");
		goto end;
	}

	pH264CodecCtx = avcodec_alloc_context3(pH264Codec);
	pH264CodecCtx->codec_id = AV_CODEC_ID_H264;
	pH264CodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	pH264CodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pH264CodecCtx->width = pCodecCtx->width;
	pH264CodecCtx->height = pCodecCtx->height;
	pH264CodecCtx->time_base.num = 1;
	pH264CodecCtx->time_base.den = 25;
	pH264CodecCtx->bit_rate = 400000;
	pH264CodecCtx->gop_size = 250;
	pH264CodecCtx->qmin = 10;
	pH264CodecCtx->qmax = 51;

	pH264CodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;


	av_dict_set(&params, "preset", "superfast", 0);
	av_dict_set(&params, "tune", "zerolatency", 0);

	if (avcodec_open2(pH264CodecCtx, pH264Codec, &params) < 0)
	{
		printf("can't open video encoder.\n");
		goto end;
	}

	avformat_alloc_output_context2(&ofmtCtx, NULL, NULL, outFilename);
	if (!ofmtCtx)
	{
		printf("can't create output context\n");
		goto end;
	}

	//创建输出流
	for (int i = 0; i < ifmtCtx->nb_streams; i++)
	{
		AVStream* outStream = avformat_new_stream(ofmtCtx, pH264Codec);
		if (!outStream)
		{
			printf("failed to allocate output stream\n");
			goto end;
		}

		avcodec_parameters_from_context(outStream->codecpar, pH264CodecCtx);
	}

	av_dump_format(ofmtCtx, 0, outFilename, 1);


	if (!(ofmtCtx->oformat->flags & AVFMT_NOFILE))
	{
		// 2.3 创建并初始化一个AVIOContext, 用以访问URL（outFilename）指定的资源
		ret = avio_open(&ofmtCtx->pb, outFilename, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			printf("can't open output URL: %s\n", outFilename);
			goto end;
		}
	}

	// 3. 数据处理
	// 3.1 写输出文件
	ret = avformat_write_header(ofmtCtx, NULL);
	if (ret < 0)
	{
		printf("Error accourred when opening output file\n");
		goto end;
	}


	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();

	outBuffer = (unsigned char*)av_malloc(
		av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,
			pCodecCtx->height, 1));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, outBuffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

	pImgConvertCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
		pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
		AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	lastTime = GetTickCount64();
	while (frameIndex < 200)
	{
		// 3.2 从输入流读取一个packet
			
		ret = av_read_frame(ifmtCtx, &pkt);

		ULONGLONG curTime = GetTickCount64();
		ULONGLONG diff = curTime - lastTime;
		printf("spend %lld\n", diff);
		lastTime = curTime;

		if (ret < 0)
		{
			break;
		}

		if (pkt.stream_index == videoIndex)
		{
			ret = avcodec_send_packet(pCodecCtx, &pkt);
			if (ret < 0)
			{
				printf("Decode error.\n");
				goto end;
			}

			if (avcodec_receive_frame(pCodecCtx, pFrame) >= 0)
			{
				sws_scale(pImgConvertCtx,
					(const unsigned char* const*)pFrame->data,
					pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data,
					pFrameYUV->linesize);


				pFrameYUV->format = AV_PIX_FMT_YUV420P;
				pFrameYUV->width = pCodecCtx->width;
				pFrameYUV->height = pCodecCtx->height;

				ret = avcodec_send_frame(pH264CodecCtx, pFrameYUV);
				if (ret < 0)
				{
					printf("failed to encode.\n");
					goto end;
				}

				if (avcodec_receive_packet(pH264CodecCtx, &pkt) >= 0)
				{
					// 设置输出DTS,PTS
					pkt.pts = pkt.dts = frameIndex * (ofmtCtx->streams[0]->time_base.den) / ofmtCtx->streams[0]->time_base.num / 25;
					frameIndex++;

					ret = av_interleaved_write_frame(ofmtCtx, &pkt);
					if (ret < 0)
					{
						printf("send packet failed: %d\n", ret);
					}
					else
					{
						printf("send %5d packet successfully!\n", frameIndex);
					}
				}
			}
		}

		av_packet_unref(&pkt);
	}

	av_write_trailer(ofmtCtx);

end:
	avformat_close_input(&ifmtCtx);

	/* close output */
	if (ofmtCtx && !(ofmtCtx->oformat->flags & AVFMT_NOFILE)) {
		avio_closep(&ofmtCtx->pb);
	}
	avformat_free_context(ofmtCtx);

	if (ret < 0 && ret != AVERROR_EOF) {
		printf("Error occurred\n");
		return -1;
	}

	return 0;

}



static int sdl_play() {
	int ret = -1;
	const char* file = "test.mp4";

	AVFormatContext* pFormatCtx = NULL;
	int i, videoStream;
	AVCodecParameters* pCodecParameters = NULL;
	AVCodecContext* pCodecCtx = NULL;
	const AVCodec* pCodec = NULL;
	AVFrame* pFrame = NULL;
	AVFrame* nvFrame = NULL;
	AVPacket packet;

	SDL_Rect rect;
	Uint32 pixformat;
	SDL_Window* win = NULL;
	SDL_Renderer* renderer = NULL;
	SDL_Texture* texture = NULL;
	struct SwsContext* swsContext = NULL;
	AVPixelFormat targetFormat = AV_PIX_FMT_RGB24;

	int imageSize = 0;

	uint8_t* buf = NULL;
	//默认窗口大小
	int w_width = 640;
	int w_height = 480;

	
	//SDL初始化
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL - %s\n", SDL_GetError());
		return ret;
	}

	AVDictionary* options = NULL;
	av_dict_set(&options, "list_devices", "true", 0);
	const AVInputFormat* ifmt = av_find_input_format("dshow");
	pFormatCtx = avformat_alloc_context();

	//av_dict_set(&options, "framerate", "15", 0);//帧lu
	if (avformat_open_input(&pFormatCtx, "video=Logitech HD Webcam C270", ifmt, NULL) != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open  video file!");
		goto __FAIL;
	}
	
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't avformat_find_stream_info!");
		goto __FAIL;
	}
	//找到视频流
	videoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (videoStream == -1) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Din't find a video stream!");
		goto __FAIL;// Didn't find a video stream
	}

	// 流参数
	pCodecParameters = pFormatCtx->streams[videoStream]->codecpar;

	//获取解码器
	pCodec = avcodec_find_decoder(pCodecParameters->codec_id);
	if (pCodec == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported codec!\n");
		goto __FAIL; // Codec not found
	}

	// 初始化一个编解码上下文
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (avcodec_parameters_to_context(pCodecCtx, pCodecParameters) != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't copy codec context");
		goto __FAIL;// Error copying codec context
	}

	// 打开解码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open decoder!\n");
		goto __FAIL; // Could not open codec
	}

	// Allocate video frame
	pFrame = av_frame_alloc();
	nvFrame = av_frame_alloc();


	imageSize = av_image_get_buffer_size(targetFormat, pCodecCtx->width, pCodecCtx->height, 1);

	buf = (uint8_t*)av_malloc(imageSize);
	av_image_fill_arrays(nvFrame->data, nvFrame->linesize, buf, targetFormat, pCodecCtx->width, pCodecCtx->height, 1);

	w_width = pCodecCtx->width;
	w_height = pCodecCtx->height;

	win = SDL_CreateWindow("wenfeng sdl win", 100, 100, 600, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
	////创建窗口
	//win = SDL_CreateWindow("Media Player",
	//	SDL_WINDOWPOS_UNDEFINED,
	//	SDL_WINDOWPOS_UNDEFINED,
	//	w_width, w_height,
	//	SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	//if (!win) {
	//	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window by SDL");
	//	goto __FAIL;
	//}


	//创建渲染器
	swsContext = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, targetFormat, SWS_BICUBIC, NULL, NULL, NULL);

	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
	//renderer = SDL_CreateRenderer(win, -1, 0);
	//if (!renderer) {
	//	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Renderer by SDL");
	//	goto __FAIL;
	//}

	pixformat = SDL_PIXELFORMAT_RGB24;//YUV格式
	// 创建纹理
	texture = SDL_CreateTexture(renderer,
		pixformat,
		SDL_TEXTUREACCESS_STREAMING,
		w_width,
		w_height);


	//读取数据
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		if (packet.stream_index == videoStream) {
			//解码
			avcodec_send_packet(pCodecCtx, &packet);
			while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {

				sws_scale(swsContext, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, nvFrame->data, nvFrame->linesize);
				//uint8_t *buf = (uint8_t*)av_malloc(pCodecCtx->height * pCodecCtx->width + pCodecCtx->height * pCodecCtx->width/2);
				//memcpy(buf, nvFrame->data[0], pCodecCtx->height * pCodecCtx->width);
				//memcpy(buf + pCodecCtx->height * pCodecCtx->width, nvFrame->data[1], pCodecCtx->height* pCodecCtx->width / 2);

				//SDL_UpdateYUVTexture(texture, NULL,
				//	pFrame->data[0], pFrame->linesize[0],
				//	pFrame->data[1], pFrame->linesize[1],
				//	pFrame->data[2], pFrame->linesize[2]);

				//SDL_UpdateYUVTexture(texture, NULL,
				//	pFrame->data[0], pFrame->linesize[0],
				//	nvFrame->data[1], 0,
				//	nvFrame->data[1], 0);

				// Set Size of Window
				rect.x = 0;
				rect.y = 0;
				rect.w = pCodecCtx->width;
				rect.h = pCodecCtx->height;

				SDL_UpdateTexture(texture,
					&rect,
					nvFrame->data[0], nvFrame->linesize[0]);
				//展示
				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, NULL, &rect);
				SDL_RenderPresent(renderer);
			}
		}

		av_packet_unref(&packet);

		// 事件处理
		SDL_Event event;
		SDL_PollEvent(&event);
		if (event.type == SDL_QUIT) {
			goto __QUIT;
		}
		else if (event.type == SDL_KEYDOWN) {
			goto __QUIT;
		}
		else {
			int a = 10;
		}

		
		//SDL_Event event;
		//SDL_PollEvent(&event);
		//switch (event.type) {
		//case SDL_QUIT:
		//	goto __QUIT;
		//default:
		//	break;
		//}


	}

__QUIT:
	ret = 0;

__FAIL:
	// Free the YUV frame
	if (pFrame) {
		av_frame_free(&pFrame);
	}

	// Close the codec
	if (pCodecCtx) {
		avcodec_close(pCodecCtx);
	}

	if (pCodecParameters) {
		avcodec_parameters_free(&pCodecParameters);
	}

	// Close the video file
	if (pFormatCtx) {
//		avformat_close_input(&pFormatCtx);
	}

	if (win) {
		SDL_DestroyWindow(win);
	}

	if (renderer) {
		SDL_DestroyRenderer(renderer);
	}

	if (texture) {
		SDL_DestroyTexture(texture);
	}

	SDL_Quit();

	return ret;
}

#define MAX_AUDIO_FRAME_SIZE 192000 //48khz 16bit audio 2 channels

int pcm_resample() {

	const char* in_file = "16k.wav";

	AVFormatContext* fctx = NULL;
	AVCodecContext* cctx = NULL;
	const AVCodec* acodec = NULL;

	FILE* audio_dst_file1 = fopen("./before_resample.pcm", "wb");
	FILE* audio_dst_file2 = fopen("./after_resample.pcm", "wb");

	avdevice_register_all();
	avformat_open_input(&fctx, in_file, NULL, NULL);
	avformat_find_stream_info(fctx, NULL);
	//get audio index
	int aidx = av_find_best_stream(fctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	printf("get aidx[%d]!!!\n", aidx);
	//open audio codec
	AVCodecParameters* codecpar = fctx->streams[aidx]->codecpar;
	acodec = avcodec_find_decoder(codecpar->codec_id);
	cctx = avcodec_alloc_context3(acodec);
	avcodec_parameters_to_context(cctx, codecpar);
	avcodec_open2(cctx, acodec, NULL);

	//init resample
	int output_channels = 1;
	int output_rate = 48000;
	int input_channels = cctx->channels;
	int input_rate = cctx->sample_rate;
	AVSampleFormat input_sample_fmt = cctx->sample_fmt;
	AVSampleFormat output_sample_fmt = AV_SAMPLE_FMT_S16;
	printf("channels[%d=>%d],rate[%d=>%d],sample_fmt[%d=>%d]\n",
		input_channels, output_channels, input_rate, output_rate, input_sample_fmt, output_sample_fmt);

	SwrContext* resample_ctx = NULL;
	resample_ctx = swr_alloc_set_opts(resample_ctx, av_get_default_channel_layout(output_channels), output_sample_fmt, output_rate,
		av_get_default_channel_layout(input_channels), input_sample_fmt, input_rate, 0, NULL);
	if (!resample_ctx) {
		printf("av_audio_resample_init fail!!!\n");
		return -1;
	}
	swr_init(resample_ctx);

	AVPacket* pkt = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();
	int size = 0;
	uint8_t* out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE);

	while (av_read_frame(fctx, pkt) == 0) {//DEMUX
		if (pkt->stream_index == aidx) {
			avcodec_send_packet(cctx, pkt);
			while (1) {
				int ret = avcodec_receive_frame(cctx, frame);
				if (ret != 0) {
					break;
				}
				else {
					//before resample
					size = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
					if (frame->data[0] != NULL) {
						fwrite(frame->data[0], 1, size, audio_dst_file1);
					}
					size = size * 3;
					//resample
					memset(out_buffer, 0x00, sizeof(out_buffer));
					int out_samples = swr_convert(resample_ctx, &out_buffer, frame->nb_samples * 3, (const uint8_t**)frame->data, frame->nb_samples);
					if (out_samples > 0) {
						av_samples_get_buffer_size(NULL, output_channels, out_samples, output_sample_fmt, 1);//out_samples*output_channels*av_get_bytes_per_sample(output_sample_fmt);
						fwrite(out_buffer, 1, size, audio_dst_file2);
					}
				}
				av_frame_unref(frame);
			}
		}
		else {
			//printf("not audio frame!!!\n");
			av_packet_unref(pkt);
			continue;
		}
		av_packet_unref(pkt);
	}

	//close
	swr_free(&resample_ctx);
	av_packet_free(&pkt);
	av_frame_free(&frame);
	avcodec_close(cctx);
	avformat_close_input(&fctx);
	av_free(out_buffer);
	fclose(audio_dst_file1);
	fclose(audio_dst_file2);

	return 0;
}


static int video_decode_example(const char* input_filename)
{
	AVFormatContext *avFormatContext = NULL;
	AVInputFormat avInputFormat;
	AVCodecContext *avCodecContext = NULL;
	const AVCodec *avCodec;
	int ret;
	AVCodecParameters* avCodecParameters;

	AVPacket* avPackage;
	AVFrame *avFrame;
	AVFrame *yuvFrame;

	ret = avformat_open_input(&avFormatContext, input_filename, NULL, NULL);
	if (ret != 0)
	{
		printf("avformat_open_input fail\n");
		return -1;
	}

	ret = avformat_find_stream_info(avFormatContext, NULL);
	if (ret < 0)
	{
		printf("avformat_find_stream_info fail\n");
		return -1;
	}

	int videoIndex = av_find_best_stream(avFormatContext,
		AVMEDIA_TYPE_VIDEO,
		-1,
		-1,
		NULL,
		0);

	if (videoIndex < 0) {
		printf("get videoIndex fail\n");
		return -1;
	}
	avCodecParameters = avFormatContext->streams[videoIndex]->codecpar;
	avCodec = avcodec_find_decoder(avCodecParameters->codec_id);

	if (avCodec == NULL){
		printf("avCodec fail\n");
		return -1;
	}

	avCodecContext = avcodec_alloc_context3(avCodec);

	if (avCodecContext == NULL) {
		printf("avCodecContext fail\n");
		return -1;
	}

	ret = avcodec_parameters_to_context(avCodecContext, avCodecParameters);

	if (ret < 0) {
		printf("avcodec_parameters_to_context fail\n");
		return -1;
	}
	avcodec_open2(avCodecContext, avCodec, NULL);


	//avFormatContext->video_codec->name
	avPackage = av_packet_alloc();
	if (avPackage == NULL) {
		printf("avPackage fail\n");
		return -1;
	}

	struct SwsContext* imgCtx = sws_getContext(avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt, avCodecContext->width, avCodecContext->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	avFrame = av_frame_alloc();
	yuvFrame = av_frame_alloc();
	if (avFrame == NULL) {
		printf("avFrame fail\n");
		return -1;
	}
	int bufSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, avCodecContext->width, avCodecContext->height, 1);
	uint8_t* buf = (uint8_t*)av_malloc(bufSize);
	av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, buf, AV_PIX_FMT_YUV420P, avCodecContext->width, avCodecContext->height, 1);
	ret = 0;
	int i = 0;
	FILE* fp_yuv = fopen("test_out.yuv", "wb+");
	while (ret >= 0) {
		ret = av_read_frame(avFormatContext, avPackage);
		if (ret < 0) {
			avcodec_send_packet(avCodecContext, NULL);
		} else {
			if (avPackage->pts == AV_NOPTS_VALUE) {
				avPackage->pts = avPackage->dts = i;
			}
			avcodec_send_packet(avCodecContext, avPackage);
		}
		av_packet_unref(avPackage);
		while (ret >= 0) {
			ret = avcodec_receive_frame(avCodecContext, avFrame);
			if (ret == AVERROR_EOF) {
				return -1;
			} else if (ret == AVERROR(EAGAIN)) {
				ret = 0;
				break;
			} else if (ret < 0) {
				printf("avFrame pts = %lld\n", avFrame->pts);
				return ret;
			}
			printf("avFrame pts = %lld\n", avFrame->pts);
			sws_scale(imgCtx, avFrame->data, avFrame->linesize, 0, avCodecContext->height, yuvFrame->data, yuvFrame->linesize);
			int y_size = avCodecContext->width * avCodecContext->height;
			fwrite(yuvFrame->data[0], 1, y_size, fp_yuv);		// Y 
			fwrite(yuvFrame->data[1], 1, y_size / 4, fp_yuv);	// U
			fwrite(yuvFrame->data[2], 1, y_size / 4, fp_yuv);	// V
			av_frame_unref(avFrame);
		}
		i++;
	}
	fclose(fp_yuv);// 非常重要
	return 1;
}

//win32显示
static void Show(HWND hwnd, unsigned char* rgb, int w, int h, bool fill)
{
	HDC hdc = GetDC(hwnd);//获取当前的显示设备上下文

	RECT rect;
	GetClientRect(hwnd, &rect);
	int cxClient = rect.right;
	int cyClient = rect.bottom;

	if (cxClient <= 0 || cyClient <= 0) {
		return;
	}

	HDC  hdcsource = CreateCompatibleDC(NULL);//创建存放图象的显示缓冲
	HBITMAP bitmap = CreateCompatibleBitmap(hdc, cxClient, cyClient);

	SelectObject(hdcsource, bitmap);    //将位图资源装入显示缓冲

	SetStretchBltMode(hdcsource, COLORONCOLOR);

	BITMAPINFO  bmi;
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = -h;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biClrUsed = 0;
	bmi.bmiHeader.biClrImportant = 0;
	bmi.bmiHeader.biSizeImage = 0;


	if (!fill) {

		int des_x = 0;
		int des_y = 0;
		int des_w = 0;
		int des_h = 0;


		if (1.0 * cxClient / cyClient > 1.0 * w / h) {
			des_h = cyClient;
			des_w = des_h * w / h;
			des_x = (cxClient - des_w) / 2;
			des_y = 0;
		}
		else {
			des_w = cxClient;
			des_h = des_w * h / w;
			des_x = 0;
			des_y = (cyClient - des_h) / 2;
		}


		BitBlt(hdcsource, 0, 0, cxClient, cyClient, hdcsource, 0, 0, SRCCOPY);
		StretchDIBits(hdcsource, des_x, des_y, des_w, des_h, \
			0, 0, w, h, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
		BitBlt(hdc, 0, 0, cxClient, cyClient, hdcsource, 0, 0, SRCCOPY);
	}
	else {
		StretchDIBits(hdcsource, 0, 0, rect.right - rect.left, rect.bottom - rect.top, \
			0, 0, w, h, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);

		BitBlt(hdc, 0, 0, cxClient, cyClient, hdcsource, 0, 0, SRCCOPY);//将图象显示缓冲的内容直接显示到屏幕
	}

	DeleteObject(bitmap);
	DeleteDC(hdcsource);
	ReleaseDC(hwnd, hdc);
}

static AVFrame* alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame* picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 4);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		return NULL;
	}

	return picture;
}

static int test1() {
	AVFormatContext* formatCtx = avformat_alloc_context();
	const AVInputFormat* ifmt = av_find_input_format("gdigrab");//设备类型
	//AVInputFormat *ifmt = av_find_input_format("dshow");//设备类型
	AVDictionary* options = NULL;
	//av_dict_set(&options, "video_size","1920*1080",0);//大小  默认全部
	av_dict_set(&options, "framerate", "15", 0);//帧lu
	if (avformat_open_input(&formatCtx, "desktop", ifmt, &options) != 0) {
		//if (avformat_open_input(&formatCtx, "video=Integrated Camera", ifmt, &options) != 0) {
		printf("open input device fail\n");
		return -1;
	}
	av_dict_free(&options);

	if (avformat_find_stream_info(formatCtx, NULL) < 0) {
		printf("avformat_find_stream_info faill\n");
		return -1;
	}
	if (formatCtx->streams[0]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
		printf("no find stream info\n");
		return -1;
	}
	//查找解密器
	const AVCodec* codec = avcodec_find_decoder(formatCtx->streams[0]->codecpar->codec_id);
	if (codec == NULL) {
		printf("codec not found\n");
		return -1;
	}
	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(ctx, formatCtx->streams[0]->codecpar);
	if (avcodec_open2(ctx, codec, NULL) < 0) {
		printf("codec not open\n");
		return -1;
	}
	printf("pix format is %d\n", formatCtx->streams[0]->codecpar->format);//==AVPixelFormat->AV_PIX_FMT_BGRA 

	AVFrame* frame = alloc_picture((AVPixelFormat)formatCtx->streams[0]->codecpar->format, formatCtx->streams[0]->codecpar->width, formatCtx->streams[0]->codecpar->height);
	//目标
	AVFrame* bgrframe = alloc_picture(AV_PIX_FMT_BGR24, formatCtx->streams[0]->codecpar->width, formatCtx->streams[0]->codecpar->height);;

	//图像转换
	SwsContext* img_convert_ctx = sws_getContext(formatCtx->streams[0]->codecpar->width, formatCtx->streams[0]->codecpar->height, (AVPixelFormat)formatCtx->streams[0]->codecpar->format,
		formatCtx->streams[0]->codecpar->width, formatCtx->streams[0]->codecpar->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

	while (true) {
		AVPacket packet = { 0 };
		//av_init_packet(&packet);
		if (av_read_frame(formatCtx, &packet) >= 0) {
			avcodec_send_packet(ctx, &packet);
			if (avcodec_receive_frame(ctx, frame) < 0) {
				printf("decode error\n");
			}
			else {
				//printf("采集到图片");
				//转换 AV_PIX_FMT_BGRA to AV_PIX_FMT_BGR24
				sws_scale(img_convert_ctx, (const unsigned char* const*)frame->data, frame->linesize, 0, frame->height, bgrframe->data, bgrframe->linesize);
				Show(FindWindow(NULL, L"视频播放"), bgrframe->data[0], bgrframe->width, bgrframe->height, true);
			}
		}
		av_packet_unref(&packet);
	}
	av_frame_free(&frame);
	av_frame_free(&bgrframe);
	avcodec_free_context(&ctx);
	avformat_close_input(&formatCtx);

	return 0;
}

static LRESULT CALLBACK WinProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{

	switch (umsg)
	{
	case WM_KEYDOWN:
	{
		if (wparam == VK_SPACE) {

		}
	}
	break;
	case WM_LBUTTONDOWN:
	{

	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, umsg, wparam, lparam);
}

#undef main
int main()
{
	printf("ok：%d\n", avcodec_version());
	//video_decode_example("nature.h264");
	avdevice_register_all();
	int i = 0;
	if (!i) {
		int cc = 10;
	}
	pcm_resample();
	//sdl_play();
	//get_pcm_from_mic();
	//recordMp4();
	//pcm2aac();
	if (true) {
		return 1;
	}


	HINSTANCE hInstance;
	hInstance = GetModuleHandle(NULL);
	WNDCLASSEX wce = { 0 };
	wce.cbSize = sizeof(wce);
	wce.cbClsExtra = 0;
	wce.cbWndExtra = 0;
	wce.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wce.hCursor = NULL;
	wce.hIcon = NULL;
	wce.hIconSm = NULL;
	wce.hInstance = hInstance;
	wce.lpfnWndProc = WinProc;
	wce.lpszClassName = L"Main";
	wce.lpszMenuName = NULL;
	wce.style = CS_HREDRAW | CS_VREDRAW;
	ATOM nAtom = RegisterClassEx(&wce);
	if (!nAtom)
	{
		MessageBox(NULL, L"RegisterClassEx失败", L"错误", MB_OK);
		return 0;
	}
	const char* szStr = "Main";
	WCHAR wszClassName[256];
	memset(wszClassName, 0, sizeof(wszClassName));
	MultiByteToWideChar(CP_ACP, 0, szStr, strlen(szStr) + 1, wszClassName,
		sizeof(wszClassName) / sizeof(wszClassName[0]));
	HWND hwnd = CreateWindow(wszClassName, L"视频播放", WS_OVERLAPPEDWINDOW, 38, 20, 640, 480, NULL, NULL, hInstance, NULL);
	// 显示窗口  
	ShowWindow(hwnd, SW_SHOW);
	// 更新窗口  
	UpdateWindow(hwnd);


	test1(); //屏幕


	// 消息循环  
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}