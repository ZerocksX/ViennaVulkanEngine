#include <winsock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include "VEInclude.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>

#include <cmath>

#include <thread>

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

}

typedef struct MyPackage {
	int id;
	int current;
	int total;
	int len;
	char data[1400];
} MyPackage;

std::mutex mtx;
namespace Socket {

	const int BUFLEN = 1600;
	const short port = 5006;

	typedef struct KeyPackage {
		char key;
		std::chrono::nanoseconds nsSent;
	};


	class UDPReciever {
	private:
		SOCKET s;
		struct sockaddr_in server, si_other;
		int slen, recv_len;
		char buf[BUFLEN];
		WSADATA wsa;
	public:
		UDPReciever() {
			this->slen = sizeof(this->si_other);

			//Initialise winsock
			printf("\nInitialising Winsock...");
			if (WSAStartup(MAKEWORD(2, 2), &(this->wsa)) != 0)
			{
				printf("Failed.Error Code : %d", WSAGetLastError());
				exit(EXIT_FAILURE);
			}
			printf("Initialised.\n");

			//Create a socket
			if ((this->s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
			{
				printf("Could not create socket : %d", WSAGetLastError());
			}
			printf("Socket created.\n ");

			//Prepare the sockaddr_in structure
			this->server.sin_family = AF_INET;
			this->server.sin_addr.s_addr = INADDR_ANY;
			this->server.sin_port = htons(port);

			//Bind
			if (bind(this->s, (struct sockaddr*) & (this->server), sizeof(this->server)) == SOCKET_ERROR)
			{
				printf("Bind failed with error code : %d", WSAGetLastError());
				exit(EXIT_FAILURE);
			}
			printf("Bind done.");

		}

		KeyPackage* recieve() {
			//clear the buffer by filling null, it might have previously received data
			memset(this->buf, 0, BUFLEN);

			//try to receive some data, this is a blocking call
			if ((this->recv_len = recvfrom(this->s, this->buf, BUFLEN, 0, (struct sockaddr*) & (this->si_other), &(this->slen))) == SOCKET_ERROR)
			{
				printf("recvfrom() failed with error code : %d", WSAGetLastError());
				exit(EXIT_FAILURE);
			}
			KeyPackage* package = (KeyPackage*)buf;

			return package;
		}

		~UDPReciever() {
			closesocket(this->s);
			WSACleanup();
		}
	};

	class UDPSender {
	private:
		int iResult;
		WSADATA wsaData;
		SOCKET SendSocket = INVALID_SOCKET;
		sockaddr_in RecvAddr;
	public:
		UDPSender(char* host, short port) {
			iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (iResult != NO_ERROR) {
				wprintf(L"WSAStartup failed with error: %d\n", iResult);
				exit(-1);
			}

			SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (SendSocket == INVALID_SOCKET) {
				wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
				WSACleanup();
				exit(-1);
			}

			RecvAddr.sin_family = AF_INET;
			RecvAddr.sin_port = htons(port);
			RecvAddr.sin_addr.s_addr = inet_addr(host);
		}



		int sendUDP(char* host, short port, char* message, int length, int id) {
			char* msgCpy = (char*)malloc(length);
			memcpy(msgCpy, message, length);


			/* send a message to the server */
			int pos = 0;
			int total = length / 1400;
			int current = 0;
			std::map<int,bool> dones;
			for (int i = 0; i <= total; i++)
				dones.insert(std::make_pair(i, false));
			do
			{
				int messageLength = std::min(1400, length - pos);
				MyPackage package;
				package.id = id;
				package.current = current++;
				package.total = total;
				package.len = messageLength;
				int pkgSize = 16 + messageLength;
				memcpy(&package.data, (char*)(msgCpy + pos), messageLength);
				//char buffer[1600];
				//memcpy(&(buffer), &package, pkgSize);
				std::vector<char> buff;
				for (int i = 0; i < pkgSize; i++) {
					buff.push_back(((char*)(&package))[i]);
				}
				//std::thread sendThread([&, buff] {
					sendto(SendSocket, buff.data(), buff.size(), 0, (struct sockaddr*) & RecvAddr, sizeof(RecvAddr));
					//printf("Sent %d %d/%d\n", ((MyPackage*)buff.data())->id, ((MyPackage*)buff.data())->current, ((MyPackage*)buff.data())->total);
					dones.erase(((MyPackage*)buff.data())->current);
				//});
				//sendThread.detach();

				pos += messageLength;
			} while (pos < length);
			//printf("Sent total %d\n", length);
			while (!dones.empty()) {
				std::this_thread::sleep_for(std::chrono::microseconds(50));
				//printf("Waited 1us\n");
			}
			free(msgCpy);
			return 0;
		}

		~UDPSender() {
			// wprintf(L"Finished sending. Closing socket.\n");
			iResult = closesocket(SendSocket);
			if (iResult == SOCKET_ERROR) {
				wprintf(L"closesocket failed with error: %d\n", WSAGetLastError());
				WSACleanup();
				exit(-1);
			}
			WSACleanup();
		}

	};

}
namespace ve {

	char* host = "127.0.0.1";
	short port = 5005;

	uint8_t endcode[] = { 0, 0, 1, 0xb7 };
	uint32_t g_score = 0;
	uint8_t g_state = 0; // 0 - searching, 1 - picked up
	int imgWidth = 1280;
	int imgHeight = 720;
	int packetsSent = 0;

	static const glm::vec3 g_goalPosition = glm::vec3(-5.0f, 1.3f, 5.0f);
	static const glm::vec3 g_carryPosition = glm::vec3(0.0f, 0.0f, 2.0f);
	static const glm::vec3 g_farPosition = glm::vec3(0.0f, 0.0f, 100.0f);

	static std::default_random_engine e{ 12345 };
	static std::uniform_real_distribution<> d{ -4.0f, 4.0f };

	class EventListenerFrameDump : public VEEventListenerGLFW
	{
	private:
		int framesPassed = 0;
		AVCodecContext* enc_ctx;
		AVFrame* frame;
		AVFormatContext* oc;
		AVOutputFormat* fmt;
		AVStream* st;
		SwsContext* sws_ctx;
		int64_t next_pts = 1;
		const int SKIP_FRAMES = 1;
		std::string filename;
		Socket::UDPSender udpSender;

		
		
		void initializeCodec(std::string encoding, int bit_rate, std::string extension, AVCodecID codec_id)
		{
			int ret;
			this->filename = std::string("media/videos/video-" +std::to_string(bit_rate) + "-" + avcodec_get_name(codec_id) + "-" + std::to_string(time(NULL)) + "." + extension);
			
			AVOutputFormat* fmt;
			AVFormatContext* oc;
			AVCodec *codec;

			const char* url = filename.c_str();

			avformat_alloc_output_context2(&oc, NULL, encoding.c_str(), url);
			if (!oc) {
				printf("Could not deduce output format from file extension: using MPEG.\n");
				avformat_alloc_output_context2(&oc, NULL, "mpeg", filename.c_str());
			}
			if (!oc) {
				fprintf(stderr, "format context not found\n");
				exit(1);
			}
			fmt = oc->oformat;

			AVCodecContext* c;
			int i;
			if (codec_id)
			{
				fmt->video_codec = codec_id;
			}
			/* find the encoder */
			codec = avcodec_find_encoder(fmt->video_codec);
			if (!codec) {
				fprintf(stderr, "Could not find encoder for '%s'\n",
					avcodec_get_name(fmt->video_codec));
				exit(1);
			}

			AVStream* st = avformat_new_stream(oc, NULL);
			if (!st) {
				fprintf(stderr, "Could not allocate stream\n");
				exit(1);
			}

			st->id = oc->nb_streams - 1;

			c = avcodec_alloc_context3(codec);
			if (!c) {
				fprintf(stderr, "Could not alloc an encoding context\n");
				exit(1);
			}

			c->codec_id = fmt->video_codec;
			c->bit_rate = bit_rate;

			// resolution must be a multiple of two
			c->width = imgWidth;
			c->height = imgHeight;

			int fps = 15;
			
			// frames per second
			st->time_base = AVRational{ 1, fps };
			st->avg_frame_rate = AVRational{ fps, 1 };
			c->time_base = st->time_base;
			c->framerate = AVRational{ fps, 1 };
			c->gop_size = 1; // emit one intra frame every 5 frames
			c->pix_fmt = AV_PIX_FMT_YUV420P;

					   	
			c->max_b_frames = 2;

			if (oc->oformat->flags & 0x0040)
				c->flags |= 1<<22;

			/* open the codec */
			ret = avcodec_open2(c, codec, NULL);
			if (ret < 0) {
				fprintf(stderr, "Could not open video codec\n");
				exit(1);
			}
			
			frame = av_frame_alloc();
			if (!frame) {
				fprintf(stderr, "Could not allocate frame data.\n");
				exit(1);
			}
			frame->format = c->pix_fmt;
			frame->width = c->width;
			frame->height = c->height;
			/* allocate the buffers for the frame data */
			ret = av_frame_get_buffer(frame, 32);
			if (ret < 0) {
				fprintf(stderr, "Could not allocate frame data.\n");
				exit(1);
			}

			ret = avcodec_parameters_from_context(st->codecpar, c);
			if (ret < 0) {
				fprintf(stderr, "Could not copy the stream parameters\n");
				exit(1);
			}

			// av_dump_format(oc, 0, filename.c_str(), 1);

			// ret = avio_open(&oc->pb, url, AVIO_FLAG_WRITE);
			ret = avio_open_dyn_buf(&oc->pb);
			if (ret < 0) {
				//fprintf(stderr, "Could not open '%s'\n", filename);
				fprintf(stderr, "Could not open dynamic buffer");
				exit(1);
			}


			ret = avformat_write_header(oc, NULL);
			if (ret < 0) {
				fprintf(stderr, "Error occurred when opening output file");
				exit(1);
			}

			unsigned char* l_strBuffer = NULL;
			int l_iBufferLen = 0;
			l_iBufferLen = avio_close_dyn_buf(oc->pb,(uint8_t**)(&l_strBuffer));
			this->udpSender.sendUDP(host, port, (char*)l_strBuffer, l_iBufferLen,packetsSent++);
			av_free(l_strBuffer);
			

			SwsContext* ctx = sws_getContext(c->width, c->height,
				AV_PIX_FMT_RGBA, c->width, c->height,
				AV_PIX_FMT_YUV420P, 0, 0, 0, 0);

			this->oc = oc;
			this->st = st;
			this->fmt = fmt;
			this->frame = frame;
			this->enc_ctx = c;
			this->sws_ctx = ctx;
		}
		void encode(uint8_t * dataImage)
		{
			int ret;
			AVCodecContext* c;
			AVFrame* frame;
			int got_packet = 0;
			AVPacket pkt = { 0 };
			
			c = this->enc_ctx;

			if (av_frame_make_writable(this->frame) < 0) {
				fprintf(stderr, "Error making the frame writable");
				exit(1);
			}
			this->frame->pts = this->next_pts++;
			frame = this->frame;

			uint8_t* inData[1] = { dataImage }; // RGBA have one plane
			int inLinesize[1] = { 4 * frame->width }; // RGB stride
			sws_scale(this->sws_ctx, inData, inLinesize, 0, frame->height, frame->data, frame->linesize);

			av_init_packet(&pkt);

			/* encode the image */
			ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
			if (ret < 0) {
				fprintf(stderr, "Error encoding video frame");
				exit(1);
			}
			if (got_packet) {
				/* rescale output packet timestamp values from codec to stream timebase */
				av_packet_rescale_ts(&pkt, c->time_base, st->time_base);
				(&pkt)->stream_index = st->index;
				
				/* Write the compressed frame to the media file. */
				

				int ret = avio_open_dyn_buf(&oc->pb);
				if (ret < 0) {
					//fprintf(stderr, "Could not open '%s'\n", filename);
					fprintf(stderr, "Could not open dynamic buffer");
					exit(1);
				}

				ret = av_interleaved_write_frame(oc, &pkt);

				unsigned char* l_strBuffer = NULL;
				int l_iBufferLen = 0;
				l_iBufferLen = avio_close_dyn_buf(oc->pb, (uint8_t**)(&l_strBuffer));
				this->udpSender.sendUDP(host, port, (char*)l_strBuffer, l_iBufferLen, packetsSent++);
				av_free(l_strBuffer);


			}
			else {
				ret = 0;
			}
			if (ret < 0) {
				fprintf(stderr, "Error while writing video frame");
				exit(1);
			}
		}


		void finalizeCodec()
		{
			int ret = avio_open_dyn_buf(&oc->pb);
			if (ret < 0) {
				//fprintf(stderr, "Could not open '%s'\n", filename);
				fprintf(stderr, "Could not open dynamic buffer");
				exit(1);
			}



			av_write_trailer(this->oc);

			unsigned char* l_strBuffer = NULL;
			int l_iBufferLen = 0;
			l_iBufferLen = avio_close_dyn_buf(oc->pb, (uint8_t**)(&l_strBuffer));
			this->udpSender.sendUDP(host, port, (char*)l_strBuffer, l_iBufferLen, packetsSent++);
			av_free(l_strBuffer);


			this->udpSender.sendUDP(host,port, (char*)endcode, sizeof(endcode), packetsSent++);

			Sleep(1000);

			avcodec_free_context(&(this->enc_ctx));
			av_frame_free(&(this->frame));
						
			// avio_closep(&(this->oc->pb));

			/* free the stream */
			avformat_free_context(this->oc);

			sws_freeContext(this->sws_ctx);
		}

	protected:
		virtual void onFrameEnded(veEvent event)
		{
			framesPassed++;
			if (framesPassed > SKIP_FRAMES)
			{
				VkExtent2D extent = getWindowPointer()->getExtent();
				uint32_t imageSize = extent.width * extent.height * 4;
				VkImage image = getRendererPointer()->getSwapChainImage();

				uint8_t* dataImage = new uint8_t[imageSize];

				vh::vhBufCopySwapChainImageToHost(getRendererPointer()->getDevice(),
					getRendererPointer()->getVmaAllocator(),
					getRendererPointer()->getGraphicsQueue(),
					getRendererPointer()->getCommandPool(),
					image, VK_FORMAT_R8G8B8A8_UNORM,
					VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					dataImage, extent.width, extent.height, imageSize);

				m_numScreenshot++;
				
				//auto now = std::chrono::system_clock::now();
				// encode(this->enc_ctx, dataImage, this->pkt, this->outfile);
				encode(dataImage);
				//printf("Endoding in milliseconds: %d\n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - now).count());

				delete[] dataImage;
				framesPassed = 0;
			}

		}
	public:
		///Constructor of class EventListenerCollision
		EventListenerFrameDump(std::string name, int bitRate, std::string encoding, std::string extension, AVCodecID codecId) : VEEventListenerGLFW(name), udpSender(host, port) {
			this->initializeCodec(encoding, bitRate, extension, codecId);
		};

		///Destructor of class EventListenerCollision
		virtual ~EventListenerFrameDump() {
			this->finalizeCodec();
		};
	};

	class EventListenerCollision : public VEEventListener {
	protected:
		virtual void onFrameStarted(veEvent event) {

			VEEntity* goalParent;
			VECHECKPOINTER(goalParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Goal Parent"));
			VEEntity* caseParent;
			VECHECKPOINTER(caseParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Case Parent"));
			VEEntity* carryParent;
			VECHECKPOINTER(carryParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Carry Parent"));
			VEEntity* characterParent;
			VECHECKPOINTER(characterParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Character Parent"));
			VEEntity* opponentParent;
			VECHECKPOINTER(opponentParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Opponent Parent"));
			float boxDistance;
			float goalDistance;
			float opponentDistance;
			switch (g_state)
			{
			case 0: //searching
				opponentDistance = glm::distance(opponentParent->getPosition(), characterParent->getPosition());
				if (opponentDistance < 2.0f) {
					opponentParent->setPosition(glm::vec3(d(e), 1.0f, d(e)));
					g_score++;
				}
				boxDistance = glm::distance(caseParent->getPosition(), characterParent->getPosition());
				if (boxDistance < 1.5f) {
					caseParent->setPosition(g_farPosition);
					carryParent->setPosition(g_carryPosition);
					//goalParent->setPosition(g_goalPosition);
					g_state = 1;
				}
				break;
			case 1: //picked up
				opponentDistance = glm::distance(opponentParent->getPosition(), characterParent->getPosition());
				if (opponentDistance < 2.0f) {
					caseParent->setPosition(glm::vec3(d(e), 1.0f, d(e)));
					characterParent->setPosition(glm::vec3(d(e), characterParent->getPosition().y, d(e)));

					carryParent->setPosition(g_farPosition);
					g_state = 0;
					g_score--;
					break;
				}
				goalDistance = glm::distance(goalParent->getPosition(), characterParent->getPosition());
				if (goalDistance < 3.0f) {
					caseParent->setPosition(glm::vec3(d(e), 1.0f, d(e)));
					carryParent->setPosition(g_farPosition);
					// goalParent->setPosition(g_farPosition);
					g_state = 0;
					g_score++;
				}
				break;

			default:
				break;
			}
		};

	public:
		///Constructor of class EventListenerCollision
		EventListenerCollision(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerCollision() {};
	};

	bool processKeyInput(Socket::KeyPackage* keyPkg) {
		char key = keyPkg->key;
		if (key == 'q') {				// q pressed - end the engine
			getEnginePointer()->end();
			return true;
		}
		
		VEEntity* characterParent;
		VECHECKPOINTER(characterParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Character Parent"));
		VEEntity* opponentParent;
		VECHECKPOINTER(opponentParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Opponent Parent"));


		glm::vec4 rot4C = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
		glm::vec4 rot4O = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
		glm::vec4 dir4C = glm::normalize(characterParent->getTransform() * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
		glm::vec3 dirC = glm::vec3(dir4C.x, dir4C.y, dir4C.z);
		glm::vec4 dir4O = glm::normalize(opponentParent->getTransform() * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
		glm::vec3 dirO = glm::vec3(dir4O.x, dir4O.y, dir4O.z);

		float rotSpeedC = 0.3f;
		float speedC = 1.3f;
		float rotSpeedO = 0.3f;
		float speedO = 1.3f;


		VECamera* pCamera = getSceneManagerPointer()->getCamera();
		VESceneNode* pParent = pCamera->getParent();

		switch (key) {
		case 'a':
			characterParent->setTransform(glm::rotate(characterParent->getTransform(), -rotSpeedC, glm::vec3(rot4C.x, rot4C.y, rot4C.z)));
			break;
		case 'd':
			characterParent->setTransform(glm::rotate(characterParent->getTransform(), rotSpeedC, glm::vec3(rot4C.x, rot4C.y, rot4C.z)));
			break;
		case 'w':
			characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), speedC * dirC));
			break;
		case 's':
			characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), -speedC * dirC));
			break;
		case 'W':
			opponentParent->multiplyTransform(glm::translate(glm::mat4(1.0f), speedO * dirO));
			break;
		case 'S':
			opponentParent->multiplyTransform(glm::translate(glm::mat4(1.0f), -speedO * dirO));
			break;
		case 'A':
			opponentParent->setTransform(glm::rotate(opponentParent->getTransform(), -rotSpeedO, glm::vec3(rot4O.x, rot4O.y, rot4O.z)));
			break;
		case 'D':
			opponentParent->setTransform(glm::rotate(opponentParent->getTransform(), +rotSpeedO, glm::vec3(rot4O.x, rot4O.y, rot4O.z)));
			break;

		default:
			return false;
		};
		return true;
	}

	class EventListenerControls : public VEEventListener {
	protected:
		virtual bool onKeyboard(veEvent event) {
			if (event.idata1 == GLFW_KEY_ESCAPE) {				//ESC pressed - end the engine
				getEnginePointer()->end();
				return true;
			}

			if (event.idata3 == GLFW_RELEASE) return false;

			VEEntity* characterParent;
			VECHECKPOINTER(characterParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Character Parent"));
			VEEntity* opponentParent;
			VECHECKPOINTER(opponentParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Opponent Parent"));


			glm::vec4 rot4C = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			glm::vec4 rot4O = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
			glm::vec4 dir4C = glm::normalize(characterParent->getTransform() * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
			glm::vec3 dirC = glm::vec3(dir4C.x, dir4C.y, dir4C.z);
			glm::vec4 dir4O = glm::normalize(opponentParent->getTransform() * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
			glm::vec3 dirO = glm::vec3(dir4O.x, dir4O.y, dir4O.z);

			float rotSpeed = 2.0f * event.dt;
			float speed = 5.0f * event.dt;


			VECamera* pCamera = getSceneManagerPointer()->getCamera();
			VESceneNode* pParent = pCamera->getParent();

			switch (event.idata1) {
			case GLFW_KEY_A:
				characterParent->setTransform(glm::rotate(characterParent->getTransform(), -rotSpeed, glm::vec3(rot4C.x, rot4C.y, rot4C.z)));
				break;
			case GLFW_KEY_D:
				characterParent->setTransform(glm::rotate(characterParent->getTransform(), rotSpeed, glm::vec3(rot4C.x, rot4C.y, rot4C.z)));
				break;
			case GLFW_KEY_W:
				characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * dirC));
				break;
			case GLFW_KEY_S:
				characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), -speed * dirC));
				break;
			case GLFW_KEY_UP:
				opponentParent->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * dirO));
				break;
			case GLFW_KEY_DOWN:
				opponentParent->multiplyTransform(glm::translate(glm::mat4(1.0f), -speed * dirO));
				break;
			case GLFW_KEY_LEFT:
				opponentParent->setTransform(glm::rotate(opponentParent->getTransform(), -rotSpeed, glm::vec3(rot4O.x, rot4O.y, rot4O.z)));
				break;
			case GLFW_KEY_RIGHT:
				opponentParent->setTransform(glm::rotate(opponentParent->getTransform(), +rotSpeed, glm::vec3(rot4O.x, rot4O.y, rot4O.z)));
				break;


			default:
				return false;
			};
			
			return true;
		};

	public:
		EventListenerControls(std::string name) : VEEventListener(name) { };
		virtual ~EventListenerControls() {};
	};

	class EventListenerGUIScore : public VEEventListener {
	protected:

		virtual void onDrawOverlay(veEvent event) {
			VESubrenderFW_Nuklear* pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context* ctx = pSubrender->getContext();

			if (nk_begin(ctx, "score", nk_rect(ve::imgWidth-275, 0, 275, 170), NK_WINDOW_BORDER)) {
				char outbuffer[100];
				nk_layout_row_dynamic(ctx, 45, 1);
				sprintf(outbuffer, "Score: %03d", g_score);
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);

				nk_layout_row_dynamic(ctx, 45, 1);
				switch (g_state)
				{
				case 0:
					sprintf(outbuffer, "Find the box");
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);
					sprintf(outbuffer, "or get the fish!");
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);
					break;
				case 1:
					sprintf(outbuffer, "Bring it back");
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);
					sprintf(outbuffer, "and escape the fish!");
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);
					break;
				default:
					break;
				}
			}

			nk_end(ctx);
		}

	public:
		///Constructor of class EventListenerGUI
		EventListenerGUIScore(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerGUI
		virtual ~EventListenerGUIScore() {};
	};


	class MyVulkanEngine : public VEEngine {
	private:
		std::string encoding;
		int bitRate;
		std::string extension;
		AVCodecID codecId;
	public:

		MyVulkanEngine(std::string encoding, int bitRate, std::string extension, AVCodecID codecId, bool debug = false) : 
			VEEngine(debug),
			encoding(encoding),
			bitRate(bitRate),
			extension(extension),
			codecId(codecId){};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user

		virtual void registerEventListeners() { 
			std::cout << "loading listeners" << std::endl;

			registerEventListener(new EventListenerCollision("Collision"), { veEvent::VE_EVENT_FRAME_STARTED });
			registerEventListener(new EventListenerControls("Controls"), { veEvent::VE_EVENT_KEYBOARD });
			registerEventListener(new EventListenerGUIScore("GUIScore"), { veEvent::VE_EVENT_DRAW_OVERLAY });
			registerEventListener(new EventListenerFrameDump("FrameDump", this->bitRate, this->encoding, this->extension, codecId), { veEvent::VE_EVENT_FRAME_ENDED });
		};

		void loadCameraAndLights() {
			VEEntity* characterParent;
			VECHECKPOINTER(characterParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Character Parent"));
			VESceneNode* cameraParent = getSceneManagerPointer()->createSceneNode("StandardCameraParent", getRoot(),
				glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, -10.0f)));

			//camera can only do yaw (parent y-axis) and pitch (local x-axis) rotations
			VkExtent2D extent = getWindowPointer()->getExtent();
			VECameraProjective* camera = (VECameraProjective*)getSceneManagerPointer()->createCamera("StandardCamera", VECamera::VE_CAMERA_TYPE_PROJECTIVE, cameraParent);
			camera->m_nearPlane = 0.1f;
			camera->m_farPlane = 500.1f;
			camera->m_aspectRatio = extent.width / (float)extent.height;
			camera->m_fov = 45.0f;
			camera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f));
			getSceneManagerPointer()->setCamera(camera);

			VELight* light4 = (VESpotLight*)getSceneManagerPointer()->createLight("StandardAmbientLight", VELight::VE_LIGHT_TYPE_AMBIENT, camera);
			light4->m_col_ambient = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

			//use one light source
			VELight* light1 = (VEDirectionalLight*)getSceneManagerPointer()->createLight("StandardDirLight", VELight::VE_LIGHT_TYPE_DIRECTIONAL, getRoot());     //new VEDirectionalLight("StandardDirLight");
			light1->lookAt(glm::vec3(0.0f, 20.0f, -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			light1->m_col_diffuse = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
			light1->m_col_specular = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
		}

		void loadScene() {


			VESceneNode* pScene;
			VECHECKPOINTER(pScene = getSceneManagerPointer()->createSceneNode("My Scene", getRoot()));

			//scene models

			VESceneNode* sp1;
			VECHECKPOINTER(sp1 = getSceneManagerPointer()->createSkybox("The Sky", "media/models/test/sky/cloudy",
				{ "bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg",
					"bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" }, pScene));

			VESceneNode* e4;
			VECHECKPOINTER(e4 = getSceneManagerPointer()->loadModel("The Plane", "media/models/test", "plane_t_n_s.obj", 0, pScene));
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity* pE4;
			VECHECKPOINTER(pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0"));
			pE4->setParam(glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f));

			VESceneNode* e1, *e2, * eParent1, *eParent2;
			eParent1 = getSceneManagerPointer()->createSceneNode("Character Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("Character", "media/models/free/", "al.obj", aiProcess_FlipWindingOrder));
			eParent1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.5f,0.5f,0.5f)));
			eParent1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.2f, 0.0f)));
			glm::vec4 rot4Character = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			eParent1->setTransform(glm::rotate(eParent1->getTransform(), glm::pi<float>(), glm::vec3(rot4Character.x, rot4Character.y, rot4Character.z)));

			eParent1->addChild(e1);
			

			VESceneNode* carryParent, * carryObj;
			carryParent = getSceneManagerPointer()->createSceneNode("Carry Parent", eParent1, glm::mat4(1.0));
			VECHECKPOINTER(carryObj = getSceneManagerPointer()->loadModel("Carry", "media/models/test/crate0/", "cube.obj"));
			carryParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f)));
			carryParent->multiplyTransform(glm::translate(glm::mat4(1.0f), g_farPosition));
			carryParent->addChild(carryObj);


			eParent2 = getSceneManagerPointer()->createSceneNode("Opponent Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e2 = getSceneManagerPointer()->loadModel("Opponent", "media/models/free/Models_G0301A095/", "Common_Nase.obj", aiProcess_FlipWindingOrder));

			eParent2->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f)));
			eParent2->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 1.0f, -3.0f)));
			glm::vec4 rot4Opponent = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
			eParent2->setTransform(glm::rotate(eParent2->getTransform(), glm::pi<float>() / 2, glm::vec3(rot4Opponent.x, rot4Opponent.y, rot4Opponent.z)));
			rot4Opponent = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			eParent2->setTransform(glm::rotate(eParent2->getTransform(), glm::pi<float>()/2, glm::vec3(rot4Opponent.x, rot4Opponent.y, rot4Opponent.z)));

			eParent2->addChild(e2);



			VESceneNode* caseParent, * caseObj;
			caseParent = getSceneManagerPointer()->createSceneNode("Case Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(caseObj = getSceneManagerPointer()->loadModel("Case", "media/models/test/crate0/", "cube.obj"));
			caseParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 1.0f, 2.0f)));
			caseParent->addChild(caseObj);

			VESceneNode* goalParent, * goalObj;
			goalParent = getSceneManagerPointer()->createSceneNode("Goal Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(goalObj = getSceneManagerPointer()->loadModel("Goal", "media/models/free/", "skyscraper.obj", aiProcess_FlipWindingOrder));
			goalParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f)));
			goalParent->multiplyTransform(glm::translate(glm::mat4(1.0f), g_goalPosition));
			goalParent->setTransform(glm::rotate(goalParent->getTransform(), -0.25f*glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));
			goalParent->addChild(goalObj);

			VESceneNode* cameraParent = getSceneManagerPointer()->getSceneNode("StandardCameraParent");
		}
		 
		///Load the first level into the game engine
		///The engine uses Y-UP, Left-handed
		virtual void loadLevel(uint32_t numLevel = 1) {

			loadScene();
			loadCameraAndLights();
			registerEventListeners();

		};
	};


}

using namespace ve;

void runGame() {
	bool debug = true;
	int br_1Kb = 1000;
	int br_1Mb = 1000 * br_1Kb;

	MyVulkanEngine mve("matroska", 400 * br_1Kb, "mkv", AV_CODEC_ID_MPEG4, debug = debug);	//enable or disable debugging (=callback, validation layers)
	mve.initEngine(ve::imgWidth, ve::imgHeight);
	mve.loadLevel(1);
	mve.run();
	std::cout << "Game finished!" << std::endl << "Score: " << g_score << std::endl;
}

void getKeyInput() {
	Socket::UDPReciever rec;
	while (true) {
		Socket::KeyPackage* keyPkg = rec.recieve();
		ve::processKeyInput(keyPkg);
	}
}

int main() {
	std::thread gameThread(runGame);
	std::thread keyInputThread(getKeyInput);
	gameThread.join();
	keyInputThread.join();

	return 0;
}
