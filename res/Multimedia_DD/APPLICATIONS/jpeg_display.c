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
#include <math.h>

#include "LogMsg.h"
#include "JPGApi.h"
#include "performance.h"
#include "lcd.h"
#include "post.h"


#define JPEG_INPUT_FILE		"./TestVectors/test_420_1600_1200.jpg"
static int lcdsize[6][2] = {{320,240}, {480,272}, {640,480}, {800,480}, {800,600},{1024,768}};


#ifdef RGB24BPP
	#define LCD_BPP			24
#else
	#define LCD_BPP			16
#endif


static int g_LCD_Width =	0;
static int g_LCD_Height=	0;


static int fb_init(int win_num, int bpp, int x, int y, int width, int height, unsigned int *addr);


/*
 *******************************************************************************
Name            : TestDecoder
Description     : To test Decoder
Parameter       : imageType - JPG_YCBYCR or JPG_RGB16
Return Value    : void
 *******************************************************************************
 */
int Forlinx_Test_Jpeg_Display(int argc, char **argv, int lcdnum)
{
	int 	fb_fd;
	int 	pp_fd;
	int		in_fd;
	int		buf_size;
	int		handle;
	long 	streamSize;
	char 	*InBuf = NULL;
	char 	*OutBuf = NULL;
	char	*pp_in_buf;
	char	*in_addr;
	char 	*pp_addr;
	UINT32 	fileSize;	
	INT32 	width, height, samplemode;
	unsigned int	fb_addr;
	s3c_pp_params_t 		pp_param;
	JPEG_ERRORTYPE 	ret;
	struct stat		s;
	struct s3c_fb_dma_info	fb_info;
	
#ifdef FPS
	struct timeval start;
	struct timeval stop;
	unsigned int	time = 0;
#endif


        g_LCD_Width = lcdsize[lcdnum][0];
        g_LCD_Height =lcdsize[lcdnum][1];

      //  printf("lcdwidth=%d,lcdheight=%d \n",g_LCD_Width,g_LCD_Height);

	/* LCD frame buffer initialization */
        fb_fd = fb_init(0, LCD_BPP, 0, 0, g_LCD_Width, g_LCD_Height, &fb_addr);
	if (fb_fd < 0) {
		printf("frame buffer open error\n");
		return -1;
	}

	/* Post processor open */
	pp_fd = open(PP_DEV_NAME, O_RDWR|O_NDELAY);
	if (pp_fd < 0) {
		printf("post processor open error\n");
		return -1;
	}

	/* Get post processor's input buffer address */
	buf_size = ioctl(pp_fd, S3C_PP_GET_RESERVED_MEM_SIZE);
	pp_addr = (char *)mmap(0, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, pp_fd, 0);
	if(pp_addr == NULL) {
		printf("Post processor mmap failed\n");
		return -1;
	}
	pp_in_buf = pp_addr;

	/* Input file(JPEG file) open */
	in_fd = open(JPEG_INPUT_FILE, O_RDONLY);
	if (in_fd < 0) {
		printf("Input file open error\n");
		return -1;
	}

	fstat(in_fd, &s);
	fileSize = s.st_size;

	/* mapping input file to memory */
	in_addr = (char *)mmap(0, fileSize, PROT_READ, MAP_SHARED, in_fd, 0);
	if (in_addr == NULL) {
		printf("Input file memory mapping failed\n");
		return -1;
	}


#ifdef FPS
	gettimeofday(&start, NULL);
#endif

	/* JPEG driver initialization */
	handle = SsbSipJPEGDecodeInit();
	if(handle < 0)
        {
            printf("SsbSipJPEGDecodeInit error \n");
            return -1;
        }

#ifdef FPS
	gettimeofday(&stop, NULL);
	time += measureTime(&start, &stop);
#endif


	/* Get jpeg's input buffer address */
        InBuf = SsbSipJPEGGetDecodeInBuf(handle, fileSize);
	if(InBuf == NULL){
		printf("Input buffer is NULL\n");
		return -1;
	}


	/* Put JPEG frame to Input buffer */
        memcpy(InBuf, in_addr, fileSize);
        close(in_fd);


#ifdef FPS
	gettimeofday(&start, NULL);
#endif

	/* Decode JPEG frame */
	ret = SsbSipJPEGDecodeExe(handle);
	if (ret != JPEG_OK) {
		printf("Decoding failed\n");
		return -1;
	}

#ifdef FPS
	gettimeofday(&stop, NULL);
	time += measureTime(&start, &stop);
	printf("[JPEG Decoding Performance] Elapsed time : %u\n", time);
	time = 0;
#endif


	/* Get Output buffer address */
        OutBuf = SsbSipJPEGGetDecodeOutBuf(handle, &streamSize);
	if(OutBuf == NULL){
		printf("Output buffer is NULL\n");
		return -1;
	}


	/* Get decode config. */
	SsbSipJPEGGetConfig(JPEG_GET_DECODE_WIDTH, &width);
	SsbSipJPEGGetConfig(JPEG_GET_DECODE_HEIGHT, &height);
	SsbSipJPEGGetConfig(JPEG_GET_SAMPING_MODE, &samplemode);



	/* Set post processor */
	pp_param.src_full_width		= width;
	pp_param.src_full_height	= height;
	pp_param.src_start_x		= 0;
	pp_param.src_start_y		= 0;
	pp_param.src_width			= pp_param.src_full_width;
	pp_param.src_height			= pp_param.src_full_height;
	pp_param.src_color_space	= YCBYCR;
	pp_param.dst_start_x		= 0;
	pp_param.dst_start_y		= 0;
        pp_param.dst_full_width		= g_LCD_Width;
        pp_param.dst_full_height	= g_LCD_Height;
	pp_param.dst_width		= pp_param.dst_full_width;
	pp_param.dst_height		= pp_param.dst_full_height;	
	pp_param.dst_color_space		= RGB16;
#ifdef RGB24BPP
	pp_param.dst_color_space		= RGB24;
#endif
	pp_param.out_path		= 0;
	pp_param.scan_mode			= 0;


      //  printf("copy decoded frame to post processor  input buffer \n");

	/* copy decoded frame to post processor's input buffer */
	memcpy(pp_in_buf, OutBuf, width*height*2);
	ioctl(fb_fd, GET_FB_INFO, &fb_info); 
	pp_param.src_buf_addr_phy = ioctl(pp_fd, S3C_PP_GET_RESERVED_MEM_ADDR_PHY);
	pp_param.dst_buf_addr_phy = fb_info.map_dma_f1;


	ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);

// phantom
	ioctl(pp_fd, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param);
	ioctl(pp_fd, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param);

//----


	ioctl(pp_fd, S3C_PP_START);

	//////////////////////////////////////////////////////////////
	// 9. finalize handle                                      //
	//////////////////////////////////////////////////////////////
	SsbSipJPEGDecodeDeInit(handle);
	close(pp_fd);
	close(fb_fd);
	
	return 0;
}


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

