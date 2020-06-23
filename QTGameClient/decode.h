#pragma once


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

#include <libavutil/avassert.h>
#include "libavcodec/avcodec.h"
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
	
#include <libavformat/avformat.h>

}

#include <QtWidgets/QApplication>

const int INBUF_SIZE = 1000;

int width = 1280, height = 720;

namespace Video {
	class Decoder {
	private:
		FILE* f;
		AVCodecParserContext* parser;
		AVCodecContext* c;
		AVFrame* frame;
		AVPacket* pkt;
		SwsContext* ctx;
		QLabel *qLabel;
	public:
		Decoder(QLabel* qL) : qLabel(qL) {
			this->pkt = av_packet_alloc();

			AVCodec* codec;
			codec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);
			parser = av_parser_init(codec->id);
			this->c = avcodec_alloc_context3(codec);
			avcodec_open2(this->c, codec, NULL);

			this->frame = av_frame_alloc();

			this->ctx = sws_getContext(width, height,
				AV_PIX_FMT_YUV420P, width, height,
				AV_PIX_FMT_RGBA, SWS_BICUBIC, 0, 0, 0);
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
				QImage qImage = QImage(rgb->data[0], this->c->width, this->c->height,
					rgb->linesize[0], QImage::Format_RGBA8888);
				printf("Im\n");
				this->qLabel->setPixmap(QPixmap::fromImage(qImage));

				// _sleep(50);
				// stbi_write_jpg(buf, frame->width, frame->height, 4, rgb->data[0], 4 * frame->width);
				av_frame_free(&rgb);
				av_free(rgbBuffer);
				//pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
			}
		}

		void decode_buffer(uint8_t* inbuf, int data_size) {
			uint8_t* data;
			int ret;
			if (!data_size)
				return;
			data = inbuf;
			while (data_size > 0) {
				ret = av_parser_parse2(parser, this->c, &(this->pkt->data), &(this->pkt->size), data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
				if (ret == 0)
					printf("test");
				data += ret;
				data_size -= ret;
				if (this->pkt->size) {
					decode_frame(this->c, this->ctx, this->frame, this->pkt);
				}
			}
		}

		void decode() {
			std::string filename = /*"mpeg4-1589340867.mkv"*/"C:\\Users\\pavao\\Documents\\erasmus\\Courses\\CloudGaming\\VVE\\GamePlayer\\mpeg4-1590520094.mkv";
			fopen_s(&(this->f), filename.c_str(), "rb");

			uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
			uint8_t* data;
			size_t   data_size;
			int ret;

			memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);


			while (!feof(this->f)) {
				data_size = fread(inbuf, 1, INBUF_SIZE, this->f);
				this->decode_buffer(inbuf, data_size);
			}
			decode_frame(this->c, this->ctx, this->frame, NULL);

			


		}

		~Decoder() {
			fclose(this->f);

			av_parser_close(parser);
			avcodec_free_context(&(this->c));
			av_frame_free(&(this->frame));
			av_packet_free(&(this->pkt));
			sws_freeContext(this->ctx);
			printf("\n");
		}
	};
}