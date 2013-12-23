#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <linux/vt.h>
#include <linux/fb.h>
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
#include "cam_dec_preview.h"
#include "videodev2_s3c_tv.h"	// peter added


#define SAMSUNG_UXGA_S5K3BA

/******************* CAMERA ********************/
#ifdef RGB24BPP
	#define LCD_24BIT		/* For s3c2443/6400 24bit LCD interface */
	#define LCD_BPP_V4L2		V4L2_PIX_FMT_RGB24 	
#else
	#define LCD_BPP_V4L2		V4L2_PIX_FMT_RGB565	/* 16 BPP - RGB565 */	
#endif

#define VIDIOC_S_TVOUT_ON 		_IO ('V', BASE_VIDIOC_PRIVATE+0)
#define VIDIOC_S_TVOUT_OFF		_IO ('V', BASE_VIDIOC_PRIVATE+1)
/* TV-OUT connector type */
#define V4L2_CID_CONNECT_TYPE		(V4L2_CID_PRIVATE_BASE+0)

#define PREVIEW_NODE  "/dev/video/preview"
#define CODEC_NODE  "/dev/video/codec"
#define TVOUT_NODE	"/dev/video/tvout"
static int cam_p_fp = -1;
static int cam_c_fp = -1;
static int tvout_fp = -1;

struct v4l2_capability cap;
/* for camera */
struct v4l2_input chan;
struct v4l2_framebuffer preview;
struct v4l2_pix_format preview_fmt;
struct v4l2_format codec_fmt;
/* for tv encoder */
struct v4l2_control ctrl;
struct v4l2_input source;
struct v4l2_output sink;
struct v4l2_format source_fmt;
struct v4l2_standard tvout_std;

/* Camera functions */
static int cam_p_init(void);
static int cam_c_init(void);
static int read_data(int fp, char *buf, int width, int height);


/************* FRAME BUFFER ***************/
#ifdef RGB24BPP
	#define LCD_BPP	24	/* 24 BPP - RGB888 */
#else
	#define LCD_BPP	16	/* 16 BPP - RGB565 */
#endif

#if 0	
#define CAM_WIDTH 	720
#define CAM_HEIGHT	480
#else	// peter for test, because flickering in the <CAM + MFC + TV out > test
#define CAM_WIDTH 	320
#define CAM_HEIGHT	240
#endif
#define RESERVE_POST_MEM	8*1024*1024	// 8mb for TV out

#define YUV_FRAME_BUFFER_SIZE	(CAM_WIDTH*CAM_HEIGHT)+(CAM_WIDTH*CAM_HEIGHT)/2		/* YCBCR 420 */

static char *win0_fb_addr = NULL;
static char *tvout_addr = NULL;
static int pre_fb_fd = -1;
static int dec_fb_fd = -1;

/* Frame buffer functions */
static int tv_init(void);


/***************** MFC *******************/
#define INPUT_BUFFER_SIZE		(204800)

static void 	*enc_handle, *dec_handle;
static int		in_fd;
static int		file_size;
static char		*in_addr;
static int		fb_size;
static unsigned char delimiter_h264[4]  = {0x00, 0x00, 0x00, 0x01};

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
//#define H264_INPUT_FILE		"./TestVectors/wanted.264"
#define H264_INPUT_FILE		"./TestVectors/veggie.264"

static int		pp_fd;
static int		key;
static int		film_cnt;
static int		frame_count;
static int 		encoding_flag = FALSE;
static int 		finished;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t 	pth, pth2;
static void encoding_thread(void);
static void decoding_thread(void *arg);
static FILE *encoded_fp;
static void sig_del_h264(int signo);


static void exit_from_app() 
{
	int start;
	//int fb_size;
	int ret;


	ioctl(dec_fb_fd, SET_OSD_STOP);	

	/* Stop previewing */
	start = 0;
	ret = ioctl(cam_p_fp, VIDIOC_OVERLAY, &start);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_OVERLAY failed\n");
		exit(1);
	}

	/* Stop TV_OUT */
	ret = ioctl(tvout_fp, VIDIOC_S_TVOUT_OFF, NULL);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_S_TVOUT_OFF) failed\n");
		exit(1);
	}

	close(cam_p_fp);
	close(cam_c_fp);
	close(pp_fd);
	close(in_fd);
	close(tvout_fp);
	
	munmap(win0_fb_addr, fb_size);
	close(dec_fb_fd);

	munmap(tvout_addr, RESERVE_POST_MEM);
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

// MFC decoding & camera preview thru TV
int Test_Cam_Dec_Preview_TV(int argc, char **argv)
{
	int ret, start, found = 0;
	int k_id, k_id2;
	char rgb_for_preview[CAM_WIDTH * CAM_HEIGHT * 4];	// MAX
	char in_filename[128];
	int tvout_on=0;
		
	
	/* Camera preview initialization */
	if ((cam_p_fp = cam_p_init()) < 0)
		exit_from_app();

	/* Camera codec initialization */
	if ((cam_c_fp = cam_c_init()) < 0)
		exit_from_app();
	
	/* TV out initialzation for previewing */
	if ((pre_fb_fd = tv_init()) < 0)
		exit_from_app();		

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

	preview_fmt.width = CAM_WIDTH;
	preview_fmt.height = CAM_HEIGHT;	
	preview_fmt.pixelformat = LCD_BPP_V4L2;		// for TV out	

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
	ret = ioctl(cam_p_fp, VIDIOC_OVERLAY, &start);	// peter for TV out
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_STREAMON failed\n");
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
	codec_fmt.fmt.pix.width = CAM_WIDTH; 
	codec_fmt.fmt.pix.height = CAM_HEIGHT; 	
	codec_fmt.fmt.pix.pixelformat= V4L2_PIX_FMT_YUV420; 
	ret = ioctl(cam_c_fp , VIDIOC_S_FMT, &codec_fmt);
	if (ret < 0) {
		printf("V4L2 : ioctl on VIDIOC_S_FMT failled\n");
		exit(1);
	}

	printf("\n[13. MFC decoding & Camera preview]\n");
	printf("Using IP                    : MFC, Post processor, LCD, Camera, TV scaler/encoder\n");
	printf("Camera preview size thru TV : SD(720x480)\n");
	printf("Display size thru LCD       : WVGA(800x480)\n");	
	
	strcpy(in_filename, H264_INPUT_FILE);
	/* Encoding and decoding threads creation */
	k_id = pthread_create(&pth, 0, (void *) &encoding_thread, 0);
	// peter	for debug
	k_id2 = pthread_create(&pth2, 0, (void *) &decoding_thread, (void *)in_filename);		

	while (1) {
		if (finished)
			break;
		
		/* Get RGB frame from camera preview */
		if (!read_data(cam_p_fp, &rgb_for_preview[0], CAM_WIDTH, CAM_HEIGHT)) {	
  		//if (!read_data(cam_p_fp, POST_BUFF_BASE_ADDR, CAM_WIDTH, CAM_HEIGHT, LCD_BPP)) {				
			printf("===========V4L2 : read_data() failed\n");
			break;
		}

		if (LCD_BPP == 16) 
			memcpy(tvout_addr, &rgb_for_preview[0], CAM_WIDTH*CAM_HEIGHT*2);
		else 
			memcpy(tvout_addr, &rgb_for_preview[0], CAM_WIDTH*CAM_HEIGHT*4);
	

		ctrl.id = V4L2_CID_MPEG_STREAM_PID_VIDEO;	
		ctrl.value = POST_BUFF_BASE_ADDR;		// COMPOSITE
		//printf("====  peter ctrl.value = 0x%08x\n", ctrl.value);	

		ret = ioctl(tvout_fp, VIDIOC_S_CTRL, &ctrl);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_CTRL) failed\n");
			exit(1);
		}

		/* Start TV_OUT */			
		if (!tvout_on) {			
			ret = ioctl(tvout_fp, VIDIOC_S_TVOUT_ON, NULL);	// Just once executed because of the free-run mode		
			if(ret < 0) {
				printf("V4L2 APPL : ioctl(VIDIOC_S_TVOUT_ON) failed\n");
				exit(1);
			}
			tvout_on = 1;
		}		

	}
	
	pthread_join(pth, NULL);
	pthread_join(pth2, NULL);

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
	unsigned char	g_yuv[YUV_FRAME_BUFFER_SIZE];
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
			ioctl(dec_fb_fd, SET_OSD_STOP);
			pthread_exit(0);
		}
		
		if (encoding_flag == TRUE) {

			pthread_mutex_lock(&mutex);

			enc_handle = mfc_encoder_init(CAM_WIDTH, CAM_HEIGHT, 30, 1000, 30);
			
			sprintf(&file_name[0], "Cam_encoding_%dx%d-%d.264", CAM_WIDTH, CAM_HEIGHT, ++film_cnt);
			printf("Name of photo file : Cam_encoding_%dx%d-%d.264\n", CAM_WIDTH, CAM_HEIGHT, film_cnt);
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
				if (read(cam_c_fp, g_yuv, YUV_FRAME_BUFFER_SIZE) < 0) {
					perror("read()");
				}


				if(frame_count == 1)
					encoded_buf = mfc_encoder_exe(enc_handle, g_yuv, YUV_FRAME_BUFFER_SIZE, 1, &encoded_size);
				else
					encoded_buf = mfc_encoder_exe(enc_handle, g_yuv, YUV_FRAME_BUFFER_SIZE, 0, &encoded_size);			

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


			//frame_count = 0;
			
			printf("100 frames were encoded\n");
			printf("\nSelect ==> ");

			mfc_encoder_free(enc_handle);

			fclose(encoded_fp);

			encoding_flag= FALSE;

			pthread_mutex_unlock(&mutex);
			
		}
		
	}
}


void decoding_thread(void *arg)
{
	
	void			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned int	pYUVBuf[2];
	
	struct stat				s;
	FRAMEX_CTX				*pFrameExCtx;	// frame extractor context
	FRAMEX_STRM_PTR 		file_strm;
	SSBSIP_H264_STREAM_INFO stream_info;	
	
	pp_params	pp_param;
	s3c_win_info_t	osd_info_to_driver;

	struct fb_fix_screeninfo	lcd_info;		
	
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif
	char			*in_filename = (char *)arg;

	if(signal(SIGINT, sig_del_h264) == SIG_ERR) {
		printf("Sinal Error\n");
	}
	

	// in file open
	in_fd	= open(in_filename, O_RDONLY);
	if(in_fd < 0) {
		printf("Input file open failed\n");
		return;
	}

	// get input file size
	fstat(in_fd, &s);
	file_size = s.st_size;
	
	// mapping input file to memory
	in_addr = (char *)mmap(0, file_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if(in_addr == NULL) {
		printf("input file memory mapping failed\n");
		return;
	}
	
	// Post processor open
	pp_fd = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd < 0)
	{
		printf("Post processor open error\n");
		return;
	}

	// LCD frame buffer open
	dec_fb_fd = open(FB_DEV_NAME, O_RDWR|O_NDELAY);
	if(dec_fb_fd < 0)
	{
		printf("LCD frame buffer open error\n");
		return;
	}

	///////////////////////////////////
	// FrameExtractor Initialization //
	///////////////////////////////////
	pFrameExCtx = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_h264, sizeof(delimiter_h264), 1);   
	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	FrameExtractorFirst(pFrameExCtx, &file_strm);
	

	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipH264DecodeInit)    ///
	//////////////////////////////////////
	dec_handle = SsbSipH264DecodeInit();
	if (dec_handle == NULL) {
		printf("H264_Dec_Init Failed.\n");
		return;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipH264DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipH264DecodeGetInBuf(dec_handle, nFrameLeng);
	if (pStrmBuf == NULL) {
		printf("SsbSipH264DecodeGetInBuf Failed.\n");
		SsbSipH264DecodeDeInit(dec_handle);
		return;
	}

	////////////////////////////////////
	//  H264 CONFIG stream extraction //
	////////////////////////////////////
	nFrameLeng = ExtractConfigStreamH264(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipH264DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipH264DecodeExe(dec_handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK) {
		printf("H.264 Decoder Configuration Failed.\n");
		return;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipH264DecodeGetConfig(dec_handle, H264_DEC_GETCONF_STREAMINFO, &stream_info);

	//printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);


	// set post processor configuration
	pp_param.SrcFullWidth	= stream_info.width;
	pp_param.SrcFullHeight	= stream_info.height;
	pp_param.SrcWidth		= stream_info.width;
	pp_param.SrcHeight		= stream_info.height;
	pp_param.SrcStartX		= 0;
	pp_param.SrcStartY		= 0;
	pp_param.SrcCSpace		= YC420;
	pp_param.DstFullWidth	= 800;		// destination width
	pp_param.DstFullHeight	= 480;		// destination height
	pp_param.DstWidth		= 800;
	pp_param.DstHeight		= 480;
	pp_param.DstStartX		= 0;
	pp_param.DstStartY		= 0;
	pp_param.DstCSpace		= RGB16;
#ifdef RGB24BPP
	pp_param.DstCSpace		= RGB24;
#endif
	pp_param.OutPath		= POST_DMA;
	pp_param.Mode			= ONE_SHOT;
	
	// get LCD frame buffer address
	fb_size = pp_param.DstFullWidth * pp_param.DstFullHeight * 2;	// RGB565
#ifdef RGB24BPP
	fb_size = pp_param.DstFullWidth * pp_param.DstFullHeight * 4;	// RGB888
#endif

	win0_fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, dec_fb_fd, 0);
	if (win0_fb_addr == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return;
	}

	osd_info_to_driver.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver.Bpp			= 24;	// RGB24
#endif
	osd_info_to_driver.LeftTop_x	= 0;	
	osd_info_to_driver.LeftTop_y	= 0;
	osd_info_to_driver.Width		= 800;	// display width
	osd_info_to_driver.Height		= 480;	// display height

	// set OSD's information 
	if(ioctl(dec_fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return;
	}

	ioctl(dec_fb_fd, SET_OSD_START);
	
	while(1)
	{
		if (finished) {
			break;
		}
		
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipH264DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipH264DecodeExe(dec_handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK)
			break;
	
	
		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipH264DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		SsbSipH264DecodeGetConfig(dec_handle, H264_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);

	
		/////////////////////////////
		// Next H.264 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameH264(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
		if (nFrameLeng < 4)
			break;
					
		// Post processing
		// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
		// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
		pp_param.SrcFrmSt		= pYUVBuf[0];	// MFC output buffer
		ioctl(dec_fb_fd, FBIOGET_FSCREENINFO, &lcd_info);
		pp_param.DstFrmSt		= lcd_info.smem_start;			// LCD frame buffer
		ioctl(pp_fd, PPROC_SET_PARAMS, &pp_param);
		ioctl(pp_fd, PPROC_START);	


	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif
	
	
	}

#ifdef FPS
	printf("Display Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	ioctl(dec_fb_fd, SET_OSD_STOP);
	SsbSipH264DecodeDeInit(dec_handle);	

	munmap(in_addr, file_size);
	munmap(win0_fb_addr, fb_size);
	close(pp_fd);
	close(dec_fb_fd);
	close(in_fd);

	pthread_exit(0);
}


static void sig_del_h264(int signo)
{
	printf("[H.264 display & preview]signal handling\n");
	ioctl(dec_fb_fd, SET_OSD_STOP);
	SsbSipH264DecodeDeInit(dec_handle);

	munmap(in_addr, file_size);
	munmap(win0_fb_addr, fb_size);
	close(pp_fd);
	close(dec_fb_fd);
	close(in_fd);

	exit(1);
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

static int read_data(int fp, char *buf, int width, int height)
{
	int ret;	

	if (LCD_BPP == 16) {
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

/***************** Display TV encoder function *****************/
static int tv_init(void)
{
	int dev_fp = -1;
	int fb_size;
	s3c_win_info_t fb_info_to_driver;	
	int ret;

	tvout_fp = open(TVOUT_NODE, O_RDWR);
	if(tvout_fp < 0) {
		printf("TV out Device file open error =%d\n", tvout_fp);
		return -1;
	}

	/* Get capability */
	ret = ioctl(tvout_fp, VIDIOC_QUERYCAP, &cap);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_QUERYCAP) failed\n");
		exit(1);
	}
	printf("V4L2 APPL : Name of the interface is %s\n",cap.driver);

	/* Check the type of device driver */
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		printf("V4L2 APPL : Check capability !!!\n");
		exit(1);
	}

	/* Set source path : index 0(LCD FIFO-IN), 1(MSDMA)*/
	source.index = 1;
	while(1) {
		ret = ioctl(tvout_fp, VIDIOC_ENUMINPUT, &source);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_ENUMINPUT) failed\n");
			exit(1);
		}
		printf("V4L2 APPL : [%d]: IN channel name %s\n", source.index, source.name);

		/* Test channel.type */
		if(source.type & V4L2_INPUT_TYPE_MSDMA) {
			printf("V4L2 APPL : DMA INPUT \n");
			break;
		}
		source.index++;
	}

	/* Setting for source channel 1 which is channel of DMA */
	ret = ioctl(tvout_fp, VIDIOC_S_INPUT, &source);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_S_INPUT) failed\n");
		exit(1);
	}
		
	/* Set sink path : index 0(TV-OUT/FIFO-OUT), 1(MSDMA)*/
	sink.index = 0;
	while(1) {
		ret = ioctl(tvout_fp, VIDIOC_ENUMOUTPUT, &sink);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_ENUMOUTPUT) failed\n");
			exit(1);
		}
		printf("V4L2 APPL : [%d]: OUT channel name %s\n", sink.index, sink.name);

		// Test channel.type
		if(sink.type & V4L2_OUTPUT_TYPE_ANALOG) {
			printf("V4L2 APPL : TV OUT \n");
			break;
		}
		sink.index++;
	}

	/* Setting for sink channel 0 which is channel of TV OUT */
	ret = ioctl(tvout_fp, VIDIOC_S_OUTPUT, &sink);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_S_OUTPUT) failed\n");
		exit(1);
	}

	/* Set source format */
	source_fmt.fmt.pix.width = CAM_WIDTH;
	source_fmt.fmt.pix.height = CAM_HEIGHT;
	//source_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;	// peter for TV out	
	source_fmt.fmt.pix.pixelformat = LCD_BPP_V4L2;		
		
	ret = ioctl(tvout_fp, VIDIOC_S_FMT, &source_fmt);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_S_FMT) failed\n");
		exit(1);
	}

	/* Set TV standard format */
	tvout_std.index = 0;

	ret = ioctl(tvout_fp, VIDIOC_ENUMSTD, &tvout_std);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_ENUMSTD) failed\n");
		exit(1);
	}

	tvout_std.id = V4L2_STD_NTSC_M;
	ret = ioctl(tvout_fp, VIDIOC_S_STD, &tvout_std.id);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_S_STD) failed\n");
		exit(1);
	}

	/* Set specific ioctl - TVOUT connect type */
	ctrl.id = V4L2_CID_CONNECT_TYPE;
	ctrl.value = 0;	// COMPOSITE

	ret = ioctl(tvout_fp, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_S_CTRL) failed\n");
		exit(1);
	}

	tvout_addr = (char *) mmap(0, RESERVE_POST_MEM, PROT_READ | PROT_WRITE, MAP_SHARED, tvout_fp, 0);
	if(tvout_addr == NULL) {
		printf("TV out frame buffer mmap failed\n");
		close(tvout_fp);
		exit(1);	
	}		
		
	return tvout_fp;	
	
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


