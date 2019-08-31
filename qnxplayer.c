#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/keycodes.h>
#include <time.h>
#include <screen/screen.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#define MMI_MSG_ERROR printf
#define MMI_MSG_MED printf
screen_context_t screen_ctx;
screen_window_t m_screen_win;

int ShareWindow(screen_window_t share_screen_win, int disp_id, int *pos, float *scale)
{
   int rc = 0;

   if(screen_ctx && m_screen_win && share_screen_win)
   {
	printf("[suxch]%s\n", __func__);
      /*
       * TODO: If it's not set to be invisible, it will flicker with a white interface when opened.
       */
      // int visible = 1;
      // screen_set_window_property_iv(share_screen_win, SCREEN_PROPERTY_VISIBLE, &visible);

      rc = screen_share_window_buffers(share_screen_win, m_screen_win);
      if ( rc )
      {
         MMI_MSG_ERROR("screen_share_window_buffers failed %d", rc);
      }

      {
         unsigned int nDisplays=0;

         if ( !rc )
         {
            rc = screen_get_context_property_iv(screen_ctx, SCREEN_PROPERTY_DISPLAY_COUNT,
                                                   (int *)&nDisplays);
            if (rc)
            {
               MMI_MSG_ERROR("screen_get_context_property_iv(SCREEN_PROPERTY_DISPLAY_COUNT)");
            }
         }

         MMI_MSG_MED("display count %d", nDisplays);

         if ( !rc )
         {
            screen_display_t* screen_disp = (screen_display_t *) calloc(nDisplays, sizeof(*screen_disp));
            if (screen_disp == NULL)
            {
               fprintf(stderr, "could not allocate memory for display list\n");
               rc = 1;
            }

            if ( !rc )
            {
               rc = screen_get_context_property_pv(screen_ctx, SCREEN_PROPERTY_DISPLAYS,
                                                   (void **)screen_disp);
               if (rc)
               {
                  MMI_MSG_ERROR("screen_get_context_property_ptr(SCREEN_PROPERTY_DISPLAYS)");
                  free(screen_disp);
               }
            }

            if ( !rc )
            {
               rc = screen_set_window_property_pv(share_screen_win, SCREEN_PROPERTY_DISPLAY,
                                                      (void **)&screen_disp[disp_id]);
               if (rc)
               {
                  //MMI_MSG_ERROR("screen_set_window_property_ptr(SCREEN_PROPERTY_DISPLAY), display id = %d",m_sDeviceCfg.nDisplayId);
               }
               else
               {
                  //MMI_MSG_LOW("screen_set_window_property_ptr(SCREEN_PROPERTY_DISPLAY), display id = %d",m_sDeviceCfg.nDisplayId);
               }

               free(screen_disp);
            }
         }
      }
#if 0
      if ( !rc )
      {
         int format = local_map_omx_color_format(m_sDeviceCfg.eColorFormat);
         rc = screen_set_window_property_iv(share_screen_win, SCREEN_PROPERTY_FORMAT, &format);
         if (rc)
         {
            MMI_MSG_ERROR("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
         }
      }
#endif

      if ( !rc )
      {
         int size[2];
         size[0] = 3840; //m_sDeviceCfg.nFrameWidth;
         size[1] = 720; //m_sDeviceCfg.nFrameHeight;

         rc = screen_set_window_property_iv(share_screen_win, SCREEN_PROPERTY_SOURCE_SIZE, size);
         if (rc)
         {
            MMI_MSG_ERROR("screen_get_window_property_iv(SCREEN_PROPERTY_SOURCE_SIZE)");
         }
      }

      if ( !rc )
      {
         MMI_MSG_MED("Screen setting position %dx%d", pos[0], pos[1]);
         rc = screen_set_window_property_iv(share_screen_win, SCREEN_PROPERTY_POSITION, pos);
         if (rc)
         {
           MMI_MSG_ERROR("screen_set_window_property_iv(SCREEN_PROPERTY_POSITION)");
         }
      }
#if 0
      if ( !rc )
      {
         int bsize[2];
         bsize[0] = m_sDeviceCfg.nFrameWidth;
         bsize[1] = m_sDeviceCfg.nFrameHeight;

         rc = screen_set_window_property_iv(share_screen_win, SCREEN_PROPERTY_BUFFER_SIZE, bsize);
         if (rc)
         {
            MMI_MSG_ERROR("screen_set_window_property_iv(SCREEN_PROPERTY_BUFFER_SIZE)");
         }
      }
#endif
      if ( !rc )
      {
         int outsize[2]= {0};

         //adjust aspect ratio
	/*
         if ( ( m_sDeviceCfg.nRotation == 90 ) || ( m_sDeviceCfg.nRotation == 270 ) )
         {
            outsize[0] = m_sDeviceCfg.nFrameHeight * scale[0];
            outsize[1] = m_sDeviceCfg.nFrameWidth * scale[1];
         }
         else
*/
         {
            outsize[0] = 3840; //m_sDeviceCfg.nFrameWidth * scale[0];
            outsize[1] = 720; //m_sDeviceCfg.nFrameHeight * scale[1];
         }

         rc = screen_set_window_property_iv(share_screen_win, SCREEN_PROPERTY_SIZE, outsize);
         if (rc)
         {
            MMI_MSG_ERROR("screen_get_window_property_iv(SCREEN_PROPERTY_SIZE)");
         }
      }
   }

   return rc;
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

	// screen
	int bformat = SCREEN_FORMAT_NV12;//SCREEN_FORMAT_RGBA8888;//SCREEN_FORMAT_RGB888;
	int usage = SCREEN_USAGE_NATIVE | SCREEN_USAGE_WRITE | SCREEN_USAGE_READ;
	int nbuffers = 2;
	int ndisplays = 0;
	screen_display_t *screen_dpy;
	struct {
			pthread_mutex_t mutex;
			pthread_cond_t cond;
			enum { detached, attached, focused } state;
			int size[2];
			int stride[2];
			screen_window_t screen_win;
			void **pointers;
			screen_buffer_t screen_buf[2];
	} *displays;

	int ret, got_picture;
	int idx = -1;
	int rc;

	// init screen
	screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT);
	screen_get_context_property_iv(screen_ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &ndisplays);
	if (ndisplays > 0) {
		screen_dpy = calloc(ndisplays, sizeof(screen_display_t));
		screen_get_context_property_pv(screen_ctx, SCREEN_PROPERTY_DISPLAYS, (void **)screen_dpy);
	}
	printf("system has %d display\n", ndisplays);

	displays = calloc(ndisplays, sizeof(*displays));
	for (i = 0; i < ndisplays; i++) {
		int active = 0;
		screen_get_display_property_iv(screen_dpy[i], SCREEN_PROPERTY_ATTACHED, &active);
		if (active) {
			if (idx == -1) {
				displays[i].state = focused;
				idx = i;
			} else {
				displays[i].state = attached;
			}
		} else {
			displays[i].state = detached;
		}

		printf("dpy: %d, state: %d\n", i, displays[i].state);
		pthread_mutex_init(&displays[i].mutex, NULL);
		pthread_cond_init(&displays[i].cond, NULL);
	}
	ndisplays=1;
	for (i = 0; i < ndisplays; i++) {
		displays[i].pointers = (void **)malloc(sizeof(void *) * nbuffers);
		if (displays[i].pointers == NULL) {
			perror("malloc buffer fail\n");
		}
		screen_create_window(&displays[i].screen_win, screen_ctx);
		/*if (idx != i) {
			idx = i;
			screen_set_window_property_pv(displays[i].screen_win, SCREEN_PROPERTY_DISPLAY, (void **)&displays[i]);
		}*/
		screen_set_window_property_pv(displays[i].screen_win, SCREEN_PROPERTY_DISPLAY, (void **)&screen_dpy[i]);
		screen_set_window_property_iv(displays[i].screen_win, SCREEN_PROPERTY_USAGE, &usage);

		displays[i].size[0] = 3840;
		displays[i].size[1] = 720;
		rc = screen_set_window_property_iv(displays[i].screen_win, SCREEN_PROPERTY_SIZE, displays[i].size);
		if (rc) {
			perror("screen_set_window_property_iv[SCREEN_PROPERTY_SIZE]");
		} else {
			printf("width: %d, height: %d\n", displays[i].size[0], displays[i].size[1]);
		}
		
		rc = screen_get_window_property_iv(displays[i].screen_win, SCREEN_PROPERTY_SIZE, displays[i].size);
		if (rc) {
			perror("screen_get_window_property_iv[SCREEN_PROPERTY_SIZE]");
		} else {
			printf("width: %d, height: %d\n", displays[i].size[0], displays[i].size[1]);
		}
		
		rc = screen_set_window_property_iv(displays[i].screen_win, SCREEN_PROPERTY_FORMAT, &bformat);
		if (rc) {
			perror("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
			goto fail;
		}
		screen_create_window_buffers(displays[i].screen_win, nbuffers);

		screen_get_window_property_pv(displays[i].screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)displays[i].screen_buf);
		for (j = 0; j < nbuffers; j++) {
			rc = screen_get_buffer_property_iv(displays[i].screen_buf[j], SCREEN_PROPERTY_STRIDE, &displays[i].stride[j]);
			if (rc) {
				perror("screen_get_buffer_property_iv[SCREEN_PROPERTY_STRIDE]");
			} else {
				printf("stride: %d\n", displays[i].stride[j]);
			}
			rc = screen_get_buffer_property_pv(displays[i].screen_buf[j], SCREEN_PROPERTY_POINTER, &(displays[i].pointers[j]));
			if (rc) {
				perror("screen_get_window_property_pv(SCREEN_PROPERTY_POINTER)");
				goto fail;
			}
		}
	}
	m_screen_win = displays[0].screen_win;
	// int ShareWindow(screen_window_t share_screen_win, int disp_id, int *pos, float *scale)
	int pos[2] = {-1920, 0};
	float scale[2] = {1, 1};
	screen_create_window(&displays[1].screen_win, screen_ctx);
	ShareWindow(displays[1].screen_win, 1, pos, scale);

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

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, displays[0].size[0], displays[0].size[1], AV_PIX_FMT_NV12/*AV_PIX_FMT_BGRA*/, SWS_BICUBIC, NULL, NULL, NULL);
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
				outbuf[0] = displays[0].pointers[0];
				outbuf[1] = displays[0].pointers[0] + displays[0].stride[0] * displays[0].size[1];
				outbuf[2] = 0;
				outbuf[3] = 0;

				outlinesize[0] = displays[0].stride[0];
				outlinesize[1] = displays[0].stride[0];
				outlinesize[2] = 0;
				outlinesize[3] = 0;

				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, outbuf, outlinesize);
				screen_post_window(displays[0].screen_win, displays[0].screen_buf[0], 0, NULL, 0);
			}
		}
		av_free_packet(packet);
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
	av_free(pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
 
	return 0;
}
