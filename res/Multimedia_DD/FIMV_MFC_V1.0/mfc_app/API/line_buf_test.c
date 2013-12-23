#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>

#include "SsbSipH264Decode.h"
#include "SsbSipMpeg4Decode.h"
#include "SsbSipVC1Decode.h"
#include "FrameExtractor.h"
#include "MPEG4Frames.h"
#include "H263Frames.h"
#include "H264Frames.h"
#include "SsbSipLogMsg.h"
#include "performance.h"
#include "FileRead.h"


static unsigned char delimiter_mpeg4[3] = {0x00, 0x00, 0x01};
//static unsigned char delimiter_h263[3]  = {0x00, 0x00, 0x80};
static unsigned char delimiter_h264[4]  = {0x00, 0x00, 0x00, 0x01};

#define INPUT_BUFFER_SIZE		(1024 * 256)


int Test_H264_Decoder_Line_Buffer(int argc, char **argv)
{
	void    		*handle;
	void			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned char	*pYUVBuf;
	long			nYUVLeng;
	int				in_fd, out_fd;
	int				file_size;
	char			*in_addr;
	int				i=0;
	char			*start_addr, *write_addr;
	
	struct stat				s;
	FRAMEX_CTX				*pFrameExCtx;	// frame extractor context
	FRAMEX_STRM_PTR 		file_strm;
	SSBSIP_H264_STREAM_INFO stream_info;	

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

#ifdef ROTATE_ENABLE
	int				rotation_value;
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
		LOG_MSG(LOG_ERROR, "Test_H264_Decoder_Line_Buffer", "Input/Output file open failed\n");
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
		LOG_MSG(LOG_ERROR, "Test_H264_Decoder_Line_Buffer", "Mmap of Input file was failed\n");
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
		LOG_MSG(LOG_ERROR, "Test_H264_Decoder_Line_Buffer", "H264_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipH264DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipH264DecodeGetInBuf(handle, nFrameLeng);
	if (pStrmBuf == NULL) {
		LOG_MSG(LOG_ERROR, "Test_H264_Decoder_Line_Buffer", "SsbSipH264DecodeGetInBuf Failed.\n");
		SsbSipH264DecodeDeInit(handle);
		return -1;
	}

	////////////////////////////////////
	//  H264 CONFIG stream extraction //
	////////////////////////////////////
	nFrameLeng = ExtractConfigStreamH264(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);

	////////////////////////////////////
	//	Rotation Test				  //
	////////////////////////////////////
#ifdef ROTATE_ENABLE
	rotation_value = 0x11;
	SsbSipH264DecodeSetConfig(handle, H264_DEC_SETCONF_POST_ROTATE, &rotation_value);
#endif

	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipH264DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK) {
		LOG_MSG(LOG_ERROR, "Test_H264_Decoder_Line_Buffer", "H.264 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_STREAMINFO, &stream_info);

	LOG_MSG(LOG_TRACE, "Test_H264_Decoder_Line_Buffer", "\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);


	while(1) {
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipH264DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK)
			break;

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif

		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipH264DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		pYUVBuf = SsbSipH264DecodeGetOutBuf(handle, &nYUVLeng);

	#ifndef FPS
		//printf("pYUVBuf = 0x%X\n", pYUVBuf);
		
		write(out_fd, pYUVBuf, (stream_info.width * stream_info.height * 3) >> 1);

		
	#endif

			
		/////////////////////////////
		// Next H.264 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameH264(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
		if (nFrameLeng < 4)
			break;
	}

#ifdef FPS
	LOG_MSG(LOG_TRACE, "Test_H264_Decoder_Line_Buffer", 	\
		"Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	///////////////////////////////////////
	///    7. SsbSipH264DecodeDeInit    ///
	///////////////////////////////////////
	SsbSipH264DecodeDeInit(handle);
	munmap(in_addr, file_size);
	
	LOG_MSG(LOG_TRACE, "Test_H264_Decoder_Line_Buffer", "\n\n@@@ Program ends.\n");

	close(in_fd);
	close(out_fd);

	return 0;
}


int Test_MPEG4_Decoder_Line_Buffer(int argc, char **argv)
{
	void    		*handle;
	void			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned char	*pYUVBuf;
	long			nYUVLeng;
	int				in_fd, out_fd;
	int				file_size;
	char			*in_addr;
	
	struct stat				s;
	FRAMEX_CTX				*pFrameExCtx;	// frame extractor context
	FRAMEX_STRM_PTR 		file_strm;
	SSBSIP_MPEG4_STREAM_INFO stream_info;	

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

#ifdef ROTATE_ENABLE
	int 			rotation_value;
#endif

	if (argc != 3) {
		printf("Usage : mfc <MPEG4 input filename> <output filename>\n");
		return -1;
	}

	///////////////////////////////////
	// Input/Output Stream File Open //
	///////////////////////////////////
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "Input/Output file open failed\n");
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
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "Mmap of Input file was failed\n");
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
	///      (SsbSipMPEG4DecodeInit)   ///
	//////////////////////////////////////
	handle = SsbSipMPEG4DecodeInit();
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "MPEG4_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipMPEG4DecodeGetInBuf(handle, nFrameLeng);
	if (pStrmBuf == NULL) {
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "SsbSipMPEG4DecodeGetInBuf Failed.\n");
		SsbSipMPEG4DecodeDeInit(handle);
		return -1;
	}

	////////////////////////////////////
	// MPEG4 CONFIG stream extraction //
	////////////////////////////////////
	nFrameLeng = ExtractConfigStreamMpeg4(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);

	////////////////////////////////////
	//	Rotation Test				  //
	////////////////////////////////////
#ifdef ROTATE_ENABLE
	rotation_value = 0x11;
	SsbSipMPEG4DecodeSetConfig(handle, MPEG4_DEC_SETCONF_POST_ROTATE, &rotation_value);
#endif

	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK) {
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "MPEG4 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipMPEG4DecodeGetConfig(handle, MPEG4_DEC_GETCONF_STREAMINFO, &stream_info);

	LOG_MSG(LOG_TRACE, "Test_MPEG4_Decoder_Line_Buffer", "\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);


	while(1) {	
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipMPEG4DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK)
			break;

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif

		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipMPEG4DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		pYUVBuf = SsbSipMPEG4DecodeGetOutBuf(handle, &nYUVLeng);

	#ifndef FPS
		write(out_fd, pYUVBuf, (stream_info.width * stream_info.height * 3) >> 1);
	#endif

		/////////////////////////////
		// Next H.264 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameMpeg4(pFrameExCtx, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
		if (nFrameLeng < 4)
			break;
	}

#ifdef FPS
	LOG_MSG(LOG_TRACE, "Test_MPEG4_Decoder_Line_Buffer", 	\
		"Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	///////////////////////////////////////
	///    7. SsbSipMPEG4DecodeDeInit    ///
	///////////////////////////////////////
	SsbSipMPEG4DecodeDeInit(handle);
	munmap(in_addr, file_size);

	LOG_MSG(LOG_TRACE, "Test_MPEG4_Decoder_Line_Buffer", "\n\n@@@ Program ends.\n");

	close(in_fd);
	close(out_fd);

	return 0;
}


int Test_H263_Decoder_Line_Buffer(int argc, char **argv)
{
	void    		*handle;
	void			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned char	*pYUVBuf;
	long			nYUVLeng;
	int				in_fd, out_fd;
	int				file_size;
	char			*in_addr;

	struct stat				s;
	MMAP_STRM_PTR 			file_strm;
	SSBSIP_MPEG4_STREAM_INFO stream_info;	

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

#ifdef ROTATE_ENABLE
	int				rotation_value;
#endif

	if (argc != 3) {
		printf("Usage : mfc <H263 input filename> <output filename>\n");
		return -1;
	}

	///////////////////////////////////
	// Input/Output Stream File Open //
	///////////////////////////////////
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "Input/Output file open failed\n");
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
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "Mmap of Input file was failed\n");
		return -1;
	}


	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipMPEG4DecodeInit)   ///
	//////////////////////////////////////
	handle = SsbSipMPEG4DecodeInit();
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_H263_Decoder_Line_Buffer", "MPEG4_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipMPEG4DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipMPEG4DecodeGetInBuf(handle, 200000);
	if (pStrmBuf == NULL) {
		LOG_MSG(LOG_ERROR, "Test_H263_Decoder_Line_Buffer", "SsbSipMPEG4DecodeGetInBuf Failed.\n");
		SsbSipMPEG4DecodeDeInit(handle);
		return -1;
	}

	////////////////////////////////////
	// H263 CONFIG stream extraction  //
	////////////////////////////////////
	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	nFrameLeng = ExtractConfigStreamH263(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);

	////////////////////////////////////
	//	Rotation Test				  //
	////////////////////////////////////
#ifdef ROTATE_ENABLE
	rotation_value = 0x11;
	SsbSipMPEG4DecodeSetConfig(handle, MPEG4_DEC_SETCONF_POST_ROTATE, &rotation_value);
#endif

	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMPEG4DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK) {
		LOG_MSG(LOG_ERROR, "Test_H263_Decoder_Line_Buffer", "MPEG4 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipMPEG4DecodeGetConfig(handle, MPEG4_DEC_GETCONF_STREAMINFO, &stream_info);

	LOG_MSG(LOG_TRACE, "Test_H263_Decoder_Line_Buffer", "\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);


	while(1) {
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipMPEG4DecodeExe)   ///
		//////////////////////////////////
		if (SsbSipMPEG4DecodeExe(handle, nFrameLeng) != SSBSIP_MPEG4_DEC_RET_OK)
			break;

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif

		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipMPEG4DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		pYUVBuf = SsbSipMPEG4DecodeGetOutBuf(handle, &nYUVLeng);

	#ifndef FPS
		write(out_fd, pYUVBuf, (stream_info.width * stream_info.height * 3) >> 1);
	#endif
	
		/////////////////////////////
		// Next H.264 VIDEO stream //
		/////////////////////////////
		nFrameLeng = NextFrameH263(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
		if (nFrameLeng < 4)
			break;
	}

#ifdef FPS
	LOG_MSG(LOG_TRACE, "Test_H263_Decoder_Line_Buffer", 	\
		"Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	///////////////////////////////////////
	///    7. SsbSipMPEG4DecodeDeInit    ///
	///////////////////////////////////////
	SsbSipMPEG4DecodeDeInit(handle);
	munmap(in_addr, file_size);
	

	LOG_MSG(LOG_TRACE, "Test_H263_Decoder_Line_Buffer", "\n\n@@@ Program ends.\n");

	close(in_fd);
	close(out_fd);

	return 0;
}


int Test_VC1_Decoder_Line_Buffer(int argc, char **argv)
{
	void    		*handle;
	void			*pStrmBuf;
	int				nFrameLeng = 0;
	unsigned char	*pYUVBuf;
	long			nYUVLeng;
	int				in_fd, out_fd;
	int				file_size;
	char			*in_addr;

	struct stat				s;
	MMAP_STRM_PTR 			file_strm;
	SSBSIP_MPEG4_STREAM_INFO stream_info;	

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

#ifdef ROTATE_ENABLE
	int				rotation_value;
#endif

	if (argc != 3) {
		printf("Usage : mfc <VC-1 input filename> <output filename>\n");
		return -1;
	}

	///////////////////////////////////
	// Input/Output Stream File Open //
	///////////////////////////////////
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "Input/Output file open failed\n");
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
		LOG_MSG(LOG_ERROR, "Test_MPEG4_Decoder_Line_Buffer", "Mmap of Input file was failed\n");
		return -1;
	}


	//////////////////////////////////////
	///    1. Create new instance      ///
	///      (SsbSipVC1DecodeInit)     ///
	//////////////////////////////////////
	handle = SsbSipVC1DecodeInit();
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_VC1_Decoder_Line_Buffer", "VC1_Dec_Init Failed.\n");
		return -1;
	}

	/////////////////////////////////////////////
	///    2. Obtaining the Input Buffer      ///
	///      (SsbSipVC1DecodeGetInBuf)       ///
	/////////////////////////////////////////////
	pStrmBuf = SsbSipVC1DecodeGetInBuf(handle, 200000);
	if (pStrmBuf == NULL) {
		LOG_MSG(LOG_ERROR, "Test_VC1_Decoder_Line_Buffer", "SsbSipVC1DecodeGetInBuf Failed.\n");
		SsbSipVC1DecodeDeInit(handle);
		return -1;
	}

	////////////////////////////////////
	// H263 CONFIG stream extraction  //
	////////////////////////////////////
	file_strm.p_start = file_strm.p_cur = (unsigned char *)in_addr;
	file_strm.p_end = (unsigned char *)(in_addr + file_size);
	nFrameLeng = ExtractConfigStreamVC1(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);

	////////////////////////////////////
	//	Rotation Test				  //
	////////////////////////////////////
#ifdef ROTATE_ENABLE
	rotation_value = 0x11;
	SsbSipVC1DecodeSetConfig(handle, VC1_DEC_SETCONF_POST_ROTATE, &rotation_value);
#endif

	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipVC1DecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipVC1DecodeExe(handle, nFrameLeng) != SSBSIP_VC1_DEC_RET_OK) {
		LOG_MSG(LOG_ERROR, "Test_VC1_Decoder_Line_Buffer", "VC1 Decoder Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipVC1DecodeGetConfig(handle, VC1_DEC_GETCONF_STREAMINFO, &stream_info);

	LOG_MSG(LOG_TRACE, "Test_VC1_Decoder_Line_Buffer", "\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);


	while(1) {
	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		//////////////////////////////////
		///       5. DECODE            ///
		///    (SsbSipVC1DecodeExe)    ///
		//////////////////////////////////
		if (SsbSipVC1DecodeExe(handle, nFrameLeng) != SSBSIP_VC1_DEC_RET_OK)
			break;

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif

		//////////////////////////////////////////////
		///    6. Obtaining the Output Buffer      ///
		///      (SsbSipVC1DecodeGetOutBuf)       ///
		//////////////////////////////////////////////
		pYUVBuf = SsbSipVC1DecodeGetOutBuf(handle, &nYUVLeng);

	#ifndef FPS
		write(out_fd, pYUVBuf, (stream_info.width * stream_info.height * 3) >> 1);
	#endif
	
		/////////////////////////////
		// Next VC-1 VIDEO stream  //
		/////////////////////////////
		nFrameLeng = NextFrameVC1(&file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
		if (nFrameLeng < 4)
			break;
	}

#ifdef FPS
	LOG_MSG(LOG_TRACE, "Test_H263_Decoder_Line_Buffer", 	\
		"Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	///////////////////////////////////////
	///    7. SsbSipMPEG4DecodeDeInit    ///
	///////////////////////////////////////
	SsbSipVC1DecodeDeInit(handle);
	munmap(in_addr, file_size);
	

	LOG_MSG(LOG_TRACE, "Test_VC1_Decoder_Line_Buffer", "\n\n@@@ Program ends.\n");

	close(in_fd);
	close(out_fd);

	return 0;
}

