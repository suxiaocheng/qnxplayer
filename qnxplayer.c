#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL.h>

SDL_Window *g_pWindow = 0;
SDL_Renderer *g_pRenderer = 0;
int need_quit = false;

void when_sigint()  
{
	printf("User quit, set quit flag\n");
	need_quit = true;
	//exit(0);  
}  

int main(int argc, char* argv[])
{
	//FFmpeg
	AVFormatContext	*pFormatCtx;
	int i, j, videoindex;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame,*pFrameYUV;
	AVPacket *packet;
	struct SwsContext *img_convert_ctx;

	// framerate static data
	struct timeval tv_last;
	struct timeval tv_current;
	int frame_rate = 0;
	double fframe_rate = 0;

	// buffer convert data struct
	unsigned char *outbuf[4];
	int outlinesize[4];

	int ret;
	int got_picture;
	unsigned char *raw_nv12 = NULL;

	// sdl var
	SDL_Texture* m_pTexture; // the new SDL_Texture variable
	SDL_Rect m_sourceRectangle; // the first rectangle
	SDL_Rect m_destinationRectangle; // another rectangle
	SDL_Surface* pTempSurface = NULL;
	int window_width = 640;
	int widnow_height = 480;

	signal(SIGINT,when_sigint);

	// initialize SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) >= 0) {
	// if succeeded create our window
		g_pWindow = SDL_CreateWindow("Chapter 1: Setting up SDL",
				SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
				window_width, widnow_height, SDL_WINDOW_SHOWN);
	// if the window creation succeeded create our renderer
		if (g_pWindow != 0) {
			g_pRenderer = SDL_CreateRenderer(g_pWindow, -1, SDL_RENDERER_SOFTWARE);
			printf("render is create: 0x%x(%s)\n", g_pRenderer, SDL_GetError());
		} else {
			printf("Error: g_pWindow create fail\n");
		}
	} else {
		return 1;	// sdl could not initialize
	}


	// ffmpeg init
	av_register_all();
	avformat_network_init();
	
	pFormatCtx = avformat_alloc_context();
 
	if(avformat_open_input(&pFormatCtx,argv[1],NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}
	}
	if(videoindex==-1){
		printf("Didn't find a video stream.\n");
		return -1;
	}
	pCodecCtx=pFormatCtx->streams[videoindex]->codec;
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL){
		printf("Codec not found.\n");
		return -1;
	}
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
		printf("Could not open codec.\n");
		return -1;
	}
	
	pFrame=av_frame_alloc();
	pFrameYUV=av_frame_alloc();
 
	packet=(AVPacket *)av_malloc(sizeof(AVPacket));
	printf("------------- File Information ------------------\n");
	av_dump_format(pFormatCtx,0,argv[1],0);
	printf("-------------------------------------------------\n");

	// sdl post init
	// pTempSurface = SDL_CreateRGBSurface(0, pCodecCtx->width, pCodecCtx->height, 32, 0, 0, 0, 0); 
	m_pTexture = SDL_CreateTexture(g_pRenderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_TARGET, pCodecCtx->width, pCodecCtx->height);
	if (m_pTexture == NULL) {
		printf("SDL_CreateTexture fail: %s\n", SDL_GetError());
	}
	raw_nv12 = SDL_calloc(1, pCodecCtx->width * pCodecCtx->height / 2 * 3);

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_NV12, SWS_BICUBIC, NULL, NULL, NULL);
	printf("codec ctx, height: %d, width: %d\n", pCodecCtx->height, pCodecCtx->width);

	gettimeofday(&tv_last, NULL);
	while(av_read_frame(pFormatCtx, packet)>=0){
		if(packet->stream_index==videoindex){
			//Decode
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if(ret < 0){
				printf("Decode Error.\n");
				return -1;
			}
			if(got_picture){
				outbuf[0] = raw_nv12;
				outbuf[1] = (unsigned char *)(raw_nv12) + pCodecCtx->width * pCodecCtx->height;
				outbuf[2] = 0;
				outbuf[3] = 0;

				outlinesize[0] = pCodecCtx->width;
				outlinesize[1] = pCodecCtx->width;
				outlinesize[2] = 0;
				outlinesize[3] = 0;
				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, outbuf, outlinesize);
				SDL_UpdateTexture(m_pTexture, NULL, raw_nv12, pCodecCtx->width);
				ret = SDL_QueryTexture(m_pTexture, NULL, NULL,
						&m_sourceRectangle.w, &m_sourceRectangle.h);
				if(ret < 0) { 
					printf("SDL_QueryTexture fail: %s\n", SDL_GetError());
				}

				m_destinationRectangle.x = m_sourceRectangle.x = 0;
				m_destinationRectangle.y = m_sourceRectangle.y = 0; 
				m_destinationRectangle.w = m_sourceRectangle.w; 
				m_destinationRectangle.h = m_sourceRectangle.h;
				SDL_RenderCopy(g_pRenderer, m_pTexture, &m_sourceRectangle,
						&m_destinationRectangle);

				SDL_RenderPresent(g_pRenderer);

				frame_rate++;
				gettimeofday(&tv_current, NULL);
				double diff = tv_current.tv_usec / 1000000.0 + tv_current.tv_sec - 
					(tv_last.tv_usec / 1000000.0 + tv_last.tv_sec);
				if(diff >= 1) {
					fframe_rate = frame_rate/diff;
					printf("Current frame rate is : %lf\n", fframe_rate);
					tv_last = tv_current;
					frame_rate = 0;
				}

			}
		}
		if (need_quit == true) {
			break;
		}
		av_free_packet(packet);
	}
	SDL_Quit();
#if 0 
	//FIX: Flush Frames remained in Codec
	while (1) {
		ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
		if (ret < 0)
			break;
		if (!got_picture)
			break;
		sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
		
		SDL_LockYUVOverlay(bmp);
		pFrameYUV->data[0]=bmp->pixels[0];
		pFrameYUV->data[1]=bmp->pixels[2];
		pFrameYUV->data[2]=bmp->pixels[1];     
		pFrameYUV->linesize[0]=bmp->pitches[0];
		pFrameYUV->linesize[1]=bmp->pitches[2];   
		pFrameYUV->linesize[2]=bmp->pitches[1];
#if OUTPUT_YUV420P
		int y_size=pCodecCtx->width*pCodecCtx->height;  
		fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);    //Y 
		fwrite(pFrameYUV->data[1],1,y_size/4,fp_yuv);  //U
		fwrite(pFrameYUV->data[2],1,y_size/4,fp_yuv);  //V
#endif
 
		SDL_UnlockYUVOverlay(bmp); 
		SDL_DisplayYUVOverlay(bmp, &rect); 
		//Delay 40ms
		SDL_Delay(40);
	}
#endif 
	sws_freeContext(img_convert_ctx);
fail: 
	if (raw_nv12 != NULL) {
		SDL_free(raw_nv12);
		raw_nv12 = NULL;
	}
	av_free(pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
 
	return 0;
}
