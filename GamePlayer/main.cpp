#include <winsock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)


#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <ctime>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

extern "C" {

#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"

#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavutil/common.h>
#include <libavutil/samplefmt.h>
#include "main.h"

}

typedef struct MyPackage {
	int id;
	int current;
	int total;
	int len;
	char data[1400];
} MyPackage;

#define BUFLEN 1600
#define INBUF_SIZE 1000

char endcode[] = { 0, 0, 1, 0xb7 };


const char* host = "127.0.0.1";
short port = 5005;

int width = 1280;
int height = 720;


void loadFromGame() {
	SOCKET s;
	struct sockaddr_in server, si_other;
	int slen, recv_len;
	char buf[BUFLEN];
	WSADATA wsa;

	slen = sizeof(si_other);

	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed.Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//Create a socket
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
	}
	printf("Socket created.\n ");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	//Bind
	if (bind(s, (struct sockaddr*) & server, sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Bind done.");


	std::map<int, std::map<int, std::vector<char>>> packages;

	//keep listening for data
	while (1)
	{

		//clear the buffer by filling null, it might have previously received data
		memset(buf, 0, BUFLEN);

		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr*) & si_other, &slen)) == SOCKET_ERROR)
		{
			printf("recvfrom() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}

		//print details of the client/peer and the data received
		//printf("Data: % s\n ", buf);
		MyPackage* package = (MyPackage*)buf;
		for (int i = 0; i < std::fmin(package->len, 20); i++) {
			printf("%d", package->data[i]);
		}printf("\n");

		if (package->len == 4) {
			bool same = true;
			for (int i = 0; i < 4; i++) {
				if (package->data[i] != endcode[i]) {
					same = false;
					break;
				}
			}
			if (same) {
				break;
			}
		}
		for (int i = 0; i < package->len; i++) {
			packages[package->id][package->current].push_back(package->data[i]);
		}
	}

	closesocket(s);
	WSACleanup();

	std::string filename = std::string("mpeg4-" + std::to_string(time(NULL)) + ".mkv");
	std::FILE* f;
	fopen_s(&f, filename.c_str(), "wb");

	for (auto package : packages) {
		int id = package.first;
		std::map<int, std::vector<char>> packets = package.second;
		for (auto packet : packets) {
			fwrite(packet.second.data(), 1, packet.second.size(), f);
		}
	}

	fclose(f);
}

static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize, char* filename) {
	FILE * f;
	int i;
	fopen_s(&f, filename, "w");
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}


void decode_frame(AVCodecContext* dec_ctx, SwsContext* ctx, AVFrame* frame, AVPacket* pkt) {
	char buf[1024];
	int ret;
	ret = avcodec_send_packet(dec_ctx, pkt);
	while (ret >= 0) {

		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		snprintf(buf, sizeof(buf), "test%02d.jpg", dec_ctx->frame_number);
		AVFrame* rgb = av_frame_alloc();
		int rgbBufferSize = avpicture_get_size(AV_PIX_FMT_RGBA, frame->width, frame->height);
		uint8_t* rgbBuffer = (uint8_t*)av_malloc(rgbBufferSize * sizeof(uint8_t));
		avpicture_fill((AVPicture*)rgb, rgbBuffer, AV_PIX_FMT_RGBA, frame->width, frame->height);
		sws_scale(ctx, frame->data, frame->linesize, 0, frame->height, rgb->data, rgb->linesize);
		stbi_write_jpg(buf, frame->width, frame->height, 4, rgb->data[0], 4 * frame->width);
		av_frame_free(&rgb);
		av_free(rgbBuffer);
		//pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
	}
}

void decode() {
	std::string filename = /*"mpeg4-1589340867.mkv"*/"mpeg4-1590520094.mkv";
	AVCodec* codec;
	AVCodecParserContext* parser;
	AVCodecContext* c = NULL;
	int frame_count;
	FILE* f;
	AVFrame* frame;
	uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	AVPacket* pkt;
	uint8_t* data;
	size_t   data_size;
	int ret;

	pkt = av_packet_alloc();
	memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	codec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);
	parser = av_parser_init(codec->id);
	c = avcodec_alloc_context3(codec);
	avcodec_open2(c, codec, NULL);
	fopen_s(&f, filename.c_str(), "rb");
	frame = av_frame_alloc();

	SwsContext* ctx = sws_getContext(width, height,
		AV_PIX_FMT_YUV420P, width, height,
		AV_PIX_FMT_RGBA, SWS_BICUBIC, 0, 0, 0);

	frame_count = 0;
	while (!feof(f)) {
		data_size = fread(inbuf, 1, INBUF_SIZE, f);
		if (!data_size)
			break;
		data = inbuf;
		while (data_size > 0) {
			ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size, data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
			if (ret == 0)
				printf("test");
			data += ret;
			data_size -= ret;
			if (pkt->size) {
				decode_frame(c,ctx,frame, pkt);
			}
		}
	}
	decode_frame(c, ctx,frame, NULL);

	fclose(f);

	av_parser_close(parser);
	avcodec_free_context(&c);
	av_frame_free(&frame);
	av_packet_free(&pkt);
	printf("\n");


}

int main() {
	decode();
	//loadFromGame();
	return 0;
}

