#include "VEInclude.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include <libswscale/swscale.h>
}

namespace ve {

	uint8_t endcode[] = { 0, 0, 1, 0xb7 };
	uint32_t g_score = 0;
	uint8_t g_state = 0; // 0 - searching, 1 - picked up
	int imgWidth = 800;
	int imgHeight = 600;

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
		AVPacket* pkt;
		FILE* outfile;
		SwsContext* sws_ctx;
		const int SKIP_FRAMES = 0;

		void encode(AVCodecContext* enc_ctx, uint8_t* dataImage, AVPacket* pkt, FILE* outfile)
		{

			AVFrame* frame = this->frame;

			if (!dataImage)
			{
				frame = NULL;
			}
			else
			{
				uint8_t* inData[1] = { dataImage }; // RGBA have one plane
				int inLinesize[1] = { 4 * frame->width }; // RGB stride
				sws_scale(this->sws_ctx, inData, inLinesize, 0, frame->height, frame->data, frame->linesize);
			}

			int ret;

			// send the frame to the encoder */
			ret = avcodec_send_frame(enc_ctx, frame);
			if (ret < 0) {
				fprintf(stderr, "error sending a frame for encoding\n");
				exit(1);
			}


			while (ret >= 0) {
				int ret = avcodec_receive_packet(enc_ctx, pkt);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					return;
				else if (ret < 0) {
					fprintf(stderr, "error during encoding\n");
					exit(1);
				}

				printf("encoded frame %lld (size=%5d)\n", pkt->pts, pkt->size);
				fwrite(pkt->data, 1, pkt->size, outfile);
				av_packet_unref(pkt);
			}
		}

		void initializeCodec()
		{
			std::string filename("media/videos/video-" + std::to_string(time(NULL)) + ".mp4");

			avcodec_register_all();
			  
			const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
			if (!codec) {
				fprintf(stderr, "codec not found\n");
				exit(1);
			}

			AVCodecContext* c = avcodec_alloc_context3(codec);
			AVFrame* picture = av_frame_alloc();

			AVPacket* pkt = av_packet_alloc();
			if (!pkt) {
				fprintf(stderr, "Cannot alloc packet\n");
				exit(1);
			}

			c->bit_rate = 400000;

			// resolution must be a multiple of two
			c->width = imgWidth;
			c->height = imgHeight;
			// frames per second
			c->time_base.num = 1;
			c->time_base.den = 25;
			c->framerate.num = 25;
			c->framerate.den = 1;

			c->gop_size = 10; // emit one intra frame every ten frames
			c->max_b_frames = 1;
			c->pix_fmt = AV_PIX_FMT_YUV420P;

			// open it
			if (avcodec_open2(c, codec, NULL) < 0) {
				fprintf(stderr, "could not open codec\n");
				exit(1);
			}

			FILE* f = fopen(filename.c_str(), "wb");
			if (!f) {
				fprintf(stderr, "could not open %s\n", filename);
				exit(1);
			}

			picture->format = c->pix_fmt;
			picture->width = c->width;
			picture->height = c->height;

			int ret = av_frame_get_buffer(picture, 0);
			if (ret < 0) {
				fprintf(stderr, "could not alloc the frame data\n");
				exit(1);
			}

			SwsContext* ctx = sws_getContext(c->width, c->height,
				AV_PIX_FMT_RGBA, c->width, c->height,
				AV_PIX_FMT_YUV420P, 0, 0, 0, 0);


			this->frame = picture;
			this->enc_ctx = c;
			this->pkt = pkt;
			this->outfile = f;
			this->sws_ctx = ctx;
		}
		void finalizeCodec()
		{
			this->encode(this->enc_ctx, NULL, this->pkt, this->outfile);
			fwrite(endcode, 1, sizeof(endcode), this->outfile);
			fclose(this->outfile);

			avcodec_free_context(&(this->enc_ctx));
			av_frame_free(&(this->frame));
			av_packet_free(&(this->pkt));
			sws_freeContext(this->sws_ctx);
		}

	protected:
		virtual void onFrameEnded(veEvent event)
		{
			framesPassed++;
			if (framesPassed >= SKIP_FRAMES)
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
				
				encode(this->enc_ctx, dataImage, this->pkt, this->outfile);

				

				delete[] dataImage;
				framesPassed = 0;
			}

		}
	public:
		///Constructor of class EventListenerCollision
		EventListenerFrameDump(std::string name) : VEEventListenerGLFW(name) {
			this->initializeCodec();
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
			float distance;
			switch (g_state)
			{
			case 0: //searching
				distance = glm::distance(caseParent->getPosition(), characterParent->getPosition());
				if (distance < 1.5f) {
					caseParent->setPosition(g_farPosition);
					carryParent->setPosition(g_carryPosition);
					goalParent->setPosition(g_goalPosition);
					g_state = 1;
				}
				break;
			case 1: //picked up
				distance = glm::distance(goalParent->getPosition(), characterParent->getPosition());
				if (distance < 3.0f) {
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


			glm::vec4 rot4Character = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			glm::vec4 dir4 = glm::normalize(characterParent->getTransform() * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
			glm::vec3 dir = glm::vec3(dir4.x, dir4.y, dir4.z);

			float rotSpeed = 2.0f * event.dt;
			float speed = 5.0f * event.dt;


			VECamera* pCamera = getSceneManagerPointer()->getCamera();
			VESceneNode* pParent = pCamera->getParent();

			switch (event.idata1) {
			case GLFW_KEY_A:
				characterParent->setTransform(glm::rotate(characterParent->getTransform(), -rotSpeed, glm::vec3(rot4Character.x, rot4Character.y, rot4Character.z)));
				break;
			case GLFW_KEY_D:
				characterParent->setTransform(glm::rotate(characterParent->getTransform(), rotSpeed, glm::vec3(rot4Character.x, rot4Character.y, rot4Character.z)));
				break;
			case GLFW_KEY_W:
				characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * dir));
				break;
			case GLFW_KEY_S:
				characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), -speed * dir));
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

	class EventListenerGUI : public VEEventListener {
	protected:

		virtual void onDrawOverlay(veEvent event) {
			VESubrenderFW_Nuklear* pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context* ctx = pSubrender->getContext();

			if (nk_begin(ctx, "", nk_rect(600, 0, 200, 170), NK_WINDOW_BORDER)) {
				char outbuffer[100];
				nk_layout_row_dynamic(ctx, 45, 1);
				sprintf(outbuffer, "Score: %03d", g_score);
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);

				nk_layout_row_dynamic(ctx, 45, 1);
				switch (g_state)
				{
				case 0:
					sprintf(outbuffer, "Find the box!");
					break;
				case 1:
					sprintf(outbuffer, "Bring it back!");
					break;
				default:
					break;
				}
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);
			}

			nk_end(ctx);
		}

	public:
		///Constructor of class EventListenerGUI
		EventListenerGUI(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerGUI
		virtual ~EventListenerGUI() {};
	};


	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(bool debug = false) : VEEngine(debug) {};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user

		virtual void registerEventListeners() { 
			std::cout << "loading listeners" << std::endl;

			registerEventListener(new EventListenerCollision("Collision"), { veEvent::VE_EVENT_FRAME_STARTED });
			registerEventListener(new EventListenerControls("Controls"), { veEvent::VE_EVENT_KEYBOARD });
			registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY });
			registerEventListener(new EventListenerFrameDump("FrameDump"), { veEvent::VE_EVENT_FRAME_ENDED });
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

			VESceneNode* e1, * eParent;
			eParent = getSceneManagerPointer()->createSceneNode("Character Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("Character", "media/models/free/", "al.obj", aiProcess_FlipWindingOrder));
			eParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.5f,0.5f,0.5f)));
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.2f, 0.0f)));
			glm::vec4 rot4Character = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			eParent->setTransform(glm::rotate(eParent->getTransform(), glm::pi<float>(), glm::vec3(rot4Character.x, rot4Character.y, rot4Character.z)));
			
			VESceneNode* caseParent, * caseObj;
			caseParent = getSceneManagerPointer()->createSceneNode("Case Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(caseObj = getSceneManagerPointer()->loadModel("Case", "media/models/test/crate0/", "cube.obj"));
			caseParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 1.0f, 2.0f)));
			caseParent->addChild(caseObj);

			VESceneNode* carryParent, * carryObj;
			carryParent = getSceneManagerPointer()->createSceneNode("Carry Parent", eParent, glm::mat4(1.0));
			VECHECKPOINTER(carryObj = getSceneManagerPointer()->loadModel("Carry", "media/models/test/crate0/", "cube.obj"));
			carryParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f)));
			carryParent->multiplyTransform(glm::translate(glm::mat4(1.0f), g_farPosition));
			carryParent->addChild(carryObj);

			VESceneNode* goalParent, * goalObj;
			goalParent = getSceneManagerPointer()->createSceneNode("Goal Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(goalObj = getSceneManagerPointer()->loadModel("Goal", "media/models/free/", "skyscraper.obj", aiProcess_FlipWindingOrder));
			goalParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f)));
			goalParent->multiplyTransform(glm::translate(glm::mat4(1.0f), g_goalPosition));
			goalParent->setTransform(glm::rotate(goalParent->getTransform(), -0.25f*glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));
			goalParent->addChild(goalObj);

			VESceneNode* cameraParent = getSceneManagerPointer()->getSceneNode("StandardCameraParent");
			eParent->addChild(e1);
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

int main() {

	bool debug = true;

	MyVulkanEngine mve(debug);	//enable or disable debugging (=callback, validation layers)
	mve.initEngine();
	mve.loadLevel(1);
	mve.run();
	std::cout << "Game finished!" << std::endl << "Score: " << g_score << std::endl;

	return 0;
}
