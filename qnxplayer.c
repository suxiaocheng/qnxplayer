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

typedef struct screen_buffer_queue{
	void *buf[10];
	int ipos;
	int opos;
	int valid_item;
}screen_buffer_queue_t;

struct {
		pthread_mutex_t mutex;
		pthread_cond_t cond;
		enum { detached, attached, focused } state;
		int size[2];
		int stride[2];
		screen_window_t screen_win;
		void **pointers;
		screen_buffer_t screen_buf[2];
} displays[2] = {0};
int nbuffers = 2;

#define MMI_MSG_ERROR printf
#define MMI_MSG_MED printf
screen_context_t screen_ctx = NULL;
screen_window_t m_screen_win = NULL;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_free_buf = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_post_screen = PTHREAD_COND_INITIALIZER;

screen_buffer_queue_t screen_buffer_queue_free;
screen_buffer_queue_t screen_buffer_queue_used;

void init_screen_buffer_queue(screen_buffer_queue_t *queue)
{
	queue->ipos = queue->opos = 0;
	queue->valid_item = 0;
}

int put_screen_buffer_queue(screen_buffer_queue_t *queue, void *item)
{
	int status = 1;
	pthread_mutex_lock( &mutex );
	if (queue->valid_item < sizeof(queue->buf)/sizeof(void *)) {
		if(queue->ipos < (sizeof(queue->buf)/sizeof(void *) - 1)) {
			queue->ipos++;
		} else {
			queue->ipos = 0;
		}
		queue->buf[queue->ipos] = item;
		queue->valid_item++;
		status = 0;
		// printf("%s %p add %p, valid: %d\n", __func__, queue, item, queue->valid_item);
	}
	pthread_mutex_unlock( &mutex );
	return status;
}

void *get_screen_buffer_queue(screen_buffer_queue_t *queue)
{
	void *buf = NULL;
	pthread_mutex_lock( &mutex );
	if (queue->valid_item > 0) {
		if (queue->opos < (sizeof(queue->buf)/sizeof(void *) - 1)) {
			queue->opos++;
		} else {
			queue->opos = 0;
		}
		queue->valid_item--;
		buf = queue->buf[queue->opos];
		// printf("%s %p del %p, valid: %d\n", __func__, queue, buf, queue->valid_item);
	}
	pthread_mutex_unlock( &mutex );
	return buf;
}

int ShareWindow(screen_window_t share_screen_win, int disp_id, int *pos, float *scale)
{
   int rc = 0;

   if(screen_ctx && m_screen_win && share_screen_win)
   {
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

void* screen_refreash( void * arg )
{
	void *buf = NULL;
	int first_refreash = true;
	struct timeval tv_last;
	struct timeval tv_current;
	int i;
	while( 1 )
	{
		buf = NULL;
		while (buf == NULL){
			buf = get_screen_buffer_queue(&screen_buffer_queue_used);
			if (buf == NULL) {
				// pthread_cond_wait( &cond_post_screen, &mutex );
				usleep(10);
			}
		}
		for(i=0; i<nbuffers; i++) {
			if (buf == displays[0].pointers[i]) {
				break;
			}
		}
		if (first_refreash) {
			gettimeofday(&tv_last, NULL);
		} else {
			/*
			while (1) {
				gettimeofday(&tv_current, NULL);
				double diff = tv_current.tv_usec / 1000000.0 + tv_current.tv_sec - 
					(tv_last.tv_usec / 1000000.0 + tv_last.tv_sec);
				if (diff >= 0.033) {
					break;
				} else {
					usleep(33-(int)(diff*1000));
				}
			}
			tv_last = tv_current;
			*/
		}
		if (i < nbuffers) {
			screen_post_window(displays[0].screen_win, displays[0].screen_buf[i], 0, NULL, 0);
			printf("->%lf\n", (double)tv_last.tv_usec / 1000000.0 + tv_last.tv_sec);
		} else {
			printf("Error: unknow buffer, didn't know how to post\n");
		}
		/* After we post the buffer to screen, return it back to free pool */
		put_screen_buffer_queue(&screen_buffer_queue_free, buf);
		pthread_cond_signal( &cond_free_buf );	
	}
	return 0;
}

int main(int argc, char* argv[])
{
	//FFmpeg
	AVFormatContext	*pFormatCtx = NULL;
	int i, j, videoindex;
	AVCodecContext	*pCodecCtx = NULL;
	AVCodec	*pCodec;
	AVFrame	*pFrame = NULL,*pFrameYUV = NULL;
	AVPacket *packet;
	struct SwsContext *img_convert_ctx = NULL;

	// framerate static data
	struct timeval tv_last;
	struct timeval tv_tmp1;
	struct timeval tv_tmp2;
	struct timeval tv_current;
	int frame_rate = 0;
	double fframe_rate = 0;

	// buffer convert data struct
	unsigned char *outbuf[4];
	int outlinesize[4];

	// screen
	int bformat = SCREEN_FORMAT_NV12;//SCREEN_FORMAT_RGBA8888;//SCREEN_FORMAT_RGB888;
	int usage = SCREEN_USAGE_NATIVE | SCREEN_USAGE_WRITE | SCREEN_USAGE_READ;
	int ndisplays = 0;
	int ret, got_picture;
	int idx = -1;
	int rc;

	// Init two queue for future thread used
	init_screen_buffer_queue(&screen_buffer_queue_free);
	init_screen_buffer_queue(&screen_buffer_queue_used);

	// step1: ffmpeg init
	avformat_network_init();
	
	pFormatCtx = avformat_alloc_context();
 
	if(avformat_open_input(&pFormatCtx,argv[1],NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		goto fail;
	}
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		goto fail;
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
		goto fail;
	}
	pCodecCtx=pFormatCtx->streams[videoindex]->codec;
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL){
		printf("Codec not found.\n");
		goto fail;
	}
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
		printf("Could not open codec.\n");
		goto fail;
	}
	
	pFrame=av_frame_alloc();
	pFrameYUV=av_frame_alloc();
 
	packet=(AVPacket *)av_malloc(sizeof(AVPacket));
	printf("------------- File Information ------------------\n");
	av_dump_format(pFormatCtx,0,argv[1],0);
	printf("-------------------------------------------------\n");

	// step2: init screen
	screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT);
	screen_get_context_property_iv(screen_ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &ndisplays);
	printf("system has %d display\n", ndisplays);

	displays[0].pointers = (void **)malloc(sizeof(void *) * nbuffers);
	if (displays[0].pointers == NULL) {
		perror("malloc buffer fail\n");
	}
	screen_create_window(&displays[0].screen_win, screen_ctx);
	screen_set_window_property_iv(displays[0].screen_win, SCREEN_PROPERTY_USAGE, &usage);

	/* force the window buffer to the video size */
	displays[0].size[0] = pCodecCtx->width;
	displays[0].size[1] = pCodecCtx->height;
	rc = screen_set_window_property_iv(displays[0].screen_win, SCREEN_PROPERTY_SIZE, displays[0].size);
	if (rc) {
		perror("screen_set_window_property_iv[SCREEN_PROPERTY_SIZE]");
	} else {
		printf("width: %d, height: %d\n", displays[0].size[0], displays[0].size[1]);
	}
	
	rc = screen_set_window_property_iv(displays[0].screen_win, SCREEN_PROPERTY_FORMAT, &bformat);
	if (rc) {
		perror("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
		goto fail;
	}
	screen_create_window_buffers(displays[0].screen_win, nbuffers);

	screen_get_window_property_pv(displays[0].screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)displays[0].screen_buf);
	for (j = 0; j < nbuffers; j++) {
		rc = screen_get_buffer_property_iv(displays[0].screen_buf[j], SCREEN_PROPERTY_STRIDE, &displays[0].stride[j]);
		if (rc) {
			perror("screen_get_buffer_property_iv[SCREEN_PROPERTY_STRIDE]");
		} else {
			printf("stride: %d\n", displays[0].stride[j]);
		}
		rc = screen_get_buffer_property_pv(displays[0].screen_buf[j], SCREEN_PROPERTY_POINTER, &(displays[0].pointers[j]));
		if (rc) {
			perror("screen_get_window_property_pv(SCREEN_PROPERTY_POINTER)");
			goto fail;
		}
		put_screen_buffer_queue(&screen_buffer_queue_free, (void *)displays[0].pointers[j]);
	}
	 
	m_screen_win = displays[0].screen_win;
	int pos[2] = {-1920, 0};
	float scale[2] = {1, 1};
	screen_create_window(&displays[1].screen_win, screen_ctx);
	ShareWindow(displays[1].screen_win, 1, pos, scale);

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, displays[0].size[0], displays[0].size[1], AV_PIX_FMT_NV12/*AV_PIX_FMT_BGRA*/, SWS_BICUBIC, NULL, NULL, NULL);
	printf("codec ctx, height: %d, width: %d\n", pCodecCtx->height, pCodecCtx->width);

	pthread_create( NULL, NULL, &screen_refreash, NULL );

	gettimeofday(&tv_last, NULL);
	while(av_read_frame(pFormatCtx, packet)>=0){
		if(packet->stream_index==videoindex){
			//Decode
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if(ret < 0){
				printf("Decode Error.\n");
				goto fail;
			}
			if(got_picture){
				void *buf = NULL; 
				while (buf == NULL) {
					buf = get_screen_buffer_queue(&screen_buffer_queue_free);
					if (buf == NULL) {
						pthread_cond_wait( &cond_free_buf, &mutex );
					}
				}
				outbuf[0] = buf;
				outbuf[1] = buf + displays[0].stride[0] * displays[0].size[1];
				outbuf[2] = 0;
				outbuf[3] = 0;

				outlinesize[0] = displays[0].stride[0];
				outlinesize[1] = displays[0].stride[0];
				outlinesize[2] = 0;
				outlinesize[3] = 0;
				gettimeofday(&tv_tmp1, NULL);
				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, outbuf, outlinesize);
				gettimeofday(&tv_tmp2, NULL);

				double decode_diff = tv_tmp2.tv_usec / 1000000.0 + tv_tmp2.tv_sec - 
					(tv_tmp1.tv_usec / 1000000.0 + tv_tmp1.tv_sec);
				printf("# %lf\n", decode_diff);

				put_screen_buffer_queue(&screen_buffer_queue_used, buf);
				pthread_cond_signal( &cond_post_screen );

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
		av_packet_unref(packet);
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


fail: 
	if (displays[0].pointers) {
		free(displays[0].pointers);	
	}
	if (displays[0].screen_win) {
		screen_destroy_window_buffers(displays[0].screen_win);
		screen_destroy_window(displays[0].screen_win);
	}
	if (m_screen_win) {
		screen_destroy_window(m_screen_win);	
	}
	if (img_convert_ctx) {
		sws_freeContext(img_convert_ctx);
	}
	if (pFrameYUV) {
		av_free(pFrameYUV);
	}
	if (pCodecCtx) {
		avcodec_close(pCodecCtx);
	}
	if (pFormatCtx) {
		avformat_close_input(&pFormatCtx);
	}
 
	return 0;
}
