﻿


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
}
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


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

int main()
{
	printf("ok：%d\n", avcodec_version());
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