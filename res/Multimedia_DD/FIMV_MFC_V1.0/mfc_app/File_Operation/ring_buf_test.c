#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "mfc.h"
#include "MfcDrvParams.h"
#include "performance.h"


int Test_Decoder_Ring_Buffer(int argc, char **argv)
{
	int			dev_fd, in_fd, out_fd;
	char		*addr, *in_addr;
	int 		cnt = 1;
	int			file_size;
	int			remain;
	struct stat	s;
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

	// arguments of ioctl
	MFC_DEC_INIT_ARG		dec_init;
	MFC_DEC_EXE_ARG			dec_exe;
	MFC_GET_BUF_ADDR_ARG	get_buf_addr;

	int		r = 0;

	
	if (argc != 4) {
		printf("Usage : mfc <input file name> <output file name> <rotation mode>\n");
		return -1;
	}


	// in/out file open
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		printf("input/output file open error\n");
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
	dev_fd = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (dev_fd < 0) {
		printf("MFC open error : %d\n", dev_fd);
		return -1;
	}

	printf("dev_fd : %d  buf size : %d\n", dev_fd, BUF_SIZE);

	// mapping shared in/out buffer between App and D/D
	addr = (char *) mmap(0, 
			BUF_SIZE, 
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			dev_fd,
			0
			);
	if ((int)addr < 0) {
		printf("MFC mmap failed\n");
		return -1;
	}

	// get input buffer address in ring buffer mode
	// When below ioctl function is called for the first time, It returns double buffer size.
	// So, Input buffer will be filled as 2 part unit size
	get_buf_addr.in_usr_data = (int)addr;

	ioctl(dev_fd, IOCTL_MFC_GET_RING_BUF_ADDR, &get_buf_addr);

	remain = file_size;

	printf("file_size : %d, get_buf_addr.out_buf_addr : 0x%X, get_buf_addr.out_buf_size : %d\n", file_size, get_buf_addr.out_buf_addr, get_buf_addr.out_buf_size);
	

	if (file_size < get_buf_addr.out_buf_size) {
		dec_init.in_strmSize = file_size;
		memcpy((char *)get_buf_addr.out_buf_addr, in_addr, file_size);			
	} else {
		dec_init.in_strmSize = get_buf_addr.out_buf_size;
		memcpy((char *)get_buf_addr.out_buf_addr, in_addr, get_buf_addr.out_buf_size);	
	}

	printf("in_strmSize : %d\n", dec_init.in_strmSize);
	
	//memcpy((char *)get_buf_addr.out_buf_addr, in_addr, get_buf_addr.out_buf_size);	
	remain -= get_buf_addr.out_buf_size;
	printf("remain : %d\n", remain);

	
	// MFC decoder initialization
	ioctl(dev_fd, IOCTL_MFC_VC1_DEC_INIT, &dec_init);
	printf("out_width : %d, out_height : %d\n", dec_init.out_width, dec_init.out_height);

	while(1)
	{
		// if input stream
		ioctl(dev_fd, IOCTL_MFC_GET_RING_BUF_ADDR, &get_buf_addr);

		//printf("dec_exe.in_strmSize : %d\n", get_buf_addr.out_buf_size);

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
			printf("remain : %d\n", remain);
		}
		else {
			dec_exe.in_strmSize = 0;
		}

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
		// MFC decoding
		r = ioctl(dev_fd, IOCTL_MFC_VC1_DEC_EXE, &dec_exe);
		
		if(dec_exe.ret_code < 0) {
			printf("ret code : %d\n", dec_exe.ret_code);
			break;
		}
	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif

		// get output buffer address
		ioctl(dev_fd, IOCTL_MFC_GET_FRAM_BUF_ADDR, &get_buf_addr);

	#ifndef FPS
		write(out_fd, (char *)get_buf_addr.out_buf_addr, get_buf_addr.out_buf_size);
	#endif
	}

	#ifdef FPS
	printf("Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
	#endif

	munmap(in_addr, file_size);
	munmap(addr, BUF_SIZE);
	
	close(dev_fd);
	close(in_fd);
	close(out_fd);
	
	return 0;
}

