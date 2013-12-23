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

#include "videodev2_s3c.h"
#include "videodev2.h"
#include "SsbSipH264Encode.h"
#include "SsbSipMpeg4Encode.h"
#include "LogMsg.h"
#include "performance.h"
#include "lcd.h"
#include "post.h"
#include "JPGApi.h"
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

//static int  LCD_WIDTH= 	640;
//static int  LCD_HEIGHT= 480;

#define LCD_WIDTH_MAX 	1024
#define LCD_HEIGHT_MAX	768

//#define YUV_FRAME_BUFFER_SIZE	(LCD_WIDTH*LCD_HEIGHT)+(LCD_WIDTH*LCD_HEIGHT)/2		/* YCBCR 420 */
//#define YUV422_FRAME_BUFFER_SIZE (LCD_WIDTH*LCD_HEIGHT*2)

#define YUV_FRAME_BUFFER_SIZE_MAX	(LCD_WIDTH_MAX*LCD_HEIGHT_MAX)+(LCD_WIDTH_MAX*LCD_HEIGHT_MAX)/2		/* YCBCR 420 */
#define YUV422_FRAME_BUFFER_SIZE_MAX (LCD_WIDTH_MAX*LCD_HEIGHT_MAX*2)



static int g_LCD_Width =	0;
static int g_LCD_Height=	0;
static int g_YUV_Frame_Buffer_Size=  0;
static int g_YUV422_Frame_Buffer_Size=  0;
static int g_Thumbnail_Width =0;
static int g_Thumbnail_Height =0;

static char *win0_fb_addr = NULL;
static int pre_fb_fd = -1;

/* Frame buffer functions */
static int fb_init(int win_num, int bpp, int x, int y, int width, int height, unsigned int *addr);
static void draw(char *dest, char *src, int width, int height, int bpp);


// preview and codec
static int g_preview_X = 0;
static int g_preview_Y = 0;
static int g_preview_Width = 0;
static int g_preview_Height = 0;
static int g_codec_Width=0;
static int g_codec_Height=0;

/***************** JPEG *****************/
static void makeExifParam(ExifFileInfo *exifFileInfo);

 

/***************** etc *******************/
#define TRUE  1
#define FALSE 0
#define MFC_LINE_BUF_SIZE_PER_INSTANCE		(204800)
#define YUV_FRAME_NUM						100



/**************** fimc driver need  dg add 2011-12-31 *************************/
#define V4L2_FMT_IN			0
#define V4L2_FMT_OUT			1


static int		film_cnt;
static int		finished;
static int		cap_flag;

static pthread_t 	pth;
static FILE *jpg_fp = NULL;
static void capture_thread(void);
static void capture();


static void exit_from_app() 
{
	int start;
        int fb_size_preview;
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
                        //fb_size = LCD_WIDTH * LCD_HEIGHT * 2;
                        fb_size_preview = g_preview_Width * g_preview_Height* 2;
			break;
		case 24:
                        //fb_size = LCD_WIDTH * LCD_HEIGHT * 4;
                        fb_size_preview = g_preview_Width * g_preview_Height* 4;
			break;
		default:
                        //fb_size = LCD_WIDTH * LCD_HEIGHT * 4;
                        fb_size_preview = g_preview_Width * g_preview_Height* 4;
			printf("LCD supports 16 or 24 bpp\n");
			break;
	}


	close(cam_p_fp);
	close(cam_c_fp);

        munmap(win0_fb_addr, fb_size_preview);
	close(pre_fb_fd);
	
//	exit(0);
}


static void sig_del(int signo)
{
	int start, ret;

	
	start = 0;
	ret = ioctl(cam_c_fp, VIDIOC_STREAMOFF, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_STREAMOFF failed\n");
		exit(1);
	}
	
	exit_from_app();
}


static void signal_ctrl_c(void)
{
	if (signal(SIGINT, sig_del) == SIG_ERR)
		printf("Signal Error\n");
}



/* Main Process(Camera previewing) */
int Forlinx_Test_Capture(int argc, char **argv, int lcdnum)
{

	int ret, start, found = 0;
	int k_id;
	unsigned int addr = 0;

	struct v4l2_capability cap;
	struct v4l2_input chan;
	struct v4l2_framebuffer preview;
	struct v4l2_pix_format preview_fmt;
	struct v4l2_format codec_fmt;

	pp_params	pp_param;
	int			pp_fd;

	

        g_LCD_Width = lcdsize[lcdnum][0];
        g_LCD_Height =lcdsize[lcdnum][1];


        g_preview_X = 0;
        g_preview_Y = 0;


        g_preview_Width = g_LCD_Width;
        g_preview_Height = g_LCD_Height;



        g_codec_Width=g_preview_Width;
        g_codec_Height=g_preview_Height;


        char rgb_for_preview[LCD_WIDTH_MAX * LCD_HEIGHT_MAX * 4];	// MAX
        g_YUV_Frame_Buffer_Size = (g_codec_Width*g_codec_Height)+(g_codec_Width*g_codec_Height)/2;		/* YCBCR 420 */
        g_YUV422_Frame_Buffer_Size = (g_codec_Width*g_codec_Height)*2;		/* YUV 422 */


         //g_Thumbnail_Width,g_Thumbnail_Height need:
        // 1  g_Thumbnail_Width * g_Thumbnail_Height  MAX  320*240
        // 2  g_Thumbnail_Width%16==0 and g_Thumbnail_Height%8==0;
        // 3  g_LCD_Width%g_Thumbnail_Width==0 and  g_LCD_Height%g_Thumbnail_Height==0

        if(g_LCD_Width==1024 && g_LCD_Height==768)
        {
          g_Thumbnail_Width=64;
          g_Thumbnail_Height=32;
        }
        else if(g_LCD_Width==800 && g_LCD_Height==600)
        {

            g_Thumbnail_Width=80;
            g_Thumbnail_Height=40;

        }else  if(g_LCD_Width==800 && g_LCD_Height==480)
        {
            g_Thumbnail_Width=80;
            g_Thumbnail_Height=40;

        }else
        {
            g_Thumbnail_Width= g_LCD_Width/2;
            g_Thumbnail_Height=g_LCD_Height/2;

        }

	/* Camera preview initialization */
	if ((cam_p_fp = cam_p_init()) < 0)
		exit_from_app();

	/* Camera codec initialization */
	if ((cam_c_fp = cam_c_init()) < 0)
		exit_from_app();

	/* Window0 initialzation for previewing */
        //if ((pre_fb_fd = fb_init(0, LCD_BPP, 0, 0, LCD_WIDTH, LCD_HEIGHT, &addr)) < 0)
                //exit_from_app();

        if ((pre_fb_fd = fb_init(0, LCD_BPP, g_preview_X, g_preview_Y, g_preview_Width, g_preview_Height, &addr)) < 0)
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

        preview_fmt.width = g_preview_Width;//LCD_WIDTH;
        preview_fmt.height = g_preview_Height;//LCD_HEIGHT;
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
        codec_fmt.fmt.pix.width = g_codec_Width;//LCD_WIDTH;
        codec_fmt.fmt.pix.height =g_codec_Height; //  LCD_HEIGHT;
        codec_fmt.fmt.pix.pixelformat= V4L2_PIX_FMT_YUYV; //YUV422
        codec_fmt.fmt.pix.priv= V4L2_FMT_OUT;  // must set

	ret = ioctl(cam_c_fp , VIDIOC_S_FMT, &codec_fmt);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_S_FMT failled\n");
		exit(1);
	}

	printf("\n[11. Camera input & JPEG encoding]\n");
	printf("Forlinx Embedded, %s\n", VERSION);
	printf("Using IP            : Post processor, LCD, Camera, JPEG\n");

       // printf("Camera preview size : VGA(640x480)\n");
        printf("Camera preview size : %dx%d\n",g_preview_Width,g_preview_Height);
        printf("Capture size        : %dx%d\n",g_codec_Width,g_codec_Height);
        printf("Display size        : %dx%d\n",g_LCD_Width,g_LCD_Height);

	/* Encoding and decoding threads creation */
	k_id = pthread_create(&pth, 0, (void *) &capture_thread, 0);

	
	while (1) {
		if (finished)
			break;
		
		/* Get RGB frame from camera preview */
                //if (!read_data(cam_p_fp, &rgb_for_preview[0], LCD_WIDTH, LCD_HEIGHT, LCD_BPP)) {
                if (!read_data(cam_p_fp, &rgb_for_preview[0], g_preview_Width, g_preview_Height, LCD_BPP)) {
                    printf("V4L2 : read_data() failed\n");
			break;
		}

		/* Write RGB frame to LCD frame buffer */
                //draw(win0_fb_addr, &rgb_for_preview[0], LCD_WIDTH, LCD_HEIGHT, LCD_BPP);
                draw(win0_fb_addr, &rgb_for_preview[0],g_preview_Width,g_preview_Height, LCD_BPP);
	}

	pthread_join(pth, NULL);

	finished = 0;
	
	exit_from_app();

	return 0;
}

/***************** Capture Thread *****************/
void capture_thread(void)
{
	int start, ret;
	int key;
	
	start = 1;
	ret = ioctl(cam_c_fp, VIDIOC_STREAMON, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_STREAMON failed\n");
		exit(1);
	}

	printf("\nc : Capture\n");
	printf("x : Exit\n");
	printf("Select ==> ");	
	
	while (1) { 
		fflush(stdin);
		key = getchar();

		if(key == 'c') {
			cap_flag = TRUE;	
		}
		else if(key == 'x'){
			finished = 1;
			pthread_exit(0);
		}

		if (cap_flag == TRUE) {
			capture();
			cap_flag = FALSE;
		}
	}

	start = 0;
	ret = ioctl(cam_c_fp, VIDIOC_STREAMOFF, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_STREAMOFF failed\n");
		exit(1);
	}

	pthread_exit(0);
}


static void capture()
{
	char file_name[100];
	int ret;
        unsigned char	g_yuv[YUV422_FRAME_BUFFER_SIZE_MAX];
	int	jpg_handle;
	char	*in_buf, *out_buf;
	ExifFileInfo	ExifInfo;
	long 			jpg_size;
        FILE *yuv_fp = NULL;

        //sprintf(&file_name[0], "Cam_capture_%dx%d-%d.jpg", LCD_WIDTH, LCD_HEIGHT, ++film_cnt);
        //printf("Name of photo file : Cam_capture_%dx%d-%d.jpg\n", LCD_WIDTH, LCD_HEIGHT, film_cnt);

        sprintf(&file_name[0], "Cam_capture_%dx%d-%d.jpg", g_codec_Width, g_codec_Height, ++film_cnt);
        printf("Name of photo file : Cam_capture_%dx%d-%d.jpg\n", g_codec_Width, g_codec_Height, film_cnt);

	/* file create/open, note to "wb" */
	jpg_fp = fopen(&file_name[0], "wb");
	if (!jpg_fp) {
		perror(&file_name[0]);
	}

	/* read from camera device */
        if (read(cam_c_fp, g_yuv, g_YUV422_Frame_Buffer_Size) < 0) {
		perror("read()");
	}

#if 0 //phantom test
	yuv_fp = fopen("yuvfile", "wb");
        fwrite(g_yuv, g_YUV422_Frame_Buffer_Size, 1, yuv_fp);

	fclose(yuv_fp);

#endif



	
	/* JPEG encoding */
	/* JPEG Handle initialization */
	jpg_handle = SsbSipJPEGEncodeInit();
	if (jpg_handle < 0)
		return;
 
	/* Set encoding configs */
	if((ret = SsbSipJPEGSetConfig(JPEG_SET_SAMPING_MODE, JPG_422)) != JPEG_OK)
		return;
	
        if((ret = SsbSipJPEGSetConfig(JPEG_SET_ENCODE_WIDTH, g_codec_Width)) != JPEG_OK)
		return;
		
        if((ret = SsbSipJPEGSetConfig(JPEG_SET_ENCODE_HEIGHT, g_codec_Height)) != JPEG_OK)
		return;
		
	if((ret = SsbSipJPEGSetConfig(JPEG_SET_ENCODE_QUALITY, JPG_QUALITY_LEVEL_2)) != JPEG_OK)
		return;
	
	if((ret = SsbSipJPEGSetConfig(JPEG_SET_ENCODE_THUMBNAIL, TRUE)) != JPEG_OK)
		return;
	
        //if((ret = SsbSipJPEGSetConfig(JPEG_SET_THUMBNAIL_WIDTH, 160)) != JPEG_OK)
        //	return;

        //   Main JPEG have to be multiple of Thumbnail resolution
        if((ret = SsbSipJPEGSetConfig(JPEG_SET_THUMBNAIL_WIDTH, g_Thumbnail_Width)) != JPEG_OK)
                return;


        //if((ret = SsbSipJPEGSetConfig(JPEG_SET_THUMBNAIL_HEIGHT, 120)) != JPEG_OK)
                //return;

       //   Main JPEG have to be multiple of Thumbnail resolution
        if((ret = SsbSipJPEGSetConfig(JPEG_SET_THUMBNAIL_HEIGHT, g_Thumbnail_Height)) != JPEG_OK)
                return;

 
	/* Get input buffer address */
        in_buf = SsbSipJPEGGetEncodeInBuf(jpg_handle, g_YUV422_Frame_Buffer_Size);
	if(in_buf == NULL)
		return;

	/* Copy YUV data from camera to JPEG driver */
        memcpy(in_buf, g_yuv, g_YUV422_Frame_Buffer_Size);

	/* Make Exif info parameters */
	memset(&ExifInfo, 0x00, sizeof(ExifFileInfo));
	makeExifParam(&ExifInfo);

	/* Encode YUV stream */
	ret = SsbSipJPEGEncodeExe(jpg_handle, &ExifInfo, JPEG_USE_HW_SCALER);    //with Exif
	
	/* Get output buffer address */
	out_buf = SsbSipJPEGGetEncodeOutBuf(jpg_handle, &jpg_size);
	if(out_buf == NULL)
		return;

	
	fwrite(out_buf, 1, jpg_size, jpg_fp);

	fclose(jpg_fp);
	SsbSipJPEGEncodeDeInit(jpg_handle);

	printf("CAPTURE SUCCESS\n");
	printf("\nSelect ==> ");
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
        //int end_x = MIN(LCD_WIDTH, width);
        int end_x = MIN(g_preview_Width, width);


	if (bpp == 16) {

#if !defined(LCD_24BIT)
		for (y = 0; y < end_y; y++) {
                        //memcpy(dest + y * LCD_WIDTH * 2, src + y * width * 2, end_x * 2);
                        memcpy(dest + y * g_preview_Width * 2, src + y * width * 2, end_x * 2);

		}
#else
		for (y = 0; y < end_y; y++) {
			rgb16 = (unsigned short *) (src + (y * width * 2));
                        //rgb32 = (unsigned long *) (dest + (y * LCD_WIDTH * 2));
                        rgb32 = (unsigned long *) (dest + (y * g_preview_Width* 2));


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
                         rgb16 = (unsigned short *) (dest + (y * g_preview_Width* 2));

			// 24 bit RGB data -> 16 bit RGB data 
			for (x = 0; x < end_x; x++) {
				*rgb16 = ( (*rgb32 >> 8) & 0xF800 ) | ( (*rgb32 >> 5) & 0x07E0 ) | ( (*rgb32 >> 3) & 0x001F );
				rgb32++;
				rgb16++;
			}
		}
#else
		for (y = 0; y < end_y; y++) {
                        //memcpy(dest + y * LCD_WIDTH * 4, src + y * width * 4, end_x * 4);
                          memcpy(dest + y * g_preview_Width * 4, src + y * width * 4, end_x * 4);
		}
#endif
	}
}


/***************** JPEG function *****************/
static void makeExifParam(ExifFileInfo *exifFileInfo)
{
	strcpy(exifFileInfo->Make,"Samsung SYS.LSI make");;
	strcpy(exifFileInfo->Model,"Samsung 2007 model");
	strcpy(exifFileInfo->Version,"version 1.0.2.0");
	strcpy(exifFileInfo->DateTime,"2007:05:16 12:32:54");
	strcpy(exifFileInfo->CopyRight,"Samsung Electronics@2007:All rights reserved");

	exifFileInfo->Height					= 320;
	exifFileInfo->Width						= 240;
	exifFileInfo->Orientation				= 1; // top-left
	exifFileInfo->ColorSpace				= 1;
	exifFileInfo->Process					= 1;
	exifFileInfo->Flash						= 0;
	exifFileInfo->FocalLengthNum			= 1;
	exifFileInfo->FocalLengthDen			= 4;
	exifFileInfo->ExposureTimeNum			= 1;
	exifFileInfo->ExposureTimeDen			= 20;
	exifFileInfo->FNumberNum				= 1;
	exifFileInfo->FNumberDen				= 35;
	exifFileInfo->ApertureFNumber			= 1;
	exifFileInfo->SubjectDistanceNum		= -20;
	exifFileInfo->SubjectDistanceDen		= -7;
	exifFileInfo->CCDWidth					= 1;
	exifFileInfo->ExposureBiasNum			= -16;
	exifFileInfo->ExposureBiasDen			= -2;
	exifFileInfo->WhiteBalance				= 6;
	exifFileInfo->MeteringMode				= 3;
	exifFileInfo->ExposureProgram			= 1;
	exifFileInfo->ISOSpeedRatings[0]		= 1;
	exifFileInfo->ISOSpeedRatings[1]		= 2;
	exifFileInfo->FocalPlaneXResolutionNum	= 65;
	exifFileInfo->FocalPlaneXResolutionDen	= 66;
	exifFileInfo->FocalPlaneYResolutionNum	= 70;
	exifFileInfo->FocalPlaneYResolutionDen	= 71;
	exifFileInfo->FocalPlaneResolutionUnit	= 3;
	exifFileInfo->XResolutionNum			= 48;
	exifFileInfo->XResolutionDen			= 20;
	exifFileInfo->YResolutionNum			= 48;
	exifFileInfo->YResolutionDen			= 20;
	exifFileInfo->RUnit						= 2;
	exifFileInfo->BrightnessNum				= -7;
	exifFileInfo->BrightnessDen				= 1;

	strcpy(exifFileInfo->UserComments,"Usercomments");
}
