#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

#include "SsbSipMfcDecode.h"
#include "SsbSipLogMsg.h"
#include "performance.h"


int Test_Decoder_Ring_Buffer(int argc, char **argv)
{
	void	*handle;
	void	*pStrmBuf;
	long	nStrmSize;
	int		nReadLeng;
	long	nYUVLeng;
	int		in_fd, out_fd;
	char	*in_addr;
	int		cnt = 1;
	int		file_size;
	int		remain;
	struct stat		s;
	unsigned char	*pYUVBuf;
	SSBSIP_MFC_STREAM_INFO  stream_info;

#ifdef FPS
	struct timeval	start, stop;
	unsigned int	time = 0;
	int				frame_cnt = 0;
#endif

#ifdef ROTATE_ENABLE
	int				rotation_value;
#endif

	if (argc != 3) {
		printf("Usage : mfc <input file name> <output filename>\n");
		return -1;
	}
	
	///////////////////////////////////
	// Input/Output Stream File Open //
	///////////////////////////////////
	in_fd	= open(argv[1], O_RDONLY);
	out_fd	= open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if( (in_fd < 0) || (out_fd < 0) ) {
		LOG_MSG(LOG_ERROR, "Test_Decoder_Ring_Buffer", "Input/Output file open failed\n");
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
		LOG_MSG(LOG_ERROR, "Test_Decoder_Ring_Buffer", "Mmap of Input file was failed\n");
		return -1;
	}

	LOG_MSG(LOG_TRACE, "Test_Decoder_Ring_Buffer", "###  Start Decoding\n");

	/////////////////////////////////////
	///    1. Create new instance     ///
	///      (SsbSipMfcDecodeInit)    ///
	/////////////////////////////////////
	handle = SsbSipMfcDecodeInit(SSBSIPMFCDEC_H264);
	if (handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_Decoder_Ring_Buffer", "SsbSipMfcDecodeInit Failed.\n");
		return -1;

	}

	////////////////////////////////////////////
	///    2. Obtaining the Input Buffer     ///
	///      (SsbSipMfcDecodeGetInBuf)       ///
	////////////////////////////////////////////
	pStrmBuf = SsbSipMfcDecodeGetInBuf(handle, &nStrmSize);
	if (pStrmBuf == NULL) {
		LOG_MSG(LOG_ERROR, "Test_Decoder_Ring_Buffer", "SsbSipMfcDecodeGetInBuf Failed.\n");
		SsbSipMfcDecodeDeInit(handle);
		return -1;
	}
	LOG_MSG(LOG_ERROR, "Test_Decoder_Ring_Buffer", "XXXX  pStrmBuf =0x%X.\n", pStrmBuf);


	/////////////////////////////////////////////
	//   Read Video Stream from File           //
	//   (Size of reading is returned          //
	//    by SsbSipMfcDecodeGetInBuf() call)   //
	/////////////////////////////////////////////
	remain = file_size;
	memcpy(pStrmBuf, in_addr, nStrmSize);
	remain -= nStrmSize;
	LOG_MSG(LOG_TRACE, "Test_Decoder_Ring_Buffer", "remain = %d\n", remain);

	////////////////////////////////////
	//	Rotation Test				  //
	////////////////////////////////////
#ifdef ROTATE_ENABLE
	rotation_value = 0x11;
	SsbSipMfcDecodeSetConfig(handle, MFC_DEC_SETCONF_POST_ROTATE, &rotation_value);
#endif

	////////////////////////////////////////////////////////////////
	///    3. Configuring the instance with the config stream    ///
	///       (SsbSipMfcDecodeExe)                             ///
	////////////////////////////////////////////////////////////////
	if (SsbSipMfcDecodeExe(handle, nStrmSize) != SSBSIP_MFC_DEC_RET_OK) {
		LOG_MSG(LOG_TRACE, "Test_Decoder_Ring_Buffer", "MFC Decoder(RING_BUF mode) Configuration Failed.\n");
		return -1;
	}


	/////////////////////////////////////
	///   4. Get stream information   ///
	/////////////////////////////////////
	SsbSipMfcDecodeGetConfig(handle, MFC_DEC_GETCONF_STREAMINFO, &stream_info);

	LOG_MSG(LOG_TRACE, "Test_Decoder_Ring_Buffer", 	\
		"\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);

	while(1) {

		////////////////////////////////////////////
		///    5. Obtaining the Input Buffer     ///
		///      (SsbSipMfcDecodeGetInBuf)       ///
		////////////////////////////////////////////
		pStrmBuf = SsbSipMfcDecodeGetInBuf(handle, &nStrmSize);


		//////////////////////////////////////////////////
		///   Fill the Input Buffer only if required.  ///
		//////////////////////////////////////////////////
		if (nStrmSize > 0) {
			if (remain >= nStrmSize) {
				//printf("\n##################################");
				//printf("\n#   READING ONE UNIT FROM FILE   #");
				//printf("\n##################################\n");

				cnt++;
				memcpy(pStrmBuf, in_addr + (cnt * nStrmSize), nStrmSize);
				remain -= nStrmSize;
				nReadLeng = nStrmSize;
			}
			else {
				//printf("\n##########################################");
				//printf("\n##    READING THE LAST BLOCK OF FILE    ##");
				//printf("\n##########################################");

				cnt++;
				memcpy(pStrmBuf, in_addr + (cnt * nStrmSize), remain);
				nReadLeng = remain;
				remain = 0;
			}
		//	LOG_MSG(LOG_TRACE, "Test_Decoder_Ring_Buffer", "remain = %d\n", remain);
		}
		else {
			nReadLeng = 0;
		}

	#ifdef FPS
		gettimeofday(&start, NULL);
	#endif
	
		/////////////////////////////////
		///       6. DECODE           ///
		///    (SsbSipMfcDecodeExe)   ///
		/////////////////////////////////
		if (SsbSipMfcDecodeExe(handle, nReadLeng) != SSBSIP_MFC_DEC_RET_OK)
			break;

	#ifdef FPS
		gettimeofday(&stop, NULL);
		time += measureTime(&start, &stop);
		frame_cnt++;
	#endif
	
		/////////////////////////////////////////////
		///    7. Obtaining the Output Buffer     ///
		///      (SsbSipMfcDecodeGetOutBuf)       ///
		/////////////////////////////////////////////
		pYUVBuf = SsbSipMfcDecodeGetOutBuf(handle, &nYUVLeng);
	
	#ifndef FPS
		write(out_fd, pYUVBuf, (stream_info.width * stream_info.height * 3) >> 1);
	#endif
	}

#ifdef FPS
	LOG_MSG(LOG_TRACE, "Test_Decoder_Ring_Buffer", 	\
		"Decoding Time : %u, Frame Count : %d, FPS : %f\n", time, frame_cnt, (float)frame_cnt*1000/time);
#endif

	//////////////////////////////////////
	///    8. SsbSipMfcDecodeDeInit    ///
	//////////////////////////////////////
	SsbSipMfcDecodeDeInit(handle);
	munmap(in_addr, file_size);

	LOG_MSG(LOG_TRACE, "Test_Decoder_Ring_Buffer", "\n\n@@@ Program ends.\n");

	close(in_fd);
	close(out_fd);

	return 0;
}

