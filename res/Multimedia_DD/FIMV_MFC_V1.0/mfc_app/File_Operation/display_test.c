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
#include "mfc.h"
#include "MfcDrvParams.h"
#include "post.h"
#include "lcd.h"
#include "performance.h"


int Test_Display(int argc, char **argv)
{
	int			mfc_fd, pp_fd, fb_fd, in_fd;
	char		*addr, *in_addr, *fb_addr;
	int 		cnt = 1;
	int			file_size, fb_size;
	int			remain;
	pp_params	pp_param;
	s3c_win_info_t	osd_info_to_driver;

	struct stat	s;
	struct fb_fix_screeninfo	lcd_info;		
	
	// arguments of MFC ioctl
	MFC_DEC_INIT_ARG			dec_init;
	MFC_DEC_EXE_ARG				dec_exe;
	MFC_GET_BUF_ADDR_ARG		get_buf_addr;

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

	if (argc != 2) {
		printf("Usage : mfc <input filename>\n");
		return -1;
	}

	// in/out file open
	in_fd	= open(argv[1], O_RDONLY);
	if(in_fd < 0) {
		printf("input file open error\n");
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

	// MFC open
	mfc_fd = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (mfc_fd < 0) {
		printf("MFC open error : %d\n", mfc_fd);
		return -1;
	}

	// mapping shared in/out buffer between App and D/D for MFC
	addr = (char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfc_fd, 0);
	if (addr == NULL) {
		printf("MFC mmap failed\n");
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

	// get input buffer address in ring buffer mode
	// When below ioctl function is called for the first time, It returns double buffer size.
	// So, Input buffer will be filled as 2 part unit size
	get_buf_addr.in_usr_data = (int)addr;

	printf("get ring buffer address before\n");
	
	ioctl(mfc_fd, IOCTL_MFC_GET_RING_BUF_ADDR, &get_buf_addr);

	printf("input buffer address : 0x%X\n", get_buf_addr.out_buf_addr);
	printf("input buffer size : %d\n", get_buf_addr.out_buf_size);
	printf("input buffer return code : %d\n", get_buf_addr.ret_code);
	
	remain = file_size;
	
	// copy input stream to input buffer
	memcpy((char *)get_buf_addr.out_buf_addr, in_addr, get_buf_addr.out_buf_size);	
	remain -= get_buf_addr.out_buf_size;
	
	// MFC decoder initialization
	dec_init.in_strmSize = get_buf_addr.out_buf_size;
	ioctl(mfc_fd, IOCTL_MFC_MPEG4_DEC_INIT, &dec_init);
	printf("out_width : %d, out_height : %d\n", dec_init.out_width, dec_init.out_height);

	// set post processor configuration
	pp_param.SrcFullWidth	= dec_init.out_width;
	pp_param.SrcFullHeight	= dec_init.out_height;
	pp_param.SrcCSpace		= YC420;
	pp_param.DstFullWidth	= 800;		// destination width
	pp_param.DstFullHeight	= 480;		// destination height
	pp_param.DstCSpace		= RGB16;
	pp_param.OutPath		= POST_DMA;
	pp_param.Mode			= ONE_SHOT;
	
	// get LCD frame buffer address
	fb_size = pp_param.DstFullWidth * pp_param.DstFullHeight * 2;	// RGB565
	fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_addr == NULL) {
		printf("LCD frame buffer mmap failed\n");
		return -1;
	}

	osd_info_to_driver.Bpp			= 16;	// RGB16
	osd_info_to_driver.LeftTop_x	= 0;	
	osd_info_to_driver.LeftTop_y	= 0;
	osd_info_to_driver.Width		= 800;	// display width
	osd_info_to_driver.Height		= 480;	// display height

	// set OSD's information 
	if(ioctl(fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
		printf("Some problem with the ioctl SET_OSD_INFO\n");
		return -1;
	}

	ioctl(fb_fd, SET_OSD_START);
	
	while(1)
	{
		// if input stream
		get_buf_addr.in_usr_data = (int)addr;
		ioctl(mfc_fd, IOCTL_MFC_GET_RING_BUF_ADDR, &get_buf_addr);
		if(get_buf_addr.out_buf_size > 0) {
			if(remain >= get_buf_addr.out_buf_size) {
				cnt++;
				memcpy((char *)get_buf_addr.out_buf_addr, in_addr + (cnt * (get_buf_addr.out_buf_size)), get_buf_addr.out_buf_size);
				remain -= get_buf_addr.out_buf_size; 
				dec_exe.in_strmSize = get_buf_addr.out_buf_size;
			} else {	
				cnt++;
				memcpy((char *)get_buf_addr.out_buf_addr, in_addr + (cnt * (get_buf_addr.out_buf_size)), remain);
				dec_exe.in_strmSize = remain;
				remain = 0;
			}
		}
		else {
			dec_exe.in_strmSize = 0;
		}

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		// MFC decoding
		ioctl(mfc_fd, IOCTL_MFC_MPEG4_DEC_EXE, &dec_exe);
		if(dec_exe.ret_code < 0) {
			printf("ret code : %d\n", dec_exe.ret_code);
			break;
		}

		// get output buffer address
		ioctl(mfc_fd, IOCTL_MFC_GET_PHY_FRAM_BUF_ADDR, &get_buf_addr);

		// Post processing
		// pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
		// pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
		pp_param.SrcFrmSt		= get_buf_addr.out_buf_addr;	// MFC output buffer
		ioctl(fb_fd, FBIOGET_FSCREENINFO, &lcd_info);
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

	munmap(in_addr, file_size);
	munmap(addr, BUF_SIZE);
	munmap(fb_addr, fb_size);

	close(mfc_fd);
	close(pp_fd);
	close(fb_fd);
	close(in_fd);
	
	return 0;
}

