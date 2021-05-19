


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


static int sdl_play() {
	int ret = -1;
	const char* file = "test.mp4";

	AVFormatContext* pFormatCtx = NULL;
	int i, videoStream;
	AVCodecParameters* pCodecParameters = NULL;
	AVCodecContext* pCodecCtx = NULL;
	AVCodec* pCodec = NULL;
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


	// 打开输入文件
	if (avformat_open_input(&pFormatCtx, file, NULL, NULL) != 0) {
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

static int video_decode_example(const char* input_filename)
{
	AVFormatContext *avFormatContext = NULL;
	AVInputFormat avInputFormat;
	AVCodecContext *avCodecContext = NULL;
	AVCodec *avCodec;
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
	AVInputFormat* ifmt = av_find_input_format("gdigrab");//设备类型
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
	AVCodec* codec = avcodec_find_decoder(formatCtx->streams[0]->codecpar->codec_id);
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
				printf("采集到图片");
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
	sdl_play();
	if (true) {
		return 1;
	}
	avdevice_register_all();


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