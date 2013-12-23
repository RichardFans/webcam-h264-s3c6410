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
#include <semaphore.h>

#include "SsbSipH264Decode.h"
#include "SsbSipMpeg4Decode.h"
#include "SsbSipVC1Decode.h"
#include "FrameExtractor.h"
#include "MPEG4Frames.h"
#include "H263Frames.h"
#include "H264Frames.h"
#include "SsbSipLogMsg.h"
#include "performance.h"
#include "post.h"
#include "lcd.h"
#include "MfcDriver.h"
#include "FileRead.h"
#include "display_optimization1.h"


static unsigned char delimiter_h264[4]  = {0x00, 0x00, 0x00, 0x01};
static int nFrameLeng;

#define INPUT_BUFFER_SIZE		(204800)


static void		*handle;
static int		in_fd;
static int		file_size;
static char		*in_addr;
static int		fb_size;
static int		pp_fd, fb_fd;
static char		*fb_addr;	

static void sig_del_h264(int signo);

static q_instance	queue[10];
static sem_t		full;
static sem_t		empty;
static int			producer_idx, consumer_idx;
static int			frame_cnt;
static int			frame_need_cnt[2];
static pthread_t	th;

static void *rendering(void *arg)
{
	pp_params	*pp_param = (pp_params *)arg;
	
	
	while(1) {
		sem_wait(&full);

		pp_param->SrcFrmSt = queue[consumer_idx].phy_addr;
		ioctl(pp_fd, PPROC_SET_PARAMS, pp_param);

		ioctl(pp_fd, PPROC_START);

		consumer_idx++;
		consumer_idx %= frame_need_cnt[0];

		if(nFrameLeng < 4) 
			break;
		
		sem_post(&empty);
	}

	return NULL;
}

int Test_Display_Optimization1(int argc, char **argv)
{
	
	void					*pStrmBuf;
	unsigned int			pYUVBuf[2];
	
	struct stat				s;
	FRAMEX_CTX				*pFrameExCtx;	// frame extractor context
	FRAMEX_STRM_PTR 		file_strm;
	SSBSIP_H264_STREAM_INFO stream_info;	
	
	pp_params				pp_param;
	s3c_win_info_t			osd_info_to_driver;

	int						r;

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
#endif


	if(signal(SIGINT, sig_del_h264) == SIG_ERR) {
		printf("Sinal Error\n");
	}

	if (argc != 2) {
		printf("Usage : mfc <H.264 input filename>\n");
		return -1;
	}

#ifndef RGB24BPP
	printf("Display optimization 1 supports 24bpp only.\n");
	printf("Because local path is used between post processor and lcd\n");
	printf("RGB24BPP macro must be added in Makefile\n");
	return -1;
#endif

	// in file open
	in_fd	= open(argv[1], O_RDONLY);
	if(in_fd < 0) {
		printf("Input file open failed\n");
		return -1;
	}

	// get input file size
	fstat(in_fd, &s);
	file_size = s.st_size;
	
	// mapping input file to memory
	in_addr = (char *)mmap(0, file_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if(in_addr == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}
	
	// Post processor open
	pp_fd = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd < 0)
	{
		printf("Post processor open error\n");
		return -1;
	}

	// LCD frame buffer open
	fb_fd = open(FB_DEV_NAME, O_RDWR|O_NDELAY);
	if(fb_fd < 0)
	{
		printf("LCD frame buffer open error\n");
		return -1;
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
	handle = SsbSipH264DecodeInit();
	if (handle == NULL) {
		printf("H264_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipH264DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipH264DecodeGetInBuf(handle, nFrameLeng);
	if (pStrmBuf == NULL) {
		printf("SsbSipH264DecodeGetInBuf Failed.\n");
		SsbSipH264DecodeDeInit(handle);
		return -1;
	}

	////////////////////////////////////
	//  H264 CONFIG stream extraction //
	////////////////////////////////////
	nFrameLeng = ExtractConfigStreamH264(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipH264DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK) {
		printf("H.264 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_STREAMINFO, &stream_info);

	printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);
	

	// set post processor configuration
	pp_param.SrcFullWidth	= stream_info.width;
	pp_param.SrcFullHeight	= stream_info.height;
	pp_param.SrcStartX		= 0;
	pp_param.SrcStartY		= 0;
	pp_param.SrcWidth		= pp_param.SrcFullWidth;
	pp_param.SrcHeight		= pp_param.SrcFullHeight;
	pp_param.SrcCSpace		= YC420;
	pp_param.DstStartX		= 0;
	pp_param.DstStartY		= 0;
	pp_param.DstFullWidth	= 800;		// destination width
	pp_param.DstFullHeight	= 480;		// destination height
	pp_param.DstWidth		= pp_param.DstFullWidth;
	pp_param.DstHeight		= pp_param.DstFullHeight;
	pp_param.DstCSpace		= RGB24;
	pp_param.OutPath		= POST_FIFO;
	pp_param.Mode			= ONE_SHOT;


	// get LCD frame buffer address
	fb_size = pp_param.DstFullWidth * pp_param.DstFullHeight * 4;	// RGB888

	fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_addr == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver.Bpp			= 24;	// RGB888
	osd_info_to_driver.LeftTop_x	= 0;	
	osd_info_to_driver.LeftTop_y	= 0;
	osd_info_to_driver.Width		= 800;	// display width
	osd_info_to_driver.Height		= 480;	// display height

	
	// set OSD's information 
	if(ioctl(fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	//Default setting of window0 doesn't use
	ioctl(fb_fd, SET_OSD_STOP);

	SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_FRAM_NEED_COUNT, frame_need_cnt);

	printf("Frame need count is %d\n", frame_need_cnt[0]);

	if(pthread_create(&th, NULL, rendering, (void *)&pp_param) < 0) {
		printf("Rendering thread creation error\n");
		return -1;
	}

	r = sem_init(&full, 0, 0);
	if(r < 0) {
		printf("sem_init failed\n");
		return -1;
	}

	r = sem_init(&empty, 0, frame_need_cnt[0]);
	if(r < 0) {
		printf("sem_init failed\n");
		return -1;
	}

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
	while(1)
	{

		sem_wait(&empty);
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipH264DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK) {
			printf("frame count : %d\n", frame_cnt);
			break;
		}
	
	
		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipH264DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);

		frame_cnt++;
		queue[producer_idx].phy_addr = pYUVBuf[0];
		queue[producer_idx].frame_number = frame_cnt;
		producer_idx++;
		producer_idx %= frame_need_cnt[0];

		sem_post(&full);
		
	
		/////////////////////////////
		// Next H.264 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameH264(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
		if (nFrameLeng < 4)
			break;
	
		if(frame_cnt == 1)
			ioctl(fb_fd, SET_OSD_START);
	}

	pthread_join(th, NULL);

#ifdef FPS
	gettimeofday(&stop, NULL);
	time = measureTime(&start, &stop);
	printf("Display Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	frame_cnt = 0;
	producer_idx = 0;
	consumer_idx = 0;

	ioctl(fb_fd, SET_OSD_STOP);	
	SsbSipH264DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);
	
	return 0;
}



static void sig_del_h264(int signo)
{
	printf("signal handling\n");

	frame_cnt = 0;
	producer_idx = 0;
	consumer_idx = 0;
	
	ioctl(fb_fd, SET_OSD_STOP);	
	//pthread_exit(0);
	pthread_join(th, NULL);
	SsbSipH264DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);
	exit(1);
}
