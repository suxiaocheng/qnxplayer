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
#include <signal.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "esUtil.h"

int need_quit = false;

typedef struct
{
   // Handle to a program object
   GLuint programObject;

   GLint texture_y;
   GLint texture_u;
   GLint texture_v;

   // Texture handle
   GLuint textureId[3];

} UserData;

unsigned char *yuv420p;
#if 1
unsigned char *video_filename="/home/suxiaocheng/Videos/desaysv.3gp";
int width = 1920;
int height = 720;
#else
unsigned char *video_filename="/home/suxiaocheng/Videos/Lion4.mp4";
int width = 3840;
int height = 720;
#endif

AVFormatContext	*pFormatCtx;
int i, j, videoindex;
AVCodecContext	*pCodecCtx;
AVCodec			*pCodec;
AVFrame	*pFrame,*pFrameYUV;
AVPacket *packet;
struct SwsContext *img_convert_ctx = NULL;

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

// Create a simple 2x2 texture image with four different colors
//
int CreateSimpleTexture2D(UserData *userData)
{
   // Use tightly packed data
   // glPixelStorei ( GL_UNPACK_ALIGNMENT, 1 );
#if 0
   {
	   int fd;
	   int err;
	   ssize_t read_len;
	   struct stat statbuf;
	   char *filename = "/home/suxiaocheng/Pictures/test.yuv";
	   err = stat(filename, &statbuf);
	   if (err != 0) {
		printf("stat the file[%s] error\n", filename);
	   	return err;	
	   }
	   yuv420p = (unsigned char *)malloc(statbuf.st_size);
	   if (yuv420p == NULL) {
	   	printf("malloc fail\n");
		return -1;
	   }
	   fd = open(filename, O_RDONLY);
	   if (fd == -1) {
		printf("open the file error\n");
		free(yuv420p);
	   	return -1;
	   }
	   read_len = read(fd, yuv420p, statbuf.st_size);
	   if (read_len != statbuf.st_size) {
	   	printf("Read size is not match filesize");
	   }
	   //memset(yuv420p, 0x0, width*height);
	   //memset(yuv420p+width*height, 0x0, width*height/4);
	   //memset(yuv420p+width*height*5/4, 0x0, width*height/4);
	   close(fd);
   }
#endif
   // 1. Generate a texture object for y
   glGenTextures ( 3, userData->textureId );
   // Bind the texture object
   glBindTexture ( GL_TEXTURE_2D, userData->textureId[0] );
   // Set the filtering mode
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

   // Bind the texture object
   glBindTexture ( GL_TEXTURE_2D, userData->textureId[1] );
   // Set the filtering mode
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

   // Bind the texture object
   glBindTexture ( GL_TEXTURE_2D, userData->textureId[2] );
   // Set the filtering mode
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

   printf("gen texture id list: %d %d %d \n", userData->textureId[0], userData->textureId[1], userData->textureId[2]);
   return 0;
}


///
// Initialize the shader and program object
//
int Init ( ESContext *esContext )
{
   UserData *userData = esContext->userData;
   int ret;

   char vShaderStr[] =
      "#version 300 es                            \n"
      "layout(location = 0) in vec4 a_position;   \n"
      "layout(location = 1) in vec2 a_texCoord;   \n"
      "out vec2 v_texCoord;                       \n"
      "void main()                                \n"
      "{                                          \n"
      "   gl_Position = a_position;               \n"
      "   v_texCoord = a_texCoord;                \n"
      "}                                          \n";
   /* shader convert yuv420p to nv12 */
   char fShaderStr[] =
      "#version 300 es                                     \n"
      "precision mediump float;                            \n"
      "in vec2 v_texCoord;                                 \n"
      "uniform sampler2D tex_y;                            \n"
      "uniform sampler2D tex_u;                            \n"
      "uniform sampler2D tex_v;                            \n"
      "out vec4 outColor;                  \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  vec3 YUV;                                         \n"
      "  vec3 RGB;                                         \n"
      "  YUV.x = texture(tex_y, v_texCoord).r; \n"
      "  YUV.y = texture(tex_u, v_texCoord).r - 0.5;\n"
      "  YUV.z = texture(tex_v, v_texCoord).r - 0.5;\n"
      "  RGB = mat3(1.0, 1.0, 1.0,                 \n"
      "      0.0, -0.21482, 2.12798,                       \n"
      "      1.28033, -0.38059, 0.0) * YUV;                \n"
      /*
      "  RGB = mat3(0.0, 0.0, 0.0,                 \n"
      "      0.0, 0.0, 0.0,                        \n"
      "      1.0, 1.0, 1.0) * YUV;                 \n"
      */
      "  outColor = vec4(RGB, 1.0);                        \n"
      "}                                                   \n";

   // Load the shaders and get a linked program object
   userData->programObject = esLoadProgram ( vShaderStr, fShaderStr );

   // Get the sampler location
   userData->texture_y = glGetUniformLocation ( userData->programObject, "tex_y" );
   userData->texture_u = glGetUniformLocation ( userData->programObject, "tex_u" );
   userData->texture_v = glGetUniformLocation ( userData->programObject, "tex_v" );

   printf("uniform loc: %d-%d-%d\n", userData->texture_y, userData->texture_u, userData->texture_v);

   // Load the texture
   ret = CreateSimpleTexture2D (userData);
   if (ret != 0) {
   	return FALSE;
   }

   glClearColor ( 1.0f, 1.0f, 1.0f, 0.0f );
   return TRUE;
}

int init_ffmpeg(char *name)
{
	// ffmpeg init
	av_register_all();
	avformat_network_init();
	
	pFormatCtx = avformat_alloc_context();
 
	if(avformat_open_input(&pFormatCtx,name,NULL,NULL)!=0){
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
	av_dump_format(pFormatCtx,0,name,0);
	printf("-------------------------------------------------\n");

	raw_nv12 = calloc(1, pCodecCtx->width * pCodecCtx->height / 2 * 3);

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
	printf("codec ctx, height: %d, width: %d\n", pCodecCtx->height, pCodecCtx->width);
	return 0;
}

///
// Draw a triangle using the shader pair created in Init()
//
void Draw ( ESContext *esContext )
{
   UserData *userData = esContext->userData;
   GLfloat vVertices[] = { -1.0f,  -1.0f, 0.0f,  // Position 0
                            0.0f,  1.0f,        // TexCoord 0 
                           1.0f, -1.0f, 0.0f,  // Position 1
                            1.0f,  1.0f,        // TexCoord 1
                            -1.0f, 1.0f, 0.0f,  // Position 2
                            0.0f,  0.0f,        // TexCoord 2
                            1.0f,  1.0f, 0.0f,  // Position 3
                            1.0f,  0.0f         // TexCoord 3
                         };
   GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
   //

   // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );

   // Clear the color buffer
   glClear ( GL_COLOR_BUFFER_BIT );

   // Use the program object
   glUseProgram ( userData->programObject );

   // Load the vertex position
   glVertexAttribPointer ( 0, 3, GL_FLOAT,
                           GL_FALSE, 5 * sizeof ( GLfloat ), vVertices );
   // Load the texture coordinate
   glVertexAttribPointer ( 1, 2, GL_FLOAT,
                           GL_FALSE, 5 * sizeof ( GLfloat ), &vVertices[3] );

   glEnableVertexAttribArray ( 0 );
   glEnableVertexAttribArray ( 1 );

   // Bind the texture
   glActiveTexture ( GL_TEXTURE0 );
   glBindTexture ( GL_TEXTURE_2D, userData->textureId[0] );
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, yuv420p);
   glUniform1i(userData->texture_y, 0);

   glActiveTexture ( GL_TEXTURE0+1 );
   glBindTexture ( GL_TEXTURE_2D, userData->textureId[1] );
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, yuv420p+width*height);
   glUniform1i(userData->texture_u, 1);

   glActiveTexture ( GL_TEXTURE0+2 );
   glBindTexture ( GL_TEXTURE_2D, userData->textureId[2] );
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, yuv420p+width*height*5/4);
   glUniform1i(userData->texture_v, 2);

   //glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
   glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

///
// Cleanup
//
void ShutDown ( ESContext *esContext )
{
	if (img_convert_ctx != NULL) {
		sws_freeContext(img_convert_ctx);
	}
	if (raw_nv12 != NULL) {
		raw_nv12 = NULL;
	}
	av_free(pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	UserData *userData = esContext->userData;

	// Delete texture object
	for (int i=0; i<sizeof(userData->textureId)/sizeof(GLuint); i++) {
		glDeleteTextures ( 1, &userData->textureId[i] );
	}

	// Delete program object
	glDeleteProgram ( userData->programObject );
}

void UpdateFunc(ESContext *es, float deltaTime)
{
	int got_frame = 0;
	while(1){
		if (av_read_frame(pFormatCtx, packet) <0 ) {
			int stream_index = av_find_default_stream_index(pFormatCtx);
			//Convert ts to frame

			//SEEK
			if (avformat_seek_file(pFormatCtx, stream_index, INT64_MIN, 0, 0, 0) < 0) {
			    av_log(NULL, AV_LOG_ERROR, "ERROR av_seek_frame: %u\n", 0);
			} else {
			    av_log(NULL, AV_LOG_ERROR, "SUCCEEDED av_seek_frame: %u newPos:%d\n", 0, pFormatCtx->pb->pos);
			    avcodec_flush_buffers(pFormatCtx->streams[stream_index]->codec);
			}
			continue;
		}
		if(packet->stream_index==videoindex){
			//Decode
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if(ret < 0){
				printf("Decode Error.\n");
				return -1;
			}
			if(got_picture){
				#if 1 
				outbuf[0] = raw_nv12;
				outbuf[1] = (unsigned char *)(raw_nv12) + pCodecCtx->width * pCodecCtx->height;
				outbuf[2] = (unsigned char *)(raw_nv12) + (pCodecCtx->width * pCodecCtx->height)*5/4;
				outbuf[3] = 0;

				outlinesize[0] = pCodecCtx->width;
				outlinesize[1] = pCodecCtx->width/2;
				outlinesize[2] = pCodecCtx->width/2;
				outlinesize[3] = 0;
				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, outbuf, outlinesize);
				yuv420p = raw_nv12;
				#else
				yuv420p = pFrame->data;
				#endif
				Draw(es);
				got_frame = 1;

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
		av_free_packet(packet);
		if ((need_quit == true) || (got_frame == 1)) {
			break;
		}
	}
out:
	return 0;
}


int esMain ( ESContext *esContext )
{
   esContext->userData = malloc ( sizeof ( UserData ) );

   init_ffmpeg(video_filename);
   esCreateWindow ( esContext, "Simple Texture 2D", width, height, ES_WINDOW_RGB );

   if ( !Init ( esContext ) )
   {
      return GL_FALSE;
   }

   //esRegisterDrawFunc ( esContext, Draw );
   esRegisterUpdateFunc ( esContext, UpdateFunc );
   esRegisterShutdownFunc ( esContext, ShutDown );

   return GL_TRUE;
}

void when_sigint()  
{
	printf("User quit, set quit flag\n");
	need_quit = true;
	//exit(0);  
}  
