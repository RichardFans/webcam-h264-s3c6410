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

#include "SsbSipH264Encode.h"
#include "SsbSipMpeg4Encode.h"
#include "SsbSipLogMsg.h"
#include "performance.h"


int Test_H264_Encoder(int argc, char **argv)
{
	int				in_fd, out_fd;
	char			*in_addr;
	int				file_size;
	int				frame_count, backup_frame_cnt;
	int				frame_size;
	void			*handle;
	int				width, height, frame_rate, bitrate, gop_num;
	unsigned char	*p_inbuf;
	unsigned char	*p_outbuf;
	long			size;
	int				ret;
	int				hdr_size;
	int				slices[2];
	int				para_change[2];
	int				frame_cnt = 0;
	struct stat		s;	
	
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
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

	width 		= atoi(argv[3]);
	height		= atoi(argv[4]);
	frame_rate	= atoi(argv[5]);
	bitrate		= atoi(argv[6]);
	gop_num		= atoi(argv[7]);

	frame_size	= (width * height * 3) >> 1;
	frame_count	= file_size / frame_size;

	printf("file_size : %d, frame_size : %d, frame count : %d\n", file_size, frame_size, frame_count);
	
	
	handle = SsbSipH264EncodeInit(width, height, frame_rate, bitrate, gop_num);
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_Encoder", "SsbSipH264EncodeInit Failed\n");
		return -1;
	}


	// Setting multiple slices
	// This setting must be called before SsbSipH264EncodeExe()
	slices[0] = 0;	// 0 for single, 1 for multiple
	slices[1] = 4;	// count of slices
	SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_NUM_SLICES, slices);
	

	ret = SsbSipH264EncodeExe(handle);

	p_inbuf = SsbSipH264EncodeGetInBuf(handle, 0);

	backup_frame_cnt = frame_count;
	
	while(frame_count > 0) 
	{
		printf("frame count : %d\n", frame_count);
		
		// copy YUV data into input buffer
		memcpy(p_inbuf, in_addr, frame_size);
		in_addr += frame_size;
		
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif

		ret = SsbSipH264EncodeExe(handle);
		if (frame_count == backup_frame_cnt) {
			SsbSipH264EncodeGetConfig(handle, H264_ENC_GETCONF_HEADER_SIZE, &hdr_size);
			printf("Header Size : %d\n", hdr_size);
		}

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
	#endif

		frame_cnt++;

		// change GOP number while encoding
		/* 
		if (frame_cnt == 35) {
			para_change[0] = H264_ENC_PARAM_GOP_NUM;
			para_change[1] = 10;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change bitrate while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = H264_ENC_PARAM_BITRATE;
			para_change[1] = 500;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change frame rate while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = H264_ENC_PARAM_F_RATE;
			para_change[1] = 5;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change QP while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = H264_ENC_PARAM_INTRA_QP;
			para_change[1] = 25;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change Intra MB refresh number while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = H264_ENC_PARAM_INTRA_REF;
			para_change[1] = 5;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change multiple or single slice mode while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = H264_ENC_PARAM_SLICE_MODE;
			para_change[1] = 4;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// make I-frame when user needs
		/*
		if (frame_cnt == 10) {
			para_change[0] = H264_ENC_PIC_OPT_IDR;
			para_change[1] = 1;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_CUR_PIC_OPT, para_change);
		}
		*/

		// skip frame when user wants
		/*
		if (frame_cnt == 10) {
			para_change[0] = H264_ENC_PIC_OPT_SKIP;
			para_change[1] = 1;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_CUR_PIC_OPT, para_change);
		}
		*/

		// The SEI message which is containing the recovery point is generated.
		/*
		if (frame_cnt == 20) {
			para_change[0] = H264_ENC_PIC_OPT_RECOVERY;
			para_change[1] = 7;
			SsbSipH264EncodeSetConfig(handle, H264_ENC_SETCONF_CUR_PIC_OPT, para_change);
		}
		*/
		
		
		p_outbuf = SsbSipH264EncodeGetOutBuf(handle, &size);
	
	#ifndef FPS
		write(out_fd, p_outbuf, size);
	#endif
	
		frame_count--;
	}

#ifdef FPS
	printf("Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif
	SsbSipH264EncodeDeInit(handle);
	munmap(in_addr, file_size);
	
	close(in_fd);
	close(out_fd);
		
	return 0;
}

int Test_MPEG4_Encoder(int argc, char **argv)
{
	int				in_fd, out_fd;
	char			*in_addr;
	int				file_size;
	int				frame_count, backup_frame_count;
	int				frame_size;
	void			*handle;
	int				width, height, frame_rate, bitrate, gop_num;
	unsigned char	*p_inbuf;
	unsigned char	*p_outbuf;
	long			size;
	int				ret;
	int				hdr_size;
	int				frame_cnt = 0;
	int				para_change[2];
	struct stat		s;	
	
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
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

	width 		= atoi(argv[3]);
	height		= atoi(argv[4]);
	frame_rate	= atoi(argv[5]);
	bitrate		= atoi(argv[6]);
	gop_num		= atoi(argv[7]);

	frame_size	= (width * height * 3) >> 1;
	frame_count	= file_size / frame_size;

	printf("file_size : %d, frame_size : %d, frame count : %d\n", file_size, frame_size, frame_count);

	handle = SsbSipMPEG4EncodeInit(SSBSIPMFCENC_MPEG4, width, height, frame_rate, bitrate, gop_num);
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_Encoder", "SsbSipMPEG4EncodeInit Failed\n");
		return -1;
	}

	ret = SsbSipMPEG4EncodeExe(handle);
	
	p_inbuf = SsbSipMPEG4EncodeGetInBuf(handle, 0);

	backup_frame_count = frame_count;
	
	while(frame_count > 0) 
	{
		printf("frame count : %d\n", frame_count);
		// copy YUV data into input buffer
		memcpy(p_inbuf, in_addr, frame_size);
		in_addr += frame_size;
		
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif

		ret = SsbSipMPEG4EncodeExe(handle);
		if (backup_frame_count == frame_count) {
			SsbSipMPEG4EncodeGetConfig(handle, MPEG4_ENC_GETCONF_HEADER_SIZE, &hdr_size);
			printf("Header Size = %d\n", hdr_size);
		}
		
	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
	#endif

		frame_cnt++;
		

		// change GOP number while encoding
		/* 
		if (frame_cnt == 50) {
			para_change[0] = MPEG4_ENC_PARAM_GOP_NUM;
			para_change[1] = 10;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change bitrate while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_BITRATE;
			para_change[1] = 200;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change frame rate while encoding
		/*
		if (frame_cnt == 30) {
			para_change[0] = MPEG4_ENC_PARAM_F_RATE;
			para_change[1] = 10;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change QP while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_INTRA_QP;
			para_change[1] = 50;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change Intra MB refresh number while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_INTRA_REF;
			para_change[1] = 5;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change multiple or single slice mode while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_SLICE_MODE;
			para_change[1] = 4;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// make I-frame when user needs
		/*
		if (frame_cnt == 10) {
			para_change[0] = MPEG4_ENC_PIC_OPT_IDR;
			para_change[1] = 1;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_CUR_PIC_OPT, para_change);
		}
		*/

		// skip frame when user wants
		/*
		if (frame_cnt == 15) {
			para_change[0] = MPEG4_ENC_PIC_OPT_SKIP;
			para_change[1] = 1;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_CUR_PIC_OPT, para_change);
		}
		*/
	
		p_outbuf = SsbSipMPEG4EncodeGetOutBuf(handle, &size);
	
	#ifndef FPS
		write(out_fd, p_outbuf, size);
	#endif
	
		frame_count--;
	}

#ifdef FPS
	printf("Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif
	SsbSipMPEG4EncodeDeInit(handle);
	munmap(in_addr, file_size);
	
	close(in_fd);
	close(out_fd);
		
	return 0;
}

int Test_H263_Encoder(int argc, char **argv)
{
	int				in_fd, out_fd;
	char			*in_addr;
	int				file_size;
	int				frame_count;
	int				frame_size;
	void			*handle;
	int				width, height, frame_rate, bitrate, gop_num;
	unsigned char	*p_inbuf;
	unsigned char	*p_outbuf;
	long			size;
	int				ret;
	int				slices[2];
	int				para_change[2];
	int				frame_cnt = 0;
	struct stat		s;	
	
#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
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

	width 		= atoi(argv[3]);
	height		= atoi(argv[4]);
	frame_rate	= atoi(argv[5]);
	bitrate		= atoi(argv[6]);
	gop_num		= atoi(argv[7]);

	frame_size	= (width * height * 3) >> 1;
	frame_count	= file_size / frame_size;

	printf("file_size : %d, frame_size : %d, frame count : %d\n", file_size, frame_size, frame_count);
	

	handle = SsbSipMPEG4EncodeInit(SSBSIPMFCENC_H263, width, height, frame_rate, bitrate, gop_num);
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_Encoder", "SsbSipMPEG4EncodeInit Failed\n");
		return -1;
	}
	
	// Below ioctl function supports multiple slice mode in H.263 encoding case
	//SsbSipMPEG4EncodeSetConfig(handle, SET_H263_MULTIPLE_SLICE, NULL);


	// Setting multiple slices
	// This setting must be called before SsbSipH264EncodeExe()
	/*
	slices[0] = 0;	// 0 for single, 1 for multiple
	slices[1] = 4;	// count of slices
	SsbSipH264EncodeSetConfig(handle, MPEG4_ENC_SETCONF_H263_NUM_SLICES, slices);
	*/
	
	// Setting ANNEX
	/*
	slices[0] = ANNEX_K_ON | ANNEX_T_ON;
	SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_H263_ANNEX, slices);
	*/
	
	ret = SsbSipMPEG4EncodeExe(handle);

	p_inbuf = SsbSipMPEG4EncodeGetInBuf(handle, 0);

	
	while(frame_count > 0) 
	{
		printf("frame count : %d\n", frame_count);
		// copy YUV data into input buffer
		memcpy(p_inbuf, in_addr, frame_size);
		in_addr += frame_size;
		
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif

		ret = SsbSipMPEG4EncodeExe(handle);
		
	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
	#endif

		frame_cnt++;
	
		// change GOP number while encoding
		/* 
		if (frame_cnt == 50) {
			para_change[0] = MPEG4_ENC_PARAM_GOP_NUM;
			para_change[1] = 10;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change bitrate while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_BITRATE;
			para_change[1] = 200;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change frame rate while encoding
		/*
		if (frame_cnt == 30) {
			para_change[0] = MPEG4_ENC_PARAM_F_RATE;
			para_change[1] = 5;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change QP while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_INTRA_QP;
			para_change[1] = 5;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change Intra MB refresh number while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_INTRA_REF;
			para_change[1] = 5;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// change multiple or single slice mode while encoding
		/*
		if (frame_cnt == 25) {
			para_change[0] = MPEG4_ENC_PARAM_SLICE_MODE;
			para_change[1] = 4;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_PARAM_CHANGE, para_change);
		}
		*/

		// make I-frame when user needs
		/*
		if (frame_cnt == 10) {
			para_change[0] = MPEG4_ENC_PIC_OPT_IDR;
			para_change[1] = 1;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_CUR_PIC_OPT, para_change);
		}
		*/

		// skip frame when user wants
		/*
		if (frame_cnt == 15) {
			para_change[0] = MPEG4_ENC_PIC_OPT_SKIP;
			para_change[1] = 1;
			SsbSipMPEG4EncodeSetConfig(handle, MPEG4_ENC_SETCONF_CUR_PIC_OPT, para_change);
		}
		*/
	
		p_outbuf = SsbSipMPEG4EncodeGetOutBuf(handle, &size);
	
	#ifndef FPS
		write(out_fd, p_outbuf, size);
	#endif
	
		frame_count--;
	}

#ifdef FPS
	printf("Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	SsbSipMPEG4EncodeDeInit(handle);
	munmap(in_addr, file_size);
	
	close(in_fd);
	close(out_fd);
		
	return 0;
}


