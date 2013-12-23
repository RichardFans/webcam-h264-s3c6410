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

#include "SsbSipH264Decode.h"
#include "SsbSipMpeg4Decode.h"
#include "SsbSipVC1Decode.h"
#include "FrameExtractor.h"
#include "MPEG4Frames.h"
#include "H263Frames.h"
#include "H264Frames.h"
#include "VC1Frames.h"
#include "LogMsg.h"
#include "performance.h"
#include "post.h"
#include "lcd.h"
#include "MfcDriver.h"
#include "FileRead.h"


#define H264_INPUT_FILE		"./TestVectors/wanted.264"
#define MPEG4_INPUT_FILE	"./TestVectors/shrek.m4v"
#define H263_INPUT_FILE		"./TestVectors/iron.263"
#define	VC1_INPUT_FILE		"./TestVectors/test2_0.rcv"

static unsigned char delimiter_mpeg4[3] = {0x00, 0x00, 0x01};
static unsigned char delimiter_h264[4]  = {0x00, 0x00, 0x00, 0x01};
static int lcdsize[6][2] = {{320,240}, {480,272}, {640,480}, {800,480}, {800,600},{1024,768}};

#define INPUT_BUFFER_SIZE		(204800)

static void			*handle;
static int			in_fd;
static int			file_size;
static char			*in_addr;
static int			fb_size;
static int			pp_fd, fb_fd;
static char			*fb_addr;	

static void sig_del_h264(int signo);
static void sig_del_mpeg4(int signo);
static void sig_del_vc1(int signo);

#if 0
#define DEBUG
//
//debug:  print line.  put a go on.  put b break
//
char debug_char;

inline void debug_line()
{
	
	while((debug_char=getchar())!='a'){
		if (debug_char=='b')
			return -1;
	}
}
#endif

#ifdef DEBUG
#define DEBUG_LINE printf("debug line %d\n",__LINE__);while((debug_char=getchar())!='a'){if (debug_char=='b')return -1;}
#else
#define DEBUG_LINE  
#endif 
int Forlinx_Test_Display_H264(int argc, char **argv, int lcdnum)
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
	int				mod_cnt = 0;
#endif


	if(signal(SIGINT, sig_del_h264) == SIG_ERR) {
		printf("Sinal Error\n");
	}

	// in file open
	in_fd	= open(H264_INPUT_FILE, O_RDONLY);
	if(in_fd < 0) {
		printf("Input file open failed\n");
		return -1;
	}
	DEBUG_LINE	
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

//	printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);

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
	pp_param.DstFullWidth	= lcdsize[lcdnum][0];//800;		// destination width
	pp_param.DstFullHeight	= lcdsize[lcdnum][1];//480;		// destination height
	pp_param.DstWidth		= pp_param.DstFullWidth;
	pp_param.DstHeight		= pp_param.DstFullHeight;
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

	fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_addr == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver.Bpp			= 24;	// RGB16
#endif
	osd_info_to_driver.LeftTop_x	= 0;	
	osd_info_to_driver.LeftTop_y	= 0;
	osd_info_to_driver.Width		= lcdsize[lcdnum][0];//800;		// display width
	osd_info_to_driver.Height		= lcdsize[lcdnum][1];//480;		// display height

	// set OSD's information 
	if(ioctl(fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd, SET_OSD_START);

	printf("\n========== H.264 File Decodec Test ==========\n");
	printf("Forlinx Embedded, %s\n", VERSION);
	printf("\n[1. H.264 display]\n");
	printf("Using IP          : MFC, Post processor, LCD\n");
	printf("Input filename    : wanted.264\n");
	printf("Input vector size : VGA(640x480)\n");
	printf("Display size      : (%dx%d)\n",lcdsize[lcdnum][0], lcdsize[lcdnum][1]);
	printf("Bitrate           : 971 Kbps\n");
	printf("FPS               : 30\n");

#if 1 //phantom   for Test_Cam_Enc_Dec
	if (argc == 222){
		//printf("debug!!\n");
		return 0;
	}
#endif
	while(1)
	{

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipH264DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK)
			break;
	
	
		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipH264DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);

	
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
		ioctl(fb_fd, FBIOGET_FSCREENINFO, &lcd_info);
		pp_param.DstFrmSt		= lcd_info.smem_start;			// LCD frame buffer
		ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);

		// phantom
		ioctl(pp_fd, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param);
		//----

		ioctl(pp_fd, S3C_PP_START);


	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
		mod_cnt++;
		if (mod_cnt == 50) {
			printf("Average FPS : %u\n", (float)mod_cnt*1000/time);
			mod_cnt = 0;
			time = 0;
		}
	#endif
	
		usleep(2000);
	}

#ifdef FPS
	printf("Display Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	ioctl(fb_fd, SET_OSD_STOP);
	SsbSipH264DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);
	
	return 0;
}


int Forlinx_Test_Display_MPEG4(int argc, char **argv, int lcdnum)
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


	if(signal(SIGINT, sig_del_mpeg4) == SIG_ERR) {
		printf("Sinal Error\n");
	}

	// in file open
	in_fd	= open(MPEG4_INPUT_FILE, O_RDONLY);
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
	pFrameExCtx = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_mpeg4, sizeof(delimiter_mpeg4), 1);   
	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	FrameExtractorFirst(pFrameExCtx, &file_strm);
	

	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipMPEG4DecodeInit)    ///
	//////////////////////////////////////
	handle = SsbSipMPEG4DecodeInit();
	if (handle == NULL) {
		printf("MPEG4_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipMPEG4DecodeGetInBuf(handle, nFrameLeng);
	if (pStrmBuf == NULL) {
		printf("SsbSipMPEG4DecodeGetInBuf Failed.\n");
		SsbSipMPEG4DecodeDeInit(handle);
		return -1;
	}

	////////////////////////////////////
	//  MPEG4 CONFIG stream extraction //
	////////////////////////////////////
	nFrameLeng = ExtractConfigStreamMpeg4(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK) {
		printf("MPEG-4 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipMPEG4DecodeGetConfig(handle, MPEG4_DEC_GETCONF_STREAMINFO, &stream_info);

	
	memset(&pp_param, 0, sizeof(pp_params));

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
	pp_param.DstFullWidth	= lcdsize[lcdnum][0];//800		// destination width
	pp_param.DstFullHeight	= lcdsize[lcdnum][1];//480;		// destination height
	pp_param.DstWidth		= pp_param.DstFullWidth;
	pp_param.DstHeight		= pp_param.DstFullHeight;
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

	fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_addr == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver.Bpp			= 24;	// RGB24
#endif
	osd_info_to_driver.LeftTop_x	= 0;	
	osd_info_to_driver.LeftTop_y	= 0;
	osd_info_to_driver.Width		= lcdsize[lcdnum][0];//800;	// display width
	osd_info_to_driver.Height		= lcdsize[lcdnum][1];//480;		// display height

	// set OSD's information 
	if(ioctl(fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
	        return -1;
	}

	ioctl(fb_fd, SET_OSD_START);

	printf("\n========== MPEG4 File Decodec Test =========\n");
	printf("Forlinx Embedded, %s\n", VERSION);
	//printf("\n");
	printf("\n[2. MPEG4 display]\n");
	printf("Using IP          : MFC, Post processor, LCD\n");
	printf("Input filename    : shrek.m4v\n");
	printf("Input vector size : QVGA(320x240)\n");
	printf("Display size      : (%dx%d)\n",lcdsize[lcdnum][0],lcdsize[lcdnum][1]);
	printf("Bitrate           : 482 Kbps\n");
	printf("FPS               : 24\n");

	
	while(1)
	{

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipMPEG4DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK)
			break;

	
		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipMPEG4DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		SsbSipMPEG4DecodeGetConfig(handle, MPEG4_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);

		
		/////////////////////////////
		// Next MPEG4 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameMpeg4(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
		if (nFrameLeng < 4)
			break;
					
		// Post processing
		// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
		// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
		pp_param.SrcFrmSt		= pYUVBuf[0];	// MFC output buffer
		ioctl(fb_fd, FBIOGET_FSCREENINFO, &lcd_info);
		pp_param.DstFrmSt		= lcd_info.smem_start;			// LCD frame buffer
		ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_START);

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif
		usleep(6000);
	}

#ifdef FPS
	printf("Display Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	ioctl(fb_fd, SET_OSD_STOP);

	SsbSipMPEG4DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);
	
	return 0;
}


int Forlinx_Test_Display_H263(int argc, char **argv, int lcdnum)
{
	
	void			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned int	pYUVBuf[2];
	
	struct stat				s;
	MMAP_STRM_PTR 			file_strm;
	SSBSIP_H264_STREAM_INFO stream_info;	
	
	pp_params		pp_param;
	s3c_win_info_t	osd_info_to_driver;

	struct fb_fix_screeninfo	lcd_info;		
	
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif


	if(signal(SIGINT, sig_del_mpeg4) == SIG_ERR) {
		printf("Sinal Error\n");
	}
	
	// in file open
	in_fd	= open(H263_INPUT_FILE, O_RDONLY);
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

	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipMPEG4DecodeInit)    ///
	//////////////////////////////////////
	handle = SsbSipMPEG4DecodeInit();
	if (handle == NULL) {
		printf("H263_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipMPEG4DecodeGetInBuf(handle, 200000);
	if (pStrmBuf == NULL) {
		printf("SsbSipMPEG4DecodeGetInBuf Failed.\n");
		SsbSipMPEG4DecodeDeInit(handle);
		return -1;
	}
	

	////////////////////////////////////
	//  MPEG4 CONFIG stream extraction //
	////////////////////////////////////
	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	nFrameLeng = ExtractConfigStreamH263(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK) {
		printf("H.263 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipMPEG4DecodeGetConfig(handle, MPEG4_DEC_GETCONF_STREAMINFO, &stream_info);

	
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
	pp_param.DstFullWidth	= lcdsize[lcdnum][0];//800;		// destination width
	pp_param.DstFullHeight	= lcdsize[lcdnum][1];//480;		// destination height
	pp_param.DstWidth		= pp_param.DstFullWidth;
	pp_param.DstHeight		= pp_param.DstFullHeight;
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

	fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_addr == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver.Bpp			= 24;	// RGB24
#endif
	osd_info_to_driver.LeftTop_x	= 0;	
	osd_info_to_driver.LeftTop_y	= 0;
	osd_info_to_driver.Width		= lcdsize[lcdnum][0];//800;	;	// display width
	osd_info_to_driver.Height		= lcdsize[lcdnum][1];//480;	;// display height

	// set OSD's information 
	if(ioctl(fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd, SET_OSD_START);

	printf("\n========== H.263 File Decodec Test ==========\n");
	printf("Forlinx Embedded, %s\n", VERSION);
	printf("\n[3. H.263 display]\n");
	printf("Using IP          : MFC, Post processor, LCD\n");
	printf("Input filename    : iron.263\n");
	printf("Input vector size : QVGA(320x240)\n");
	printf("Display size      : (%dx%d)\n",lcdsize[lcdnum][0],lcdsize[lcdnum][1]);
	printf("Bitrate           : 460 Kbps\n");
	printf("FPS               : 30\n");

	
	while(1)
	{

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipMPEG4DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK)
			break;

	
		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipMPEG4DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		SsbSipMPEG4DecodeGetConfig(handle, MPEG4_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);

	
		/////////////////////////////
		// Next MPEG4 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameH263(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);

		if (nFrameLeng < 4)
			break;
					
		// Post processing
		// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
		// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
		pp_param.SrcFrmSt		= pYUVBuf[0];	// MFC output buffer
		ioctl(fb_fd, FBIOGET_FSCREENINFO, &lcd_info);
		pp_param.DstFrmSt		= lcd_info.smem_start;			// LCD frame buffer
		ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_START);

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif
		usleep(6000);
	}

#ifdef FPS
	printf("Display Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	ioctl(fb_fd, SET_OSD_STOP);

	SsbSipMPEG4DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);
	
	return 0;
}


int Forlinx_Test_Display_VC1(int argc, char **argv, int lcdnum)
{
	
	void			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned int	pYUVBuf[2];
		
	struct stat				s;
	MMAP_STRM_PTR 			file_strm;
	SSBSIP_H264_STREAM_INFO stream_info;	
	
	pp_params		pp_param;
	s3c_win_info_t	osd_info_to_driver;

	struct fb_fix_screeninfo	lcd_info;		
	
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

	int r = 0;


	if(signal(SIGINT, sig_del_vc1) == SIG_ERR) {
		printf("Sinal Error\n");
	}
	
	// in file open
	in_fd	= open(VC1_INPUT_FILE, O_RDONLY);
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

	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipMPEG4DecodeInit)    ///
	//////////////////////////////////////
	handle = SsbSipVC1DecodeInit();
	if (handle == NULL) {
		printf("VC1_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipVC1DecodeGetInBuf(handle, 200000);
	if (pStrmBuf == NULL) {
		printf("SsbSipVC1DecodeGetInBuf Failed.\n");
		SsbSipVC1DecodeDeInit(handle);
		return -1;
	}
	

	////////////////////////////////////
	//  MPEG4 CONFIG stream extraction //
	////////////////////////////////////
	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	nFrameLeng = ExtractConfigStreamVC1(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	r = SsbSipVC1DecodeExe(handle, nFrameLeng);
	if (r != SSBSIP_VC1_DEC_RET_OK) {
		printf("VC-1 Decoder Configuration Failed. : %d\n", r);
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipVC1DecodeGetConfig(handle, VC1_DEC_GETCONF_STREAMINFO, &stream_info);

	
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
	pp_param.DstFullWidth	= lcdsize[lcdnum][0];//800;	;		// destination width
	pp_param.DstFullHeight	= lcdsize[lcdnum][1];//480;	;		// destination height
	pp_param.DstWidth		= pp_param.DstFullWidth;
	pp_param.DstHeight		= pp_param.DstFullHeight;
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

	fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_addr == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver.Bpp			= 24;	// RGB24
#endif
	osd_info_to_driver.LeftTop_x	= 0;	
	osd_info_to_driver.LeftTop_y	= 0;
	osd_info_to_driver.Width		= lcdsize[lcdnum][0];//800;	;	// display width
	osd_info_to_driver.Height		= lcdsize[lcdnum][1];//480;	;	// display height

	// set OSD's information 
	if(ioctl(fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd, SET_OSD_START);

	printf("\n========== VC-1 File Decodec Test ===========\n");
	printf("Forlinx Embedded, %s\n", VERSION);
	printf("\n[4. VC-1 display]\n");
	printf("Using IP          : MFC, Post processor, LCD\n");
	printf("Input filename    : test2_0.rcv\n");
	printf("Input vector size : QVGA(320x240)\n");
	printf("Display size      : (%dx%d)\n",lcdsize[lcdnum][0], lcdsize[lcdnum][1]);
	printf("Bitrate           : 460 Kbps\n");
	printf("FPS               : 30\n");

	
	while(1)
	{

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipMPEG4DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipVC1DecodeExe(handle, nFrameLeng) != SSBSIP_VC1_DEC_RET_OK)
			break;

	
		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipMPEG4DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		SsbSipVC1DecodeGetConfig(handle, VC1_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);

	
		/////////////////////////////
		// Next MPEG4 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameVC1(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);

		if (nFrameLeng < 4)
			break;
					
		// Post processing
		// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
		// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
		pp_param.SrcFrmSt		= pYUVBuf[0];	// MFC output buffer
		ioctl(fb_fd, FBIOGET_FSCREENINFO, &lcd_info);
		pp_param.DstFrmSt		= lcd_info.smem_start;			// LCD frame buffer
		ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param);
		ioctl(pp_fd, S3C_PP_START);

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif
		usleep(6000);
	
	}

#ifdef FPS
	printf("Display Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	ioctl(fb_fd, SET_OSD_STOP);

	SsbSipVC1DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);
	
	return 0;
}


static void sig_del_h264(int signo)
{
	printf("[H.264 display] signal handling\n");

	ioctl(fb_fd, SET_OSD_STOP);
	SsbSipH264DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);

	exit(1);
}

static void sig_del_mpeg4(int signo)
{
	printf("[MPEG4/H263 display] signal handling\n");

	ioctl(fb_fd, SET_OSD_STOP);
	SsbSipMPEG4DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);

	exit(1);
}

static void sig_del_vc1(int signo)
{
	printf("[VC-1 display] signal handling\n");

	ioctl(fb_fd, SET_OSD_STOP);
	SsbSipVC1DecodeDeInit(handle);

	munmap(in_addr, file_size);
	munmap(fb_addr, fb_size);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);

	exit(1);
}

