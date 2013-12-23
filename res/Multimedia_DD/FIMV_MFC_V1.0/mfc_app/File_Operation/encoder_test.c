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


int Test_Encoder(int argc, char **argv)
{
	int			dev_fd, in_fd, out_fd;
	char		*addr, *in_addr;
	int 		cnt = 0;
	int			file_size;
	int			frame_count;
	int			frame_size;
	char		*in_buf, *out_buf;
	struct stat	s;
	
	// arguments of ioctl
	MFC_ENC_INIT_ARG		enc_init;
	MFC_ENC_EXE_ARG			enc_exe;
	MFC_GET_BUF_ADDR_ARG	get_buf_addr;

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

	if (argc != 8) {
		printf("Usage : mfc <YUV file name> <output filename> <width> <height> ");
		printf("<frame rate> <bitrate> <GOP number>\n");
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

	// mapping shared in/out buffer between App and D/D
	addr = (char *) mmap(0, 
			BUF_SIZE, 
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			dev_fd,
			0
			);
	if (addr == NULL) {
		printf("MFC mmap failed\n");
		return -1;
	}

	// Below ioctl function support multiple slice mode in H.263 encoding case only
	//ioctl(dev_fd, IOCTL_MFC_SET_H263_MULTIPLE_SLICE);

	// set encoding arguments
	enc_init.in_width			= atoi(argv[3]);
	enc_init.in_height			= atoi(argv[4]);
	enc_init.in_frameRateRes	= atoi(argv[5]);
	enc_init.in_frameRateDiv	= 0;
	enc_init.in_bitrate			= atoi(argv[6]);
	enc_init.in_gopNum			= atoi(argv[7]);
	

	ioctl(dev_fd, IOCTL_MFC_H264_ENC_INIT, &enc_init);

	frame_size	= (enc_init.in_width * enc_init.in_height * 3) >> 1;
	frame_count	= file_size / frame_size;

	// get input buffer address
	get_buf_addr.in_usr_data = (int)addr;
	ioctl(dev_fd, IOCTL_MFC_GET_FRAM_BUF_ADDR, &get_buf_addr);
	in_buf = (char *)get_buf_addr.out_buf_addr;

	// get output buffer address
	get_buf_addr.in_usr_data = (int)addr;
	ioctl(dev_fd, IOCTL_MFC_GET_LINE_BUF_ADDR, &get_buf_addr);
	out_buf = (char *)get_buf_addr.out_buf_addr;
	
	while(frame_count > 0) 
	{
		// copy YUV data into input buffer
		memcpy(in_buf, in_addr + frame_size * cnt, frame_size);

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif

		// encoding
		ioctl(dev_fd, IOCTL_MFC_H264_ENC_EXE, &enc_exe);
		printf("Header size = %d\n", enc_exe.out_header_size);	
		
		//if(enc_exe.ret_code != 0) {
		//	printf("ret code : %d\n", enc_exe.ret_code);
		//}
	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif

	#ifndef FPS
		write(out_fd, out_buf, enc_exe.out_encoded_size);
	#endif
	
		frame_count--;
		cnt++;
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
