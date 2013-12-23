#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <semaphore.h>

#include "SsbSipH264Encode.h"
#include "SsbSipH264Decode.h"
#include "SsbSipMpeg4Decode.h"
#include "SsbSipVC1Decode.h"
#include "FrameExtractor.h"
#include "MPEG4Frames.h"
#include "H263Frames.h"
#include "H264Frames.h"
#include "LogMsg.h"
#include "performance.h"
#include "post.h"
#include "lcd.h"
#include "MfcDriver.h"
#include "FileRead.h"
#include "cam_encoder_test.h"


#define SAMSUNG_UXGA_S5K3BA


/******************* CAMERA ********************/
#ifdef RGB24BPP
	#define LCD_24BIT		/* For s3c2443/6400 24bit LCD interface */
	#define LCD_BPP_V4L2		V4L2_PIX_FMT_RGB24
#else
	#define LCD_BPP_V4L2		V4L2_PIX_FMT_RGB565	/* 16 BPP - RGB565 */
#endif

#define PREVIEW_NODE  "/dev/video1"
#define CODEC_NODE  "/dev/video0"
static int cam_p_fp = -1;
static int cam_c_fp = -1;

/* Camera functions */
static int cam_p_init(void);
static int cam_c_init(void);
static int read_data(int fp, char *buf, int width, int height, int bpp);



/************* FRAME BUFFER ***************/
#ifdef RGB24BPP
	#define LCD_BPP	24	/* 24 BPP - RGB888 */
#else
	#define LCD_BPP	16	/* 16 BPP - RGB565 */
#endif
static int lcdsize[6][2] = {{320,240}, {480,272}, {640,480}, {800,480}, {800,600},{1024,768}};



#define LCD_WIDTH_MAX 	1024
#define LCD_HEIGHT_MAX	768

#define  YUV_FRAME_BUFFER_SIZE_MAX   (LCD_WIDTH_MAX*LCD_HEIGHT_MAX)+(LCD_WIDTH_MAX*LCD_HEIGHT_MAX)/2		/* YCBCR 420 */


static int g_LCD_Width =	0;
static int g_LCD_Height=	0;
static int g_YUV_Frame_Buffer_Size=     0;

static char *win0_fb_addr = NULL;
static int pre_fb_fd = -1;

/* Frame buffer functions */
static int fb_init(int win_num, int bpp, int x, int y, int width, int height, unsigned int *addr);
static void draw(char *dest, char *src, int width, int height, int bpp);

// codec
static int codec_Width=0;
static int codec_Height=0;

/***************** MFC *******************/
static void 	*handle; 

/* MFC functions */
static void *mfc_encoder_init(int width, int height, int frame_rate, int bitrate, int gop_num);
static void *mfc_encoder_exe(void *handle, unsigned char *yuv_buf, int frame_size, int first_frame, long *size);
static void mfc_encoder_free(void *handle);
 


/***************** etc *******************/
#define TRUE  1
#define FALSE 0
#define SHARED_BUF_NUM						100
#define MFC_LINE_BUF_SIZE_PER_INSTANCE		(204800)
#define YUV_FRAME_NUM						100

/**************** fimc driver need  dg add 2011-12-31 *************************/
#define V4L2_FMT_IN			0
#define V4L2_FMT_OUT                    1

static int		film_cnt;
static int		frame_count;
static int 		encoding_flag = FALSE;
static int 		finished;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t 	pth;
static void encoding_thread(void);
static FILE *encoded_fp;






static void exit_from_app() 
{
	int start;
	int fb_size;
	int ret;


	ioctl(pre_fb_fd, SET_OSD_STOP);
	
	/* Stop previewing */
	start = 0;
	ret = ioctl(cam_p_fp, VIDIOC_OVERLAY, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_OVERLAY failed\n");
		exit(1);
	}

	switch(LCD_BPP)
	{
		case 16:
                        fb_size = g_LCD_Width * g_LCD_Height * 2;
			break;
		case 24:
                        fb_size = g_LCD_Width * g_LCD_Height* 4;
			break;
		default:
                        fb_size = g_LCD_Width * g_LCD_Height * 4;
			printf("LCD supports 16 or 24 bpp\n");
			break;
	}

	close(cam_p_fp);
	close(cam_c_fp);

	mfc_encoder_free(handle);

	munmap(win0_fb_addr, fb_size);
	close(pre_fb_fd);
}


static void sig_del(int signo)
{
	exit_from_app();
}


static void signal_ctrl_c(void)
{
	if (signal(SIGINT, sig_del) == SIG_ERR)
		printf("Signal Error\n");
}



/* Main Process(Camera previewing) */
int Forlinx_Test_Cam_Encoding(int argc, char **argv, int lcdnum)
{


	int ret, start, found = 0;
	int k_id ; 
	unsigned int addr = 0;
        char rgb_for_preview[LCD_WIDTH_MAX * LCD_HEIGHT_MAX * 4];	// MAX

	struct v4l2_capability cap;
	struct v4l2_input chan;
	struct v4l2_framebuffer preview;
	struct v4l2_pix_format preview_fmt;
	struct v4l2_format codec_fmt;

	pp_params	pp_param;
	int			pp_fd;

        g_LCD_Width = lcdsize[lcdnum][0];
        g_LCD_Height =lcdsize[lcdnum][1];

        // source picture width and height must be a multiple of 16 in codecing
         codec_Width=g_LCD_Width/16*16;
         codec_Height=g_LCD_Height/16*16;

         g_YUV_Frame_Buffer_Size = (codec_Width*codec_Height)+(codec_Width*codec_Height)/2;


	/* Camera preview initialization */
	if ((cam_p_fp = cam_p_init()) < 0)
		exit_from_app();



	/* Camera codec initialization */
	if ((cam_c_fp = cam_c_init()) < 0)
		exit_from_app();

	/* Window0 initialzation for previewing */
        if ((pre_fb_fd = fb_init(0, LCD_BPP, 0, 0, g_LCD_Width,g_LCD_Height, &addr)) < 0)
		exit_from_app();
	
	win0_fb_addr = (char *)addr;

	pp_fd = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd < 0)
	{
		printf("Post processor open error\n");
		return -1;
	}
	memset(&pp_param, 0, sizeof(pp_params));
	pp_param.OutPath = POST_DMA;
	ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);
	
	signal_ctrl_c();

#if 1
	/* Get capability */
	ret = ioctl(cam_p_fp , VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_QUERYCAP failled\n");
		exit(1);
	}
	//printf("V4L2 : Name of the interface is %s\n", cap.driver);


	/* Check the type - preview(OVERLAY) */
	if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY)) {
		printf("V4L2 : Can not capture(V4L2_CAP_VIDEO_OVERLAY is false)\n");
		exit(1);
	}

	chan.index = 0;
	found = 0;
	while(1) {
		ret = ioctl(cam_p_fp, VIDIOC_ENUMINPUT, &chan);
		if (ret < 0) {
			printf("V4L2 : ioctl on VIDIOC_ENUMINPUT failled\n");
			break;
		}

		//printf("[%d] : Name of this channel is %s\n", chan.index, chan.name);

		/* Test channel.type */
		if (chan.type & V4L2_INPUT_TYPE_CAMERA ) {
			//printf("V4L2 : Camera Input(V4L2_INPUT_TYPE_CAMERA )\n");
			found = 1;
			break;
		}
		chan.index++;
	}	
	if(!found) 
		exit_from_app();

	/*  Settings for input channel 0 which is channel of webcam */
	chan.type = V4L2_INPUT_TYPE_CAMERA;
	ret = ioctl(cam_p_fp, VIDIOC_S_INPUT, &chan);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_S_INPUT failed\n");
		exit(1);
	}

        preview_fmt.width = g_LCD_Width;
        preview_fmt.height =g_LCD_Height;
	preview_fmt.pixelformat = LCD_BPP_V4L2;

	preview.capability = 0;
	preview.flags = 0;
	preview.fmt = preview_fmt;

	/* Set up for preview */
	ret = ioctl(cam_p_fp, VIDIOC_S_FBUF, &preview);
	if (ret< 0) {
		printf("V4L2 : ioctl on VIDIOC_S_BUF failed\n");
		exit(1);
	}

	/* Preview start */
	start = 1;
	ret = ioctl(cam_p_fp, VIDIOC_OVERLAY, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_OVERLAY failed\n");
		exit(1);
	}
#endif



#if 1
	/* Codec set */
	/* Get capability */
	ret = ioctl(cam_c_fp , VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_QUERYCAP failled\n");
		exit(1);
	}
	//printf("V4L2 : Name of the interface is %s\n", cap.driver);


	/* Check the type - preview(OVERLAY) */
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		printf("V4L2 : Can not capture(V4L2_CAP_VIDEO_CAPTURE is false)\n");
		exit(1);
	}

	/* Set format */
	codec_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        codec_fmt.fmt.pix.width = codec_Width;//LCD_WIDTH;
        codec_fmt.fmt.pix.height = codec_Height; //LCD_HEIGHT;
	codec_fmt.fmt.pix.pixelformat= V4L2_PIX_FMT_YUV420; 
        codec_fmt.fmt.pix.priv= V4L2_FMT_OUT;  // must set

	ret = ioctl(cam_c_fp , VIDIOC_S_FMT, &codec_fmt);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_S_FMT failled\n");
		exit(1);
	}
#endif
	printf("\n[8. Camera preview & MFC encoding]\n");
	printf("Forlinx Embedded, %s\n", VERSION);
	printf("Using IP          : MFC, Post processor, LCD, Camera\n");
        printf("Display size      : (%dx%d)\n",g_LCD_Width, g_LCD_Height);


	/* Encoding and decoding threads creation */

	k_id = pthread_create(&pth, 0, (void *) &encoding_thread, 0);
	
	while (1) {
		if (finished)
			break;
#if 1
		/* Get RGB frame from camera preview */
                if (!read_data(cam_p_fp, &rgb_for_preview[0], g_LCD_Width, g_LCD_Height, LCD_BPP)) {
			printf("V4L2 : read_data() failed\n");
			break;
		}

		/* Write RGB frame to LCD frame buffer */
                draw(win0_fb_addr, &rgb_for_preview[0], g_LCD_Width, g_LCD_Height, LCD_BPP);
#endif

	}

	pthread_join(pth, NULL);

	finished = 0;

	exit_from_app();

	return 0;
}

/***************** Encoding Thread *****************/
void encoding_thread(void)
{
	char file_name[100];
	int yuv_cnt = 0;
	int start, ret;
	int frame_num = YUV_FRAME_NUM;
	int key;
        unsigned char	g_yuv[YUV_FRAME_BUFFER_SIZE_MAX];
	unsigned char	*encoded_buf;
	long			encoded_size;


	printf("\ne : Encoding\n");
	printf("x : Exit\n");
	printf("Select ==> ");	

	while (1) {

		key = getchar();

		if(key == 'e')
			encoding_flag = TRUE;
		else if(key == 'x') {
			finished = 1;
 			pthread_exit(0);
		}
		
		if (encoding_flag == TRUE) {

			pthread_mutex_lock(&mutex);
			
                        //handle = mfc_encoder_init(LCD_WIDTH, LCD_HEIGHT, 30, 1000, 30);
                        handle = mfc_encoder_init(codec_Width, codec_Height, 30, 1000, 30);

                        //sprintf(&file_name[0], "Cam_encoding_%dx%d-%d.264", LCD_WIDTH, LCD_HEIGHT, ++film_cnt);
                         sprintf(&file_name[0], "Cam_encoding_%dx%d-%d.264", codec_Width, codec_Height, ++film_cnt);
                       // printf("Name of encoded file : Cam_encoding_%dx%d-%d.264\n", LCD_WIDTH, LCD_HEIGHT, film_cnt);
                         printf("Name of encoded file : Cam_encoding_%dx%d-%d.264\n", codec_Width, codec_Height, film_cnt);

			fflush(stdout);

			/* file create/open, note to "wb" */
			encoded_fp = fopen(&file_name[0], "wb");
			if (!encoded_fp) {
                                perror(&file_name[0]);
			}

			/* Codec start */
			start = 1;
			ret = ioctl(cam_c_fp, VIDIOC_STREAMON, &start);
			if (ret < 0) {
				printf("V4L2 : ioctl on VIDIOC_STREAMON failed\n");
				exit(1);
			}

			for(yuv_cnt=0; yuv_cnt < frame_num; yuv_cnt++){
				frame_count++;

				/* read from camera device */
                                if (read(cam_c_fp, g_yuv, g_YUV_Frame_Buffer_Size) < 0) {
					perror("read()");
				}


				if(frame_count == 1)
                                        encoded_buf = mfc_encoder_exe(handle, g_yuv, g_YUV_Frame_Buffer_Size, 1, &encoded_size);
				else
                                        encoded_buf = mfc_encoder_exe(handle, g_yuv, g_YUV_Frame_Buffer_Size, 0, &encoded_size);

				fwrite(encoded_buf, 1, encoded_size, encoded_fp);
			}

			frame_count = 0;
			
			/* Codec stop */
			start = 0;
			ret = ioctl(cam_c_fp, VIDIOC_STREAMOFF, &start);
			if (ret < 0) {
				printf("V4L2 : ioctl on VIDIOC_STREAMOFF failed\n");
				exit(1);
			}

			printf("100 frames were encoded\n");
			printf("\nSelect ==> ");

			mfc_encoder_free(handle);

			fclose(encoded_fp);

			encoding_flag= FALSE;

			pthread_mutex_unlock(&mutex);
		}
	}
}



/***************** Camera driver function *****************/
static int cam_p_init(void) 
{
	int dev_fp = -1;

	dev_fp = open(PREVIEW_NODE, O_RDWR);

	if (dev_fp < 0) {
		perror(PREVIEW_NODE);
		return -1;
	}
	return dev_fp;
}

static int cam_c_init(void)
{
	int dev_fp = -1;

	dev_fp = open(CODEC_NODE, O_RDWR);

	if (dev_fp < 0) {
		perror(CODEC_NODE);
		printf("CODEC : Open Failed \n");
		return -1;
	}
	return dev_fp;
}

static int read_data(int fp, char *buf, int width, int height, int bpp)
{
	int ret;

	if (bpp == 16) {
		if ((ret = read(fp, buf, width * height * 2)) != width * height * 2) {
			return 0;
		}
	} else {
		if ((ret = read(fp, buf, width * height * 4)) != width * height * 4) {
			return 0;
		}
	}

	return ret;
}



/***************** Display driver function *****************/
static int fb_init(int win_num, int bpp, int x, int y, int width, int height, unsigned int *addr)
{
	int 			dev_fp = -1;
	int 			fb_size;
	s3c_win_info_t	fb_info_to_driver;


	switch(win_num)
	{
		case 0:
			dev_fp = open(FB_DEV_NAME, O_RDWR);
			break;
		case 1:
			dev_fp = open(FB_DEV_NAME1, O_RDWR);
			break;
		case 2:
			dev_fp = open(FB_DEV_NAME2, O_RDWR);
			break;
		case 3:
			dev_fp = open(FB_DEV_NAME3, O_RDWR);
			break;
		case 4:
			dev_fp = open(FB_DEV_NAME4, O_RDWR);
			break;
		default:
			printf("Window number is wrong\n");
			return -1;
	}
	if (dev_fp < 0) {
		perror(FB_DEV_NAME);
		return -1;
	}


	switch(bpp)
	{
		case 16:
			fb_size = width * height * 2;	
			break;
		case 24:
			fb_size = width * height * 4;
			break;
		default:
			printf("16 and 24 bpp support");
			return -1;
	}
		
	
	if ((*addr = (unsigned int) mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fp, 0)) < 0) {
		printf("mmap() error in fb_init()");
		return -1;
	}

	fb_info_to_driver.Bpp 		= bpp;
	fb_info_to_driver.LeftTop_x	= x;
	fb_info_to_driver.LeftTop_y	= y;
	fb_info_to_driver.Width 	= width;
	fb_info_to_driver.Height 	= height;

	if (ioctl(dev_fp, SET_OSD_INFO, &fb_info_to_driver)) {
		printf("Some problem with the ioctl SET_VS_INFO!!!\n");
		return -1;
	}


	if (ioctl(dev_fp, SET_OSD_START)) {
		printf("Some problem with the ioctl START_OSD!!!\n");
		return -1;
	}


	return dev_fp;
}

#define MIN(x,y) ((x)>(y)?(y):(x))
static void draw(char *dest, char *src, int width, int height, int bpp)
{
	int x, y;
	unsigned long *rgb32;
	unsigned short *rgb16;

	int end_y = height;
        int end_x = MIN(g_LCD_Width, width);

	if (bpp == 16) {

#if !defined(LCD_24BIT)
		for (y = 0; y < end_y; y++) {
                        memcpy(dest + y * g_LCD_Width * 2, src + y * width * 2, end_x * 2);
		}
#else
		for (y = 0; y < end_y; y++) {
			rgb16 = (unsigned short *) (src + (y * width * 2));
                        //rgb32 = (unsigned long *) (dest + (y * LCD_WIDTH * 2));
                        rgb32 = (unsigned long *) (dest + (y * g_LCD_Width * 2));




			// TO DO : 16 bit RGB data -> 24 bit RGB data
			for (x = 0; x < end_x; x++) {
				*rgb32 = ( ((*rgb16) & 0xF800) << 16 ) | ( ((*rgb16) & 0x07E0) << 8 ) |
					( (*rgb16) & 0x001F );
				rgb32++;
				rgb16++;
			}
		}

#endif
	} else if (bpp == 24) {
#if !defined(LCD_24BIT)
		for (y = 0; y < end_y; y++) {
			rgb32 = (unsigned long *) (src + (y * width * 4));
                        //rgb16 = (unsigned short *) (dest + (y * LCD_WIDTH * 2));
                        rgb16 = (unsigned short *) (dest + (y * g_LCD_Width * 2));

			// 24 bit RGB data -> 16 bit RGB data 
			for (x = 0; x < end_x; x++) {
				*rgb16 = ( (*rgb32 >> 8) & 0xF800 ) | ( (*rgb32 >> 5) & 0x07E0 ) | ( (*rgb32 >> 3) & 0x001F );
				rgb32++;
				rgb16++;
			}
		}
#else
		for (y = 0; y < end_y; y++) {
			memcpy(dest + y * LCD_WIDTH * 4, src + y * width * 4, end_x * 4);
		}
#endif
	}
}



/***************** MFC driver function *****************/
void *mfc_encoder_init(int width, int height, int frame_rate, int bitrate, int gop_num)
{
	int				frame_size;
	void			*handle;
	int				ret;


	frame_size	= (width * height * 3) >> 1;

	handle = SsbSipH264EncodeInit(width, height, frame_rate, bitrate, gop_num);
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_Encoder", "SsbSipH264EncodeInit Failed\n");
		return NULL;
	}

	ret = SsbSipH264EncodeExe(handle);

	return handle;
}

void *mfc_encoder_exe(void *handle, unsigned char *yuv_buf, int frame_size, int first_frame, long *size)
{
	unsigned char	*p_inbuf, *p_outbuf;
	int				hdr_size;
	int				ret;


	p_inbuf = SsbSipH264EncodeGetInBuf(handle, 0);

	memcpy(p_inbuf, yuv_buf, frame_size);

	ret = SsbSipH264EncodeExe(handle);
	if (first_frame) {
		SsbSipH264EncodeGetConfig(handle, H264_ENC_GETCONF_HEADER_SIZE, &hdr_size);
		//printf("Header Size : %d\n", hdr_size);
	}

	p_outbuf = SsbSipH264EncodeGetOutBuf(handle, size);

	return p_outbuf;
}

void mfc_encoder_free(void *handle)
{
	SsbSipH264EncodeDeInit(handle);
}

