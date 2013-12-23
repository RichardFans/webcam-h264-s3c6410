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


#define H264_INPUT_FILE		"./TestVectors/veggie.264"
#define MPEG4_INPUT_FILE	"./TestVectors/shrek.m4v"
#define H263_INPUT_FILE		"./TestVectors/iron.263"
#define VC1_INPUT_FILE		"./TestVectors/test2_0.rcv"


/************************************************************************
 * If you execute this demo application, MFC driver must be configured	* 
 * as MFC_NUM_INSTANCES_MAX 4 in MfcConfig.h header file                *
 * **********************************************************************/


#define INPUT_BUFFER_SIZE		(204800)


static unsigned char delimiter_h264[4]  = {0x00, 0x00, 0x00, 0x01};
static unsigned char delimiter_mpeg4[] = {0x00, 0x00, 0x01 };
static int lcdsize[6][2] = {{320,240}, {480,272}, {640,480}, {800,480}, {800,600},{1024,768}};


static int			in_fd1, in_fd2, in_fd3, in_fd4;
static int			file_size1, file_size2, file_size3, file_size4;
static int			fb_size1, fb_size2, fb_size3, fb_size4;
static int			pp_fd1, pp_fd2, pp_fd3, pp_fd4;
static int			fb_fd1, fb_fd2, fb_fd3, fb_fd4;
static int			last_frame1, last_frame2, last_frame3, last_frame4;
static char			*fb_addr1, *fb_addr2, *fb_addr3, *fb_addr4;	
static char			*in_addr1, *in_addr2, *in_addr3, *in_addr4;
static char			in_filename1[128], in_filename2[128], in_filename3[128], in_filename4[128];
static void			*handle1, *handle2, *handle3, *handle4;
 
static void sig_del(int signo);


int Forlinx_Test_Display_4_Windows(int argc, char **argv, int lcdnum)
{
 	void			*pStrmBuf1, *pStrmBuf2, *pStrmBuf3, *pStrmBuf4;
	int				nFrameLeng1 = 0;
	int				nFrameLeng2 = 0;
	int				nFrameLeng3 = 0;
	int				nFrameLeng4 = 0;
	int 			r4 = 0;
	unsigned int	pYUVBuf1[2], pYUVBuf2[2], pYUVBuf3[2], pYUVBuf4[2];

	FRAMEX_CTX				*pFrameExCtx1, *pFrameExCtx2;	
	FRAMEX_STRM_PTR 		file_strm1, file_strm2, file_strm3, file_strm4;
	SSBSIP_H264_STREAM_INFO stream_info1, stream_info2, stream_info3, stream_info4;		
	pp_params				pp_param1, pp_param2, pp_param3, pp_param4;
	s3c_win_info_t	osd_info_to_driver1, osd_info_to_driver2, osd_info_to_driver3, osd_info_to_driver4;

	struct stat					s1, s2, s3, s4;
	struct fb_fix_screeninfo	lcd_info1, lcd_info2, lcd_info3, lcd_info4;		


	strcpy(in_filename1, H264_INPUT_FILE);
	strcpy(in_filename2, MPEG4_INPUT_FILE);
	strcpy(in_filename3, H263_INPUT_FILE);
	strcpy(in_filename4, VC1_INPUT_FILE);


	if(signal(SIGINT, sig_del) == SIG_ERR) {
		printf("Sinal Error\n");
	}

	// in file open
	in_fd1	= open(in_filename1, O_RDONLY);
	if(in_fd1 < 0) {
		printf("H.264 Input file open failed\n");
		return -1;
	}

	// get input file size
	fstat(in_fd1, &s1);
	file_size1 = s1.st_size;

	// mapping input file to memory
	in_addr1 = (char *)mmap(0, file_size1, PROT_READ, MAP_SHARED, in_fd1, 0);
	if(in_addr1 == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}

	// Post processor open
	pp_fd1 = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd1 < 0)
	{
		printf("Post processor open error\n");
		return -1;
	}

	// LCD frame buffer open
	fb_fd1 = open(FB_DEV_NAME, O_RDWR|O_NDELAY);
	if(fb_fd1 < 0)
	{
		printf("LCD frame buffer open error\n");
		return -1;
	}

	///////////////////////////////////
	// FrameExtractor Initialization //
	///////////////////////////////////
	pFrameExCtx1 = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_h264, sizeof(delimiter_h264), 1);   
	file_strm1.p_start = file_strm1.p_cur = (unsigned char *)in_addr1;
	file_strm1.p_end = (unsigned char *)(in_addr1 + file_size1);
	FrameExtractorFirst(pFrameExCtx1, &file_strm1);


	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipH264DecodeInit)    ///
	//////////////////////////////////////
	handle1 = SsbSipH264DecodeInit();
	if (handle1 == NULL) {
		printf("H264_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipH264DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf1 = SsbSipH264DecodeGetInBuf(handle1, nFrameLeng1);
	if (pStrmBuf1 == NULL) {
		printf("SsbSipH264DecodeGetInBuf Failed.\n");
		SsbSipH264DecodeDeInit(handle1);
		return -1;
	}

	////////////////////////////////////
	//  H264 CONFIG stream extraction //
	////////////////////////////////////
	nFrameLeng1 = ExtractConfigStreamH264(pFrameExCtx1, &file_strm1, pStrmBuf1, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipH264DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipH264DecodeExe(handle1, nFrameLeng1) != SSBSIP_H264_DEC_RET_OK) {
		printf("H.264 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipH264DecodeGetConfig(handle1, H264_DEC_GETCONF_STREAMINFO, &stream_info1);

	//printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info1.width, stream_info1.height);


	// set post processor configuration
	pp_param1.SrcFullWidth	= stream_info1.width;
	pp_param1.SrcFullHeight	= stream_info1.height;
	pp_param1.SrcStartX		= 0;
	pp_param1.SrcStartY		= 0;
	pp_param1.SrcWidth		= pp_param1.SrcFullWidth;
	pp_param1.SrcHeight		= pp_param1.SrcFullHeight;
	pp_param1.SrcCSpace		= YC420;
	pp_param1.DstStartX		= 0;
	pp_param1.DstStartY		= 0;
	pp_param1.DstFullWidth	= lcdsize[lcdnum][0] / 2;//400;		// destination width
	pp_param1.DstFullHeight	= lcdsize[lcdnum][1] / 2;// 240;		// destination height
	pp_param1.DstWidth		= pp_param1.DstFullWidth;
	pp_param1.DstHeight		= pp_param1.DstFullHeight;
	pp_param1.DstCSpace		= RGB16;
#ifdef RGB24BPP
	pp_param1.DstCSpace		= RGB24;
#endif
	pp_param1.OutPath		= POST_DMA;
	pp_param1.Mode			= ONE_SHOT;


	// get LCD frame buffer address
	fb_size1 = pp_param1.DstFullWidth * pp_param1.DstFullHeight * 2;	// RGB565
#ifdef RGB24BPP
	fb_size1 = pp_param1.DstFullWidth * pp_param1.DstFullHeight * 4;	// RGB888
#endif

	fb_addr1 = (char *)mmap(0, fb_size1, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd1, 0);
	if (fb_addr1 == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver1.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver1.Bpp			= 24;	// RGB16
#endif
	osd_info_to_driver1.LeftTop_x	= 0;	
	osd_info_to_driver1.LeftTop_y	= 0;
	osd_info_to_driver1.Width		=  lcdsize[lcdnum][0] / 2;// 400;	// display width
	osd_info_to_driver1.Height		=  lcdsize[lcdnum][1] / 2;//240;	// display height

	// set OSD's information 
	if(ioctl(fb_fd1, SET_OSD_INFO, &osd_info_to_driver1)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd1, SET_OSD_START);


	// in file open
	in_fd2	= open(in_filename2, O_RDONLY);
	if(in_fd2 < 0) {
		printf("MPEG4 Input file open failed\n");
		return -1;
	}

	// get input file size
	fstat(in_fd2, &s2);
	file_size2 = s2.st_size;

	// mapping input file to memory
	in_addr2 = (char *)mmap(0, file_size2, PROT_READ, MAP_SHARED, in_fd2, 0);
	if(in_addr2 == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}

	// Post processor open
	pp_fd2 = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd2 < 0)
	{
		printf("Post processor open error\n");
		return -1;
	}

	// LCD frame buffer open
	fb_fd2 = open(FB_DEV_NAME1, O_RDWR|O_NDELAY);
	if(fb_fd2 < 0)
	{
		printf("LCD frame buffer open error\n");
		return -1;
	}

	///////////////////////////////////
	// FrameExtractor Initialization //
	///////////////////////////////////
	pFrameExCtx2 = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_mpeg4, sizeof(delimiter_mpeg4), 1);   
	file_strm2.p_start = file_strm2.p_cur = (unsigned char *)in_addr2;
	file_strm2.p_end = (unsigned char *)(in_addr2 + file_size2);
	FrameExtractorFirst(pFrameExCtx2, &file_strm2);


	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipMPEG4DecodeInit)    ///
	//////////////////////////////////////
	handle2 = SsbSipMPEG4DecodeInit();
	if (handle2 == NULL) {
		printf("MPEG4_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf2 = SsbSipMPEG4DecodeGetInBuf(handle2, nFrameLeng2);
	if (pStrmBuf2 == NULL) {
		printf("SsbSipMPEG4DecodeGetInBuf Failed.\n");
		SsbSipMPEG4DecodeDeInit(handle2);
		return -1;
	}

	////////////////////////////////////
	//  MPEG4 CONFIG stream extraction //
	////////////////////////////////////
	nFrameLeng2 = ExtractConfigStreamMpeg4(pFrameExCtx2, &file_strm2, pStrmBuf2, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipMPEG4DecodeExe(handle2, nFrameLeng2) != SSBSIP_MPEG4_DEC_RET_OK) {
		printf("MPEG-4 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipMPEG4DecodeGetConfig(handle2, MPEG4_DEC_GETCONF_STREAMINFO, &stream_info2);

	//printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info2.width, stream_info2.height);

	memset(&pp_param2, 0, sizeof(pp_params));

	// set post processor configuration
	pp_param2.SrcFullWidth	= stream_info2.width;
	pp_param2.SrcFullHeight	= stream_info2.height;
	pp_param2.SrcStartX		= 0;
	pp_param2.SrcStartY		= 0;
	pp_param2.SrcWidth		= pp_param2.SrcFullWidth;
	pp_param2.SrcHeight		= pp_param2.SrcFullHeight;
	pp_param2.SrcCSpace		= YC420;
	pp_param2.DstStartX		= 0;
	pp_param2.DstStartY		= 0;
	pp_param2.DstFullWidth	= lcdsize[lcdnum][0] / 2;//400;		// destination width
	pp_param2.DstFullHeight	= lcdsize[lcdnum][1] / 2;//240;		// destination height
	pp_param2.DstWidth		= pp_param2.DstFullWidth;
	pp_param2.DstHeight		= pp_param2.DstFullHeight;
	pp_param2.DstCSpace		= RGB16;
#ifdef RGB24BPP
	pp_param2.DstCSpace		= RGB24;
#endif
	pp_param2.OutPath		= POST_DMA;
	pp_param2.Mode			= ONE_SHOT;


	// get LCD frame buffer address
	fb_size2 = pp_param2.DstFullWidth * pp_param2.DstFullHeight * 2;	// RGB565
#ifdef RGB24BPP
	fb_size2 = pp_param2.DstFullWidth * pp_param2.DstFullHeight * 4;	// RGB888
#endif

	fb_addr2 = (char *)mmap(0, fb_size2, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd2, 0);
	if (fb_addr2 == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver2.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver2.Bpp			= 24;	// RGB24
#endif
	osd_info_to_driver2.LeftTop_x	= lcdsize[lcdnum][0] / 2;//400;	
	osd_info_to_driver2.LeftTop_y	= 0;
	osd_info_to_driver2.Width		= lcdsize[lcdnum][0] / 2;//400;	// display width
	osd_info_to_driver2.Height		= lcdsize[lcdnum][1] / 2;///240;	// display height

	// set OSD's information 
	if(ioctl(fb_fd2, SET_OSD_INFO, &osd_info_to_driver2)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd2, SET_OSD_START);


	// in file open
	in_fd3	= open(in_filename3, O_RDONLY);
	if(in_fd3 < 0) {
		printf("H.263 Input file open failed\n");
		return -1;
	}

	// get input file size
	fstat(in_fd3, &s3);
	file_size3 = s3.st_size;

	// mapping input file to memory
	in_addr3 = (char *)mmap(0, file_size3, PROT_READ, MAP_SHARED, in_fd3, 0);
	if(in_addr3 == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}

	// Post processor open
	pp_fd3 = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd3 < 0)
	{
		printf("Post processor open error\n");
		return -1;
	}

	// LCD frame buffer open
	fb_fd3 = open(FB_DEV_NAME2, O_RDWR|O_NDELAY);
	if(fb_fd3 < 0)
	{
		printf("LCD frame buffer open error\n");
		return -1;
	}

	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipMPEG4DecodeInit)    ///
	//////////////////////////////////////
	handle3 = SsbSipMPEG4DecodeInit();
	if (handle3 == NULL) {
		printf("H263_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf3 = SsbSipMPEG4DecodeGetInBuf(handle3, 200000);
	if (pStrmBuf3 == NULL) {
		printf("SsbSipMPEG4DecodeGetInBuf Failed.\n");
		SsbSipMPEG4DecodeDeInit(handle3);
		return -1;
	}


	////////////////////////////////////
	//  MPEG4 CONFIG stream extraction //
	////////////////////////////////////
	file_strm3.p_start = file_strm3.p_cur = (unsigned char *)in_addr3;
	file_strm3.p_end = (unsigned char *)(in_addr3 + file_size3);
	nFrameLeng3 = ExtractConfigStreamH263(&file_strm3, pStrmBuf3, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipMPEG4DecodeExe(handle3, nFrameLeng3) != SSBSIP_MPEG4_DEC_RET_OK) {
		printf("H.263 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipMPEG4DecodeGetConfig(handle3, MPEG4_DEC_GETCONF_STREAMINFO, &stream_info3);

	//printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info3.width, stream_info3.height);


	// set post processor configuration
	pp_param3.SrcFullWidth	= stream_info3.width;
	pp_param3.SrcFullHeight	= stream_info3.height;
	pp_param3.SrcStartX		= 0;
	pp_param3.SrcStartY		= 0;
	pp_param3.SrcWidth		= pp_param3.SrcFullWidth;
	pp_param3.SrcHeight		= pp_param3.SrcFullHeight;
	pp_param3.SrcCSpace		= YC420;
	pp_param3.DstStartX		= 0;
	pp_param3.DstStartY		= 0;
	pp_param3.DstFullWidth	= lcdsize[lcdnum][0] / 2;//400;		// destination width
	pp_param3.DstFullHeight	= lcdsize[lcdnum][1] / 2;//240;		// destination height
	pp_param3.DstWidth		= pp_param3.DstFullWidth;
	pp_param3.DstHeight		= pp_param3.DstFullHeight;
	pp_param3.DstCSpace		= RGB16;
#ifdef RGB24BPP
	pp_param3.DstCSpace		= RGB24;
#endif
	pp_param3.OutPath		= POST_DMA;
	pp_param3.Mode			= ONE_SHOT;


	// get LCD frame buffer address
	fb_size3 = pp_param3.DstFullWidth * pp_param3.DstFullHeight * 2;	// RGB565
#ifdef RGB24BPP
	fb_size3 = pp_param3.DstFullWidth * pp_param3.DstFullHeight * 4;	// RGB888
#endif

	fb_addr3 = (char *)mmap(0, fb_size3, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd3, 0);
	if (fb_addr3 == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver3.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver3.Bpp			= 24;	// RGB24
#endif
	osd_info_to_driver3.LeftTop_x	= 0;	
	osd_info_to_driver3.LeftTop_y	= lcdsize[lcdnum][1] / 2;///240;
	osd_info_to_driver3.Width		= lcdsize[lcdnum][0] / 2;///400;	// display width
	osd_info_to_driver3.Height		= lcdsize[lcdnum][1] / 2;///240;	// display height

	// set OSD's information 
	if(ioctl(fb_fd3, SET_OSD_INFO, &osd_info_to_driver3)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd3, SET_OSD_START);


	// in file open
	in_fd4	= open(in_filename4, O_RDONLY);
	if(in_fd4 < 0) {
		printf("VC1 Input file open failed\n");
		return -1;
	}

	// get input file size
	fstat(in_fd4, &s4);
	file_size4 = s4.st_size;

	// mapping input file to memory
	in_addr4 = (char *)mmap(0, file_size4, PROT_READ, MAP_SHARED, in_fd4, 0);
	if(in_addr4 == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}

	// Post processor open
	pp_fd4 = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if(pp_fd4 < 0)
	{
		printf("Post processor open error\n");
		return -1;
	}

	// LCD frame buffer open
	fb_fd4 = open(FB_DEV_NAME3, O_RDWR|O_NDELAY);
	if(fb_fd4 < 0)
	{
		printf("LCD frame buffer open error\n");
		return -1;
	}

	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipMPEG4DecodeInit)    ///
	//////////////////////////////////////
	handle4 = SsbSipVC1DecodeInit();
	if (handle4 == NULL) {
		printf("VC1_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf4 = SsbSipVC1DecodeGetInBuf(handle4, 200000);
	if (pStrmBuf4 == NULL) {
		printf("SsbSipVC1DecodeGetInBuf Failed.\n");
		SsbSipVC1DecodeDeInit(handle4);
		return -1;
	}


	////////////////////////////////////
	//  MPEG4 CONFIG stream extraction //
	////////////////////////////////////
	file_strm4.p_start = file_strm4.p_cur = (unsigned char *)in_addr4;
	file_strm4.p_end = (unsigned char *)(in_addr4 + file_size4);
	nFrameLeng4 = ExtractConfigStreamVC1(&file_strm4, pStrmBuf4, INPUT_BUFFER_SIZE, NULL);


	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	r4 = SsbSipVC1DecodeExe(handle4, nFrameLeng4);
	if (r4 != SSBSIP_VC1_DEC_RET_OK) {
		printf("VC-1 Decoder Configuration Failed. : %d\n", r4);
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipVC1DecodeGetConfig(handle4, VC1_DEC_GETCONF_STREAMINFO, &stream_info4);

	//printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info4.width, stream_info4.height);


	// set post processor configuration
	pp_param4.SrcFullWidth	= stream_info4.width;
	pp_param4.SrcFullHeight	= stream_info4.height;
	pp_param4.SrcStartX		= 0;
	pp_param4.SrcStartY		= 0;
	pp_param4.SrcWidth		= pp_param4.SrcFullWidth;
	pp_param4.SrcHeight		= pp_param4.SrcFullHeight;
	pp_param4.SrcCSpace		= YC420;
	pp_param4.DstStartX		= 0;
	pp_param4.DstStartY		= 0;
	pp_param4.DstFullWidth	= lcdsize[lcdnum][0] / 2;///400;		// destination width
	pp_param4.DstFullHeight	= lcdsize[lcdnum][1] / 2;///240;		// destination height
	pp_param4.DstWidth		= pp_param4.DstFullWidth;
	pp_param4.DstHeight		= pp_param4.DstFullHeight;
	pp_param4.DstCSpace		= RGB16;
#ifdef RGB24BPP
	pp_param4.DstCSpace		= RGB24;
#endif
	pp_param4.OutPath		= POST_DMA;
	pp_param4.Mode			= ONE_SHOT;


	// get LCD frame buffer address
	fb_size4 = pp_param4.DstFullWidth * pp_param4.DstFullHeight * 2;	// RGB565
#ifdef RGB24BPP
	fb_size4 = pp_param4.DstFullWidth * pp_param4.DstFullHeight * 4;	// RGB888
#endif

	fb_addr4 = (char *)mmap(0, fb_size4, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd4, 0);
	if (fb_addr4 == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver4.Bpp			= 16;	// RGB16
#ifdef RGB24BPP
	osd_info_to_driver4.Bpp			= 24;	// RGB24
#endif
	osd_info_to_driver4.LeftTop_x	= lcdsize[lcdnum][0] / 2;///400;	
	osd_info_to_driver4.LeftTop_y	= lcdsize[lcdnum][1] / 2;///240;
	osd_info_to_driver4.Width		= lcdsize[lcdnum][0] / 2;///400;	// display width
	osd_info_to_driver4.Height		= lcdsize[lcdnum][1] / 2;///240;	// display height

	// set OSD's information 
	if(ioctl(fb_fd4, SET_OSD_INFO, &osd_info_to_driver4)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd4, SET_OSD_START);
	printf("\n============= H.264 MPEG4 H.263 VC-1 File Decodec Test =============\n");
	//printf("Forlinx Embedded, %s\n", VERSION);

	printf("\n[4-windows display]   Forlinx Embedded, %s\n", VERSION);
	printf("Using IP : MFC, Post processor, LCD\n");
	printf("**********************************************************************\n");
	printf("*                                 *                                  *\n");
	printf("* Frame buffer      : 0           * Frame buffer      : 1            *\n");
	printf("* Codec             : H.264       * Codec             : MPEG4        *\n");
	printf("* Input filename    : veggie.264  * Input filename    : shrek.m4v    *\n");
	printf("* Input vector size : QVGA        * Input vector size : QVGA         *\n");
	printf("* Display size      : %dx%d     * Display size      : %dx%d      *\n",lcdsize[lcdnum][0] / 2, lcdsize[lcdnum][1] / 2, lcdsize[lcdnum][0] / 2, lcdsize[lcdnum][0] / 2);
	printf("* Bitrate           : 460 Kbps    * Bitrate           : 482 Kbps     *\n");
	printf("* FPS               : 30          * FPS               : 24           *\n");
	printf("*                                 *                                  *\n");
	printf("**********************************************************************\n");
	printf("*                                 *                                  *\n");
	printf("* Frame buffer      : 2           * Frame buffer      : 3            *\n");
	printf("* Codec             : H.263       * Codec             : VC-1         *\n");
	printf("* Input filename    : iron.263    * Input filename    : test2_0.rcv  *\n");
	printf("* Input vector size : QVGA        * Input vector size : QVGA         *\n");
	printf("* Display size      : %dx%d     * Display size      : %dx%d      *\n",lcdsize[lcdnum][0] / 2, lcdsize[lcdnum][1] / 2, lcdsize[lcdnum][0] / 2, lcdsize[lcdnum][1] / 2);
	printf("* Bitrate           : 460 Kbps    * Bitrate           : 460 Kbps     *\n");
	printf("* FPS               : 30          * FPS               : 30           *\n");
	printf("*                                 *                                  *\n");
	printf("**********************************************************************\n");


	while(!last_frame1 || !last_frame2 || !last_frame3 || !last_frame4)
	{
		/***************************** H.264 Display *******************************/

		if (last_frame1 != 1) {
			//////////////////////////////////
			///       5. DECODE            ///
			///    (SsbSipH264DecodeExe)   ///
			//////////////////////////////////
			if (SsbSipH264DecodeExe(handle1, nFrameLeng1) != SSBSIP_H264_DEC_RET_OK)
				break;


			//////////////////////////////////////////////
			///    6. Obtaining the Output Buffer      ///
			///      (SsbSipH264DecodeGetOutBuf)       ///
			//////////////////////////////////////////////
			SsbSipH264DecodeGetConfig(handle1, H264_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf1);


			/////////////////////////////
			// Next H.264 VIDEO stream //
			/////////////////////////////
			nFrameLeng1 = NextFrameH264(pFrameExCtx1, &file_strm1, pStrmBuf1, INPUT_BUFFER_SIZE, NULL);
			if (nFrameLeng1 < 4) {
				last_frame1 = 1;
				//break;
			}

			// Post processing
			// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
			// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
			pp_param1.SrcFrmSt		= pYUVBuf1[0];	// MFC output buffer
			ioctl(fb_fd1, FBIOGET_FSCREENINFO, &lcd_info1);
			pp_param1.DstFrmSt		= lcd_info1.smem_start;			// LCD frame buffer
			ioctl(pp_fd1, S3C_PP_SET_PARAMS, &pp_param1);
			ioctl(pp_fd1, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param1);
			ioctl(pp_fd1, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param1);
			ioctl(pp_fd1, S3C_PP_START);


		}


		/***************************** MPEG4 Display *******************************/

		if (last_frame2 != 1) {
			//////////////////////////////////
			///       5. DECODE            ///
			///    (SsbSipMPEG4DecodeExe)   ///
			//////////////////////////////////
			if (SsbSipMPEG4DecodeExe(handle2, nFrameLeng2) != SSBSIP_MPEG4_DEC_RET_OK)
				break;


			//////////////////////////////////////////////
			///    6. Obtaining the Output Buffer      ///
			///      (SsbSipMPEG4DecodeGetOutBuf)       ///
			//////////////////////////////////////////////
			SsbSipMPEG4DecodeGetConfig(handle2, MPEG4_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf2);


			/////////////////////////////
			// Next MPEG4 VIDEO stream //
			/////////////////////////////
			nFrameLeng2 = NextFrameMpeg4(pFrameExCtx2, &file_strm2, pStrmBuf2, INPUT_BUFFER_SIZE, NULL);
			if (nFrameLeng2 < 4) {
				last_frame2 = 1;
				//break;
			}

			// Post processing
			// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
			// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
			pp_param2.SrcFrmSt		= pYUVBuf2[0];	// MFC output buffer
			ioctl(fb_fd2, FBIOGET_FSCREENINFO, &lcd_info2);
			pp_param2.DstFrmSt		= lcd_info2.smem_start;			// LCD frame buffer
			ioctl(pp_fd2, S3C_PP_SET_PARAMS, &pp_param2);
			ioctl(pp_fd2, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param2);
			ioctl(pp_fd2, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param2);
			ioctl(pp_fd2, S3C_PP_START);		

		}


		/***************************** H.263 Display *******************************/

		if (last_frame3 != 1) {
			//////////////////////////////////
			///       5. DECODE            ///
			///    (SsbSipMPEG4DecodeExe)   ///
			//////////////////////////////////
			if (SsbSipMPEG4DecodeExe(handle3, nFrameLeng3) != SSBSIP_MPEG4_DEC_RET_OK)
				break;


			//////////////////////////////////////////////
			///    6. Obtaining the Output Buffer      ///
			///      (SsbSipMPEG4DecodeGetOutBuf)       ///
			//////////////////////////////////////////////
			SsbSipMPEG4DecodeGetConfig(handle3, MPEG4_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf3);


			/////////////////////////////
			// Next MPEG4 VIDEO stream //
			/////////////////////////////
			nFrameLeng3 = NextFrameH263(&file_strm3, pStrmBuf3, INPUT_BUFFER_SIZE, NULL);

			if (nFrameLeng3 < 4) {
				last_frame3 = 1;	
				//break;
			}

			// Post processing
			// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
			// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
			pp_param3.SrcFrmSt		= pYUVBuf3[0];	// MFC output buffer
			ioctl(fb_fd3, FBIOGET_FSCREENINFO, &lcd_info3);
			pp_param3.DstFrmSt		= lcd_info3.smem_start;			// LCD frame buffer
			ioctl(pp_fd3, S3C_PP_SET_PARAMS, &pp_param3);
			ioctl(pp_fd3, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param3);
			ioctl(pp_fd3, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param3);
			ioctl(pp_fd3, S3C_PP_START);

		}



		/***************************** VC-1 Display *******************************/

		if (last_frame4 != 1) {
			//////////////////////////////////
			///       5. DECODE            ///
			///    (SsbSipMPEG4DecodeExe)   ///
			//////////////////////////////////
			if (SsbSipVC1DecodeExe(handle4, nFrameLeng4) != SSBSIP_VC1_DEC_RET_OK)
				break;


			//////////////////////////////////////////////
			///    6. Obtaining the Output Buffer      ///
			///      (SsbSipMPEG4DecodeGetOutBuf)       ///
			//////////////////////////////////////////////
			SsbSipVC1DecodeGetConfig(handle4, VC1_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf4);


			/////////////////////////////
			// Next MPEG4 VIDEO stream //
			/////////////////////////////
			nFrameLeng4 = NextFrameVC1(&file_strm4, pStrmBuf4, INPUT_BUFFER_SIZE, NULL);

			if (nFrameLeng4 < 4) {
				last_frame4 = 1;
				//break;
			}

			// Post processing
			// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
			// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
			pp_param4.SrcFrmSt		= pYUVBuf4[0];	// MFC output buffer
			ioctl(fb_fd4, FBIOGET_FSCREENINFO, &lcd_info4);
			pp_param4.DstFrmSt		= lcd_info4.smem_start;			// LCD frame buffer
			ioctl(pp_fd4, S3C_PP_SET_PARAMS, &pp_param4);
			ioctl(pp_fd4, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param4);
			ioctl(pp_fd4, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param4);
			ioctl(pp_fd4, S3C_PP_START);

		}
	}


	ioctl(fb_fd1, SET_OSD_STOP);
	ioctl(fb_fd2, SET_OSD_STOP);
	ioctl(fb_fd3, SET_OSD_STOP);
	ioctl(fb_fd4, SET_OSD_STOP);

	SsbSipH264DecodeDeInit(handle1);

	munmap(in_addr1, file_size1);
	munmap(fb_addr1, fb_size1);
	close(pp_fd1);
	close(fb_fd1);
	close(in_fd1);

	SsbSipMPEG4DecodeDeInit(handle2);

	munmap(in_addr2, file_size2);
	munmap(fb_addr2, fb_size2);
	close(pp_fd2);
	close(fb_fd2);
	close(in_fd2);

	SsbSipMPEG4DecodeDeInit(handle3);

	munmap(in_addr3, file_size3);
	munmap(fb_addr3, fb_size3);
	close(pp_fd3);
	close(fb_fd3);
	close(in_fd3);

	SsbSipVC1DecodeDeInit(handle4);

	munmap(in_addr4, file_size4);
	munmap(fb_addr4, fb_size4);
	close(pp_fd4);
	close(fb_fd4);
	close(in_fd4);


	return 0;
}


static void sig_del(int signo)
{
	printf("[4 windows display] signal handling\n");


	ioctl(fb_fd1, SET_OSD_STOP);
	ioctl(fb_fd2, SET_OSD_STOP);
	ioctl(fb_fd3, SET_OSD_STOP);
	ioctl(fb_fd4, SET_OSD_STOP);
	

	SsbSipH264DecodeDeInit(handle1);
	SsbSipMPEG4DecodeDeInit(handle2);
	SsbSipMPEG4DecodeDeInit(handle3);
	SsbSipVC1DecodeDeInit(handle4);


	munmap(in_addr1, file_size1);
	munmap(fb_addr1, fb_size1);
	munmap(in_addr2, file_size2);
	munmap(fb_addr2, fb_size2);
	munmap(in_addr3, file_size3);
	munmap(fb_addr3, fb_size3);
	munmap(in_addr4, file_size4);
	munmap(fb_addr4, fb_size4);


	close(pp_fd1);
	close(fb_fd1);
	close(in_fd1);
	close(pp_fd2);
	close(fb_fd2);
	close(in_fd2);
	close(pp_fd3);
	close(fb_fd3);
	close(in_fd3);
	close(pp_fd4);
	close(fb_fd4);
	close(in_fd4);

	exit(1);
}


