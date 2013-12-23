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
#include <sys/stat.h>

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
#include "cam_enc_dec_test.h"


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


//static int  LCD_WIDTH= 	400;
//static int  LCD_HEIGHT=	480;

#define LCD_WIDTH_MAX 	1024
#define LCD_HEIGHT_MAX	768

#define YUV_FRAME_BUFFER_SIZE_MAX	(LCD_WIDTH_MAX*LCD_HEIGHT_MAX)+(LCD_WIDTH_MAX*LCD_HEIGHT_MAX)/2		/* YCBCR 420 */
//#define YUV_FRAME_BUFFER_SIZE	(LCD_WIDTH*LCD_HEIGHT)+(LCD_WIDTH*LCD_HEIGHT)/2		/* YCBCR 420 */


static int g_LCD_Width =	0;
static int g_LCD_Height=	0;
static int g_YUV_Frame_Buffer_Size=  0;

static char *win0_fb_addr = NULL;
static char	*win1_fb_addr = NULL;	
static int pre_fb_fd = -1;
static int dec_fb_fd = -1;

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



/***************** MFC *******************/
static void 	*enc_handle, *dec_handle;
static int 		enc_frame_cnt, dec_frame_cnt;

/* MFC functions */
static void *mfc_encoder_init(int width, int height, int frame_rate, int bitrate, int gop_num);
static void *mfc_encoder_exe(void *handle, unsigned char *yuv_buf, int frame_size, int first_frame, long *size);
static void mfc_encoder_free(void *handle);
static void *mfc_decoder_init(char *encoded_buf, int encoded_size);
static unsigned int mfc_decoder_exe(void * handle, char *buf, int size);
static void mfc_decoder_free(void *handle);



/***************** etc *******************/
#define SHARED_BUF_NUM						5
#define MFC_LINE_BUF_SIZE_PER_INSTANCE		(204800)

/**************** fimc driver need  dg add 2011-12-31 *************************/
#define V4L2_FMT_IN			0
#define V4L2_FMT_OUT			1


static int		pp_fd;
static int		key;
static int 		finished;

static int 			producer_idx, consumer_idx;
static sem_t 		full, empty;
static q_instance	queue[SHARED_BUF_NUM];

static pthread_t 	pth, pth2, pth3;
static void encoding_thread(void);
static void decoding_thread(void);
static void termination_thread(void);


static void exit_from_app() 
{
	int start;
        int fb_size_preview;
        int fb_size_decode;
	int ret;


	ioctl(pre_fb_fd, SET_OSD_STOP);
	ioctl(dec_fb_fd, SET_OSD_STOP);

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
                        fb_size_decode = g_codec_Width * g_codec_Height*2;

			break;
		case 24:
                        //fb_size = LCD_WIDTH * LCD_HEIGHT * 4;
                        fb_size_preview = g_preview_Width * g_preview_Height* 4;
                        fb_size_decode = g_codec_Width * g_codec_Height*4;

                       break;
		default:
                        //fb_size = LCD_WIDTH * LCD_HEIGHT * 4;
                         fb_size_preview = g_preview_Width * g_preview_Height* 4;
                         fb_size_decode = g_codec_Width * g_codec_Height*4;
                        printf("LCD supports 16 or 24 bpp\n");
			break;
	}


	mfc_encoder_free(enc_handle);
	mfc_decoder_free(dec_handle);

	close(cam_p_fp);
	close(cam_c_fp);
	close(pp_fd);
	 
        munmap(win0_fb_addr, fb_size_preview);
	close(pre_fb_fd);

        munmap(win1_fb_addr, fb_size_decode);
	close(dec_fb_fd);

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



static	struct v4l2_framebuffer preview;
static	struct v4l2_pix_format preview_fmt;
static	struct v4l2_capability cap;
//static	struct v4l2_input chan;
//static	struct v4l2_format codec_fmt;
/* Main Process(Camera previewing) */
int Forlinx_Test_Cam_Enc_Dec(int argc, char **argv, int lcdnum)
{

	int ret, start, found = 0;
	int k_id, k_id2, k_id3;
	int i;
	unsigned int addr;

	struct v4l2_input chan;
	struct v4l2_format codec_fmt;
	
         g_LCD_Width = lcdsize[lcdnum][0];
         g_LCD_Height =lcdsize[lcdnum][1];

         g_preview_X = g_LCD_Width/2;
         g_preview_Y = 0;


         g_preview_Width = g_LCD_Width/2;
         g_preview_Height = g_LCD_Height;


         // source picture width and height must be a multiple of 16 in MFC codecing
         g_codec_Width=g_preview_Width/16*16;
         g_codec_Height=g_preview_Height/16*16;


         char rgb_for_preview[LCD_WIDTH_MAX * LCD_HEIGHT_MAX * 4];	// MAX
         g_YUV_Frame_Buffer_Size = (g_codec_Width*g_codec_Height)+(g_codec_Width*g_codec_Height)/2;		/* YCBCR 420 */


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

	signal_ctrl_c();


	// To allocate shared buffer between producer(encoder) and consumer(decoder)
	for(i=0; i<SHARED_BUF_NUM; i++) {
		queue[i].vir_addr = (unsigned int)malloc(MFC_LINE_BUF_SIZE_PER_INSTANCE);
	}

	ret = sem_init(&full, 0, 0);
	if(ret < 0) {
		printf("sem_init failed\n");
		return -1;
	}

	ret = sem_init(&empty, 0, SHARED_BUF_NUM);
	if(ret < 0) {
		printf("sem_init failed\n");
		return -1;
	}

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
        preview_fmt.height =g_preview_Height;// LCD_HEIGHT;
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
        codec_fmt.fmt.pix.height = g_codec_Height;// LCD_HEIGHT;
        codec_fmt.fmt.pix.pixelformat= V4L2_PIX_FMT_YUV420;
        codec_fmt.fmt.pix.priv= V4L2_FMT_OUT;  // must set

	ret = ioctl(cam_c_fp , VIDIOC_S_FMT, &codec_fmt);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_S_FMT failled\n");
		exit(1);
	}

	printf("\n[10. Camera preview & MFC encoding/decoding]\n");
	printf("Forlinx Embedded, %s\n", VERSION);
	printf("Using IP            : MFC, Post processor, LCD, Camera\n");
        //printf("Camera preview size : (400x480)\n");
        //printf("Display size        : (400x480)\n");

        printf("Camera preview size : %dx%d\n",g_preview_Width,g_preview_Height);
        printf("Display size        : %dx%d\n",g_LCD_Width,g_LCD_Height);

        printf("\nx : Exit\n");
	printf("Select ==> ");	
	
	/* Encoding and decoding threads creation */
        k_id = pthread_create(&pth, 0, (void *) &encoding_thread, 0);
        k_id2 = pthread_create(&pth2, 0, (void *) &decoding_thread, 0);
        k_id3 = pthread_create(&pth3, 0, (void *) &termination_thread, 0);
	

	while (1) {
		if (finished)
			break;
		
		/* Get RGB frame from camera preview */
                //if (!read_data(cam_p_fp, &rgb_for_preview[0], LCD_WIDTH, LCD_HEIGHT, LCD_BPP)) {
                  if (!read_data(cam_p_fp, &rgb_for_preview[0], g_preview_Width,g_preview_Height, LCD_BPP)) {
                      printf("V4L2 : read_data() failed\n");
			break;
		}

		/* Write RGB frame to LCD frame buffer */
                //draw(win0_fb_addr, &rgb_for_preview[0], LCD_WIDTH, LCD_HEIGHT, LCD_BPP);
                  draw(win0_fb_addr, &rgb_for_preview[0], g_preview_Width, g_preview_Height, LCD_BPP);

	}

	pthread_join(pth, NULL);
	pthread_join(pth2, NULL);
	pthread_join(pth3, NULL);	

	for(i=0; i<SHARED_BUF_NUM; i++) {
		free((char *)queue[i].vir_addr);
	}

	exit_from_app();

	finished = 0;

	return 0;
}


/***************** Encoding Thread *****************/
void encoding_thread(void)
{
	int 			start, ret;
	long			encoded_size;
        unsigned char	yuv_buf[YUV_FRAME_BUFFER_SIZE_MAX];
	unsigned char	*encoded_buf;


	/* Setting MFC encoding parameters and initializing MFC encoder */
//	enc_handle = mfc_encoder_init(LCD_WIDTH, LCD_HEIGHT, 30, 1000, 30);
        enc_handle = mfc_encoder_init(g_codec_Width, g_codec_Height, 30, 1000, 30);

        /* Codec start */
	start = 1;
	ret = ioctl(cam_c_fp, VIDIOC_STREAMON, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_STREAMON failed\n");
		exit(1);
	}

	while (1) {
		sem_wait(&empty);

		/* read YUV frame from camera device */
                if (read(cam_c_fp, yuv_buf, g_YUV_Frame_Buffer_Size) < 0) {
			perror("read()");
		}

		enc_frame_cnt++;


		/* If this frame is first frame, It is mandatory to make encoding header */
		if(enc_frame_cnt == 1)
                        encoded_buf = mfc_encoder_exe(enc_handle, yuv_buf, g_YUV_Frame_Buffer_Size, 1, &encoded_size);
		else
                        encoded_buf = mfc_encoder_exe(enc_handle, yuv_buf, g_YUV_Frame_Buffer_Size, 0, &encoded_size);

		/* copy from encoder's output buffer to shared buffer that exists between encoder and decoder */
		memcpy((char *)queue[producer_idx].vir_addr, encoded_buf, encoded_size);

		queue[producer_idx].frame_number = enc_frame_cnt;
		queue[producer_idx].size = encoded_size;
		producer_idx++;
		producer_idx %= SHARED_BUF_NUM;

		sem_post(&full);
		
		if (finished)
			break;
	}


	/* Codec stop */
	start = 0;
	ret = ioctl(cam_c_fp, VIDIOC_STREAMOFF, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_STREAMOFF failed\n");
		exit(1);
	}

	producer_idx = 0;
	enc_frame_cnt = 0;
	mfc_encoder_free(enc_handle);
	pthread_exit(0);
}



void decoding_thread(void)
{
	unsigned int			phy_dec_out_buf;
	pp_params				pp_param;
	struct s3c_fb_dma_info	fb_info;
	unsigned int			addr;

	
	// Post processor open
	pp_fd = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd < 0)
	{
		printf("Post processor open error\n");
		return;
	}

	// set post processor configuration
        pp_param.SrcFullWidth	= g_codec_Width; //400;
        pp_param.SrcFullHeight	= g_codec_Height; //480;
	pp_param.SrcStartX		= 0;
	pp_param.SrcStartY		= 0;
	pp_param.SrcWidth		= pp_param.SrcFullWidth;
	pp_param.SrcHeight		= pp_param.SrcFullHeight;
	pp_param.SrcCSpace		= YC420;
	pp_param.DstStartX		= 0;
	pp_param.DstStartY		= 0;
        pp_param.DstFullWidth	= g_codec_Width;//400;		// destination width
        pp_param.DstFullHeight	= g_codec_Height;//480;		// destination height
	pp_param.DstWidth		= pp_param.DstFullWidth;
	pp_param.DstHeight		= pp_param.DstFullHeight;
	pp_param.DstCSpace		= RGB16;
#ifdef RGB24BPP
	pp_param.DstCSpace		= RGB24;
#endif
	pp_param.OutPath		= POST_DMA;	
	pp_param.Mode			= ONE_SHOT;


        //dec_fb_fd = fb_init(1, LCD_BPP, 400, 0, 400, 480, &addr);
        dec_fb_fd = fb_init(1, LCD_BPP, 0, 0, g_codec_Width, g_codec_Height, &addr);

	win1_fb_addr = (char *)addr;
	

	while(1) {
		
		sem_wait(&full);
		
		dec_frame_cnt++;
		
		if(dec_frame_cnt == 1)
			dec_handle = mfc_decoder_init((char *)queue[consumer_idx].vir_addr, queue[consumer_idx].size);
		
		phy_dec_out_buf = mfc_decoder_exe(dec_handle, (char *)queue[consumer_idx].vir_addr, queue[consumer_idx].size);

		ioctl(dec_fb_fd, GET_FB_INFO, &fb_info);
		pp_param.SrcFrmSt = phy_dec_out_buf;
 		pp_param.DstFrmSt = fb_info.map_dma_f1;

		ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_START);

		consumer_idx++;
		consumer_idx %= SHARED_BUF_NUM;

		sem_post(&empty);

		if (finished)
			break;
	}	

	consumer_idx = 0;
	dec_frame_cnt = 0;
	mfc_decoder_free(dec_handle);
	pthread_exit(0);
}


void termination_thread(void)
{
	while(1) {
		key = getchar();
		
		if (key == 'x') {
			finished = 1;
			pthread_exit(0);
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
        //int end_x = MIN(LCD_WIDTH, width);
        int end_x = MIN(g_preview_Width, width);


	if (bpp == 16) {

#if !defined(LCD_24BIT)
                for (y = 0; y < end_y; y++)
                {
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


void *mfc_decoder_init(char *encoded_buf, int encoded_size)
{
	void			*pStrmBuf;
	void			*handle;
	int				nFrameLeng = 0;	
	SSBSIP_H264_STREAM_INFO stream_info;	
	

	handle = SsbSipH264DecodeInit();
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "mfc_decoder_init", "H264_Dec_Init Failed.\n");
		return NULL;
	}


	pStrmBuf = SsbSipH264DecodeGetInBuf(handle, nFrameLeng);
	if (pStrmBuf == NULL) {
		LOG_MSG(LOG_ERROR, "mfc_decoder_init", "SsbSipH264DecodeGetInBuf Failed.\n");
		SsbSipH264DecodeDeInit(handle);
		return NULL;
	}

	memcpy(pStrmBuf, encoded_buf, encoded_size);


	if (SsbSipH264DecodeExe(handle, encoded_size) != SSBSIP_H264_DEC_RET_OK) {
		LOG_MSG(LOG_ERROR, "mfc_decoder_init", "H.264 Decoder Configuration Failed.\n");
		return NULL;
	}

	SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_STREAMINFO, &stream_info);



	LOG_MSG(LOG_TRACE, "mfc_decoder_init", "\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);

	return handle;
}

unsigned int mfc_decoder_exe(void *handle, char *buf, int size)
{
	char			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned int	pYUVBuf[2];


	pStrmBuf = SsbSipH264DecodeGetInBuf(handle, nFrameLeng);
	if (pStrmBuf == NULL) {
		LOG_MSG(LOG_ERROR, "mfc_decoder_exe", "SsbSipH264DecodeGetInBuf Failed.\n");
		SsbSipH264DecodeDeInit(handle);
		return -1;
	}

	memcpy(pStrmBuf, buf, size);

	if (SsbSipH264DecodeExe(handle, size) != SSBSIP_H264_DEC_RET_OK)
		return -1;

	SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);


	//printf("1   ");
	return pYUVBuf[0];
}

void mfc_decoder_free(void *handle)
{
	SsbSipH264DecodeDeInit(handle);
}
