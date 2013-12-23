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
#include "mfc.h"
#include "FrameExtractor.h"
#include "MPEG4Frames.h"
#include "H264Frames.h"
#include "VC1Frames.h"
#include "H263Frames.h"
#include "MfcDrvParams.h"
#include "line_buf_test.h"
#include "performance.h"


static unsigned char delimiter_h264[] = {0x00, 0x00, 0x00, 0x01 };
static unsigned char delimiter_mpeg4[] = {0x00, 0x00, 0x01 };
static unsigned char delimiter_h263[3]  = {0x00, 0x00, 0x80};


int Test_H263_Decoder_Line_Buffer(int argc, char **argv)
{
	int		in_fd, out_fd, mfc_fd;
	int		file_size;
	char	*addr, *in_addr, *pStrmBuf;
	int		nStrmSize;
	int		r;
	struct stat		s;
	FRAMEX_CTX		*pFrameExCtx;	// frame extractor context
	FRAMEX_STRM_PTR file_strm;
	
	// arguments of MFC ioctl
	MFC_DEC_INIT_ARG			dec_init;
	MFC_DEC_EXE_ARG				dec_exe;
	MFC_GET_BUF_ADDR_ARG		get_buf_addr;
	
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

	if (argc != 3) {
		printf("Usage : mfc <H.263 input filename> <output filename>\n");
		return -1;
	}

	///////////////////////////////////
	// Input/Output Stream File Open //
	///////////////////////////////////
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		printf("input/output file open error\n");
		return -1;
	}

	// get input file size
	fstat(in_fd, &s);
	file_size = s.st_size;


	// Input file should be mapped with memory. 
	// because file operations have a lot of performance down.
	// So, I Strongly recommend you to use mmap() of input file.
	in_addr = (char *)mmap(0, file_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if(in_addr == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}

	pStrmBuf = (char *)malloc(VIDEO_BUFFER_SIZE);

	pFrameExCtx = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_h263, sizeof(delimiter_h263), 1);   

	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	FrameExtractorFirst(pFrameExCtx, &file_strm);

	nStrmSize = ExtractConfigStreamH263(pFrameExCtx, &file_strm, (unsigned char *)pStrmBuf, VIDEO_BUFFER_SIZE, NULL);
	if (nStrmSize < 4) {
		printf("Cannot get the config stream for the H.263.\n");
		return -1;
	}

	////////////////////////////
	// MFC Device Driver Open //
	////////////////////////////
	mfc_fd = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (mfc_fd < 0) {
		printf("MFC open error : %d\n", mfc_fd);
		return -1;
	}


	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfc_fd, 0);
	if (addr == NULL) {
		printf("MFC mmap failed\n");
		return -1;
	}

	//////////////////////////////////
	//			(MFC ioctl)			//
	//	IOCTL_MFC_GET_LINE_BUF_ADDR	//
	//////////////////////////////////
	get_buf_addr.in_usr_data = (int)addr;
	r = ioctl(mfc_fd, IOCTL_MFC_GET_LINE_BUF_ADDR, &get_buf_addr);
	if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
		if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
			printf("The Handle of MFC Instance was invalidated!!!\n"); 
		}
		printf("IOCTL_MFC_GET_LINE_BUF_ADDR failure...\n");
		return -1;
	}
	
	// copy the config stream into the input buffer
	memcpy((char *)get_buf_addr.out_buf_addr, pStrmBuf, nStrmSize);	

	
	//////////////////////////////
	//			(MFC ioctl)		//
	//	IOCTL_MFC_H263_DEC_INIT	//
	//////////////////////////////
	dec_init.in_strmSize = nStrmSize;
	r = ioctl(mfc_fd, IOCTL_MFC_H263_DEC_INIT, &dec_init);
	if ( (r < 0) || (dec_init.ret_code < 0) ) {
		if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
			printf("The Handle of MFC Instance was invalidated!!!\n"); 
		}
		printf("IOCTL_MFC_H263_DEC_INIT failure...\n");
		return -1;
	}

	printf("WIDTH : %d, HEIGHT : %d\n", dec_init.out_width, dec_init.out_height);
	
	
	//////////////////
	//	Decoding	//
	//////////////////
	while(1) {
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////
		//		(MFC ioctl)			//
		//	IOCTL_MFC_H263_DEC_EXE	//
		//////////////////////////////
		dec_exe.in_strmSize = nStrmSize;
		r = ioctl(mfc_fd, IOCTL_MFC_H263_DEC_EXE, &dec_exe);
		if ( (r < 0) || (dec_exe.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_H263_DEC_EXE failure...\n");
			return -1;
		}
		
	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif
	
		nStrmSize = NextFrameH263(pFrameExCtx, &file_strm, (unsigned char *)pStrmBuf, VIDEO_BUFFER_SIZE, NULL);
		if (nStrmSize < 4) {
			break;
		}

		//////////////////////////////////
		//		(MFC ioctl)				//
		//	IOCTL_MFC_GET_LINE_BUF_ADDR	//
		//////////////////////////////////
		get_buf_addr.in_usr_data = (int)addr;
		r = ioctl(mfc_fd, IOCTL_MFC_GET_LINE_BUF_ADDR, &get_buf_addr);
		if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_GET_LINE_BUF_ADDR failure...\n");
			return -1;
		}
		
		memcpy((char *)get_buf_addr.out_buf_addr, pStrmBuf, nStrmSize);
		
		//////////////////////////////////
		//		(MFC ioctl)				//
		//	IOCTL_MFC_GET_FRAM_BUF_ADDR	//
		//////////////////////////////////
		get_buf_addr.in_usr_data = (int)addr;
		r = ioctl(mfc_fd, IOCTL_MFC_GET_FRAM_BUF_ADDR, &get_buf_addr);
		if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_GET_FRAM_BUF_ADDR failure...\n");
			return -1;
		}
	#ifndef FPS
		write(out_fd, (char *)get_buf_addr.out_buf_addr, get_buf_addr.out_buf_size);
	#endif
	}

#ifdef FPS
	printf("Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	munmap(in_addr, file_size);
	munmap(addr, BUF_SIZE);

	close(in_fd);
	close(out_fd);
	close(mfc_fd);
	free(pStrmBuf);
	
	return 0;
}

int Test_H264_Decoder_Line_Buffer(int argc, char **argv)
{
	int		in_fd, out_fd, mfc_fd;
	int		file_size;
	char	*addr, *in_addr, *pStrmBuf;
	int		nStrmSize;
	int		r;
	struct stat		s;
	FRAMEX_CTX		*pFrameExCtx;	// frame extractor context
	FRAMEX_STRM_PTR file_strm;
	
	// arguments of MFC ioctl
	MFC_DEC_INIT_ARG			dec_init;
	MFC_DEC_EXE_ARG				dec_exe;
	MFC_GET_BUF_ADDR_ARG		get_buf_addr;
	
#ifdef ROTATE_ENABLE
	MFC_SET_CONFIG_ARG			set_config;
#endif

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif


	if (argc != 3) {
		printf("Usage : mfc <H.264 input filename> <output filename>\n");
		return -1;
	}

	///////////////////////////////////
	// Input/Output Stream File Open //
	///////////////////////////////////
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		printf("input/output file open error\n");
		return -1;
	}

	// get input file size
	fstat(in_fd, &s);
	file_size = s.st_size;


	// Input file should be mapped with memory. 
	// because file operations have a lot of performance down.
	// So, I Strongly recommend you to use mmap() of input file.
	in_addr = (char *)mmap(0, file_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if(in_addr == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}

	pStrmBuf = (char *)malloc(VIDEO_BUFFER_SIZE);

	pFrameExCtx = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_h264, sizeof(delimiter_h264), 1);   

	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	FrameExtractorFirst(pFrameExCtx, &file_strm);

	nStrmSize = ExtractConfigStreamH264(pFrameExCtx, &file_strm, (unsigned char *)pStrmBuf, VIDEO_BUFFER_SIZE, NULL);
	if (nStrmSize < 4) {
		printf("Cannot get the config stream for the H.264.\n");
		return -1;
	}

	////////////////////////////
	// MFC Device Driver Open //
	////////////////////////////
	mfc_fd = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (mfc_fd < 0) {
		printf("MFC open error : %d\n", mfc_fd);
		return -1;
	}


	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfc_fd, 0);
	if (addr == NULL) {
		printf("MFC mmap failed\n");
		return -1;
	}

	//////////////////////////////////
	//			(MFC ioctl)			//
	//	IOCTL_MFC_GET_LINE_BUF_ADDR	//
	//////////////////////////////////
	get_buf_addr.in_usr_data = (int)addr;
	r = ioctl(mfc_fd, IOCTL_MFC_GET_LINE_BUF_ADDR, &get_buf_addr);
	if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
		if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
			printf("The Handle of MFC Instance was invalidated!!!\n"); 
		}
		printf("IOCTL_MFC_GET_LINE_BUF_ADDR failure...\n");
		return -1;
	}
	
	// copy the config stream into the input buffer
	memcpy((char *)get_buf_addr.out_buf_addr, pStrmBuf, nStrmSize);	
	
	//////////////////////////
	// Rotation mode		//
	//////////////////////////
#ifdef ROTATE_ENABLE
	set_config.in_config_param = MFC_SET_CONFIG_DEC_ROTATE;
	set_config.in_config_value[0] = 0x11;
	r = ioctl(mfc_fd, IOCTL_MFC_SET_CONFIG, &set_config);
	if ( (r < 0) || (set_config.ret_code < 0) ) {
		printf("Rotation setting was failed, r = %d, set_config.ret_code = %d\n", r, set_config.ret_code);
		return -1;
	}
#endif

	//////////////////////////////
	//			(MFC ioctl)		//
	//	IOCTL_MFC_H264_DEC_INIT	//
	//////////////////////////////
	dec_init.in_strmSize = nStrmSize;
	r = ioctl(mfc_fd, IOCTL_MFC_H264_DEC_INIT, &dec_init);
	if ( (r < 0) || (dec_init.ret_code < 0) ) {
		if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
			printf("The Handle of MFC Instance was invalidated!!!\n"); 
		}
		printf("IOCTL_MFC_H264_DEC_INIT failure...\n");
		return -1;
	}

	printf("WIDTH : %d, HEIGHT : %d\n", dec_init.out_width, dec_init.out_height);
	
	
	//////////////////
	//	Decoding	//
	//////////////////
	while(1) {
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////
		//		(MFC ioctl)			//
		//	IOCTL_MFC_H264_DEC_EXE	//
		//////////////////////////////
		dec_exe.in_strmSize = nStrmSize;
		r = ioctl(mfc_fd, IOCTL_MFC_H264_DEC_EXE, &dec_exe);
		if ( (r < 0) || (dec_exe.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_H264_DEC_EXE failure...\n");
			return -1;
		}
		
	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif
	
		nStrmSize = NextFrameH264(pFrameExCtx, &file_strm, (unsigned char *)pStrmBuf, VIDEO_BUFFER_SIZE, NULL);
		if (nStrmSize < 4) {
			break;
		}

		//////////////////////////////////
		//		(MFC ioctl)				//
		//	IOCTL_MFC_GET_LINE_BUF_ADDR	//
		//////////////////////////////////
		get_buf_addr.in_usr_data = (int)addr;
		r = ioctl(mfc_fd, IOCTL_MFC_GET_LINE_BUF_ADDR, &get_buf_addr);
		if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_GET_LINE_BUF_ADDR failure...\n");
			return -1;
		}
		
		memcpy((char *)get_buf_addr.out_buf_addr, pStrmBuf, nStrmSize);
		
		//////////////////////////////////
		//		(MFC ioctl				//
		//	IOCTL_MFC_GET_FRAM_BUF_ADDR	//
		//////////////////////////////////
		get_buf_addr.in_usr_data = (int)addr;
		r = ioctl(mfc_fd, IOCTL_MFC_GET_FRAM_BUF_ADDR, &get_buf_addr);
		if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_GET_FRAM_BUF_ADDR failure...\n");
			return -1;
		}
	#ifndef FPS
		write(out_fd, (char *)get_buf_addr.out_buf_addr, get_buf_addr.out_buf_size);
	#endif
	}

#ifdef FPS
	printf("Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	munmap(in_addr, file_size);
	munmap(addr, BUF_SIZE);
	
	close(in_fd);
	close(out_fd);
	close(mfc_fd);
	free(pStrmBuf);
	
	return 0;
}

int Test_MPEG4_Decoder_Line_Buffer(int argc, char **argv)
{
	int		in_fd, out_fd, mfc_fd;
	int		file_size;
	char	*addr, *in_addr, *pStrmBuf;
	int		nStrmSize;
	int		r;
	struct stat		s;
	FRAMEX_CTX		*pFrameExCtx;	// frame extractor context
	FRAMEX_STRM_PTR file_strm;
	
	// arguments of MFC ioctl
	MFC_DEC_INIT_ARG			dec_init;
	MFC_DEC_EXE_ARG				dec_exe;
	MFC_GET_BUF_ADDR_ARG		get_buf_addr;

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif


	if (argc != 4) {
		printf("Usage : mfc <MPEG4 input filename> <output filename> <rotation value>\n");
		return -1;
	}

	///////////////////////////////////
	// Input/Output Stream File Open //
	///////////////////////////////////
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		printf("input/output file open error\n");
		return -1;
	}

	// get input file size
	fstat(in_fd, &s);
	file_size = s.st_size;


	// Input file should be mapped with memory. 
	// because file operations have a lot of performance down.
	// So, I Strongly recommend you to use mmap() of input file.
	in_addr = (char *)mmap(0, file_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if(in_addr == NULL) {
		printf("input file memory mapping failed\n");
		return -1;
	}

	pStrmBuf = (char *)malloc(VIDEO_BUFFER_SIZE);

	pFrameExCtx = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_mpeg4, sizeof(delimiter_mpeg4), 1);   

	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	FrameExtractorFirst(pFrameExCtx, &file_strm);

	nStrmSize = ExtractConfigStreamMpeg4(pFrameExCtx, &file_strm, (unsigned char *)pStrmBuf, VIDEO_BUFFER_SIZE, NULL);
	if (nStrmSize < 4) {
		printf("Cannot get the config stream for the MPEG4.\n");
		return -1;
	}

	////////////////////////////
	// MFC Device Driver Open //
	////////////////////////////
	mfc_fd = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (mfc_fd < 0) {
		printf("MFC open error : %d\n", mfc_fd);
		return -1;
	}


	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfc_fd, 0);
	if (addr == NULL) {
		printf("MFC mmap failed\n");
		return -1;
	}

	//////////////////////////////////
	//			(MFC ioctl)			//
	//	IOCTL_MFC_GET_LINE_BUF_ADDR	//
	//////////////////////////////////
	get_buf_addr.in_usr_data = (int)addr;
	r = ioctl(mfc_fd, IOCTL_MFC_GET_LINE_BUF_ADDR, &get_buf_addr);
	if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
		if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
			printf("The Handle of MFC Instance was invalidated!!!\n"); 
		}
		printf("IOCTL_MFC_GET_LINE_BUF_ADDR failure...\n");
		return -1;
	}
	
	// copy the config stream into the input buffer
	memcpy((char *)get_buf_addr.out_buf_addr, pStrmBuf, nStrmSize);	
	
	
	//////////////////////////////
	//			(MFC ioctl)		//
	//	IOCTL_MFC_MPEG4_DEC_INIT	//
	//////////////////////////////
	dec_init.in_strmSize = nStrmSize;
	r = ioctl(mfc_fd, IOCTL_MFC_MPEG4_DEC_INIT, &dec_init);
	if ( (r < 0) || (dec_init.ret_code < 0) ) {
		if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
			printf("The Handle of MFC Instance was invalidated!!!\n"); 
		}
		printf("IOCTL_MFC_MPEG4_DEC_INIT failure...\n");
		return -1;
	}

	printf("WIDTH : %d, HEIGHT : %d\n", dec_init.out_width, dec_init.out_height);
	
	
	//////////////////
	//	Decoding	//
	//////////////////
	while(1) {
		#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////
		//		(MFC ioctl)			//
		//	IOCTL_MFC_MPEG4_DEC_EXE	//
		//////////////////////////////
		dec_exe.in_strmSize = nStrmSize;
		r = ioctl(mfc_fd, IOCTL_MFC_MPEG4_DEC_EXE, &dec_exe);
		if ( (r < 0) || (dec_exe.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_MPEG4_DEC_EXE failure...\n");
			return -1;
		}
		
	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif

		nStrmSize = NextFrameMpeg4(pFrameExCtx, &file_strm, (unsigned char *)pStrmBuf, VIDEO_BUFFER_SIZE, NULL);
		if (nStrmSize < 4) {
			break;
		}

		//////////////////////////////////
		//		(MFC ioctl)				//
		//	IOCTL_MFC_GET_LINE_BUF_ADDR	//
		//////////////////////////////////
		get_buf_addr.in_usr_data = (int)addr;
		r = ioctl(mfc_fd, IOCTL_MFC_GET_LINE_BUF_ADDR, &get_buf_addr);
		if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_GET_LINE_BUF_ADDR failure...\n");
			return -1;
		}
		
		memcpy((char *)get_buf_addr.out_buf_addr, pStrmBuf, nStrmSize);
		
		//////////////////////////////////
		//		(MFC ioctl				//
		//	IOCTL_MFC_GET_FRAM_BUF_ADDR	//
		//////////////////////////////////
		get_buf_addr.in_usr_data = (int)addr;
		r = ioctl(mfc_fd, IOCTL_MFC_GET_FRAM_BUF_ADDR, &get_buf_addr);
		if ( (r < 0) || (get_buf_addr.ret_code < 0) ) {
			if (get_buf_addr.ret_code == MFCDRV_RET_ERR_HANDLE_INVALIDATED) {
				printf("The Handle of MFC Instance was invalidated!!!\n"); 
			}
			printf("IOCTL_MFC_GET_FRAM_BUF_ADDR failure...\n");
			return -1;
		}
	#ifndef FPS
		write(out_fd, (char *)get_buf_addr.out_buf_addr, get_buf_addr.out_buf_size);
	#endif
	}

#ifdef FPS
	printf("Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	munmap(in_addr, file_size);
	munmap(addr, BUF_SIZE);

	close(in_fd);
	close(out_fd);
	close(mfc_fd);
	free(pStrmBuf);
	
	return 0;
}

