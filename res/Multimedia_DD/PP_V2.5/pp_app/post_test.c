#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/vt.h>
#include "./post_test.h"

#define DEVICE_FILE_NAME	"/dev/misc/s3c-pp"

int main(int argc, char **argv)
{
	int			dev_fd, in_fd, out_fd;
	int			file_size;
	int			buf_size;
	int			out_size;
	char 		*in_addr;
	char		*in_buf, *out_buf;
	struct stat	s;
	pp_params 	pp_param;


	if(argc != 11 ) {
		printf("Check number of arguments!!!\n");
		printf("Usage : [src_width] [src_height] [src_format] [dst_width] [dst_height] [dst_format] ");
		printf("[out_path] [mode] [in file name] [out file name]\n\n");
		printf("[src/dst_format] : 6(RGB16), 9(RGB24), 12(420YCbCr), 14(422CRYCBY)\n");
		printf("                   15(422CBYCRY), 16(422YCRYCB), 17(422YCBYCR)\n");
		printf("[out_path] : 0(DMA), 1(FIFO)\n");
		printf("[mode] : 0(ONE-SHOT), 1(FREE-RUN)\n");

		return -1;
	}

	// set post processor configuration
	pp_param.SrcFullWidth	= atoi(argv[1]);
	pp_param.SrcFullHeight	= atoi(argv[2]);
	pp_param.SrcStartX		= 0;
	pp_param.SrcStartY		= 0;
	pp_param.SrcWidth		= pp_param.SrcFullWidth;
	pp_param.SrcHeight		= pp_param.SrcFullHeight;
	pp_param.SrcCSpace		= atoi(argv[3]);
	pp_param.DstStartX		= 0;
	pp_param.DstStartY		= 0;
	pp_param.DstFullWidth	= atoi(argv[4]);
	pp_param.DstFullHeight	= atoi(argv[5]);
	pp_param.DstWidth		= pp_param.DstFullWidth;
	pp_param.DstHeight		= pp_param.DstFullHeight;
	pp_param.DstCSpace		= atoi(argv[6]);
	pp_param.OutPath		= atoi(argv[7]);
	pp_param.Mode			= atoi(argv[8]);


	// open in/out file
	in_fd = open(argv[9], O_RDONLY);
	out_fd	= open(argv[10], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if((in_fd < 0) || (out_fd < 0)) {
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

	// open post processor 
	dev_fd = open(DEVICE_FILE_NAME, O_RDWR|O_NDELAY);
	if(dev_fd < 0)
	{
		printf("Post processor open error\n");
		return -1;
	}

	// in_buf is post processor input buffer
	buf_size = ioctl(dev_fd, PPROC_GET_BUF_SIZE);
	in_buf = (char *) mmap(0, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0);
	if(in_buf == NULL) {
		printf("Post processor mmap failed\n");
		return -1;
	}
	out_buf = in_buf + ioctl(dev_fd, PPROC_GET_INBUF_SIZE);

	memcpy(in_buf, in_addr, file_size); 
	
	pp_param.SrcFrmSt = ioctl(dev_fd, PPROC_GET_PHY_INBUF_ADDR);
	pp_param.DstFrmSt =	pp_param.SrcFrmSt + ioctl(dev_fd, PPROC_GET_INBUF_SIZE);

	ioctl(dev_fd, PPROC_SET_PARAMS, &pp_param);
	ioctl(dev_fd, PPROC_START);	

	out_size = ioctl(dev_fd, PPROC_GET_OUT_DATA_SIZE);
	write(out_fd, out_buf, out_size);
	
	munmap(in_buf, buf_size);
	
	close(dev_fd);
	close(in_fd);
	close(out_fd);
	
	return 0;
}
