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

#include "MfcDriver.h"
#include "MfcDrvParams.h"

#include "SsbSipMfcDecode.h"
#include "SsbSipLogMsg.h"

#define _MFCLIB_DEC_MAGIC_NUMBER		0x92241000

typedef struct
{
	int		magic;
	int		hOpen;
	void	*p_buf;
	int		size;
	int		fInit;

	unsigned char	*mapped_addr;
	unsigned int	width, height;	
	int     		decoder;
} _MFCLIB_DEC;



void *SsbSipMfcDecodeInit(int dec_type)
{
	_MFCLIB_DEC		*pCTX;
	int				hOpen;
	unsigned char	*addr;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (   (dec_type != SSBSIPMFCDEC_MPEG4)
		&& (dec_type != SSBSIPMFCDEC_H263)
		&& (dec_type != SSBSIPMFCDEC_H264)
		&& (dec_type != SSBSIPMFCDEC_VC1)) {

		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeInit", "Undefined codec type.\n");
		return NULL;
	}


	//////////////////////////////
	/////     CreateFile     /////
	//////////////////////////////
	hOpen = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (hOpen < 0) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeInit", "MFC Open failure.\n");
		return NULL;
	}

	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (unsigned char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hOpen, 0);
	if (addr == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeInit", "MFC Mmap failure.\n");
		return NULL;
	}

	pCTX = (_MFCLIB_DEC *) malloc(sizeof(_MFCLIB_DEC));
	if (pCTX == NULL) {
		LOG_MSG(LOG_ERROR, "[SsbSipMfcDecodeInit", "malloc failed.\n");
		close(hOpen);
		return NULL;
	}
	memset(pCTX, 0, sizeof(_MFCLIB_DEC));

	pCTX->magic   		= _MFCLIB_DEC_MAGIC_NUMBER;
	pCTX->hOpen   		= hOpen;
	pCTX->decoder 		= dec_type;
	pCTX->mapped_addr	= addr;
	pCTX->fInit   		= 0;

	return (void *) pCTX;
}


int SsbSipMfcDecodeExe(void *openHandle, long lengthBufFill)
{
	_MFCLIB_DEC    *pCTX;
	MFC_ARGS        mfc_args;
	int            r;

	int  ioctl_cmd;

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeExe", "openHandle is NULL\n");
		return SSBSIP_MFC_DEC_RET_ERR_INVALID_HANDLE;
	}
	if ((lengthBufFill < 0) || (lengthBufFill > 0x100000)) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeExe", "lengthBufFill is invalid. (lengthBufFill=%d)\n", lengthBufFill);
		return SSBSIP_MFC_DEC_RET_ERR_INVALID_PARAM;
	}

	pCTX  = (_MFCLIB_DEC *) openHandle;

	if (!pCTX->fInit) {
		switch (pCTX->decoder) {
		case SSBSIPMFCDEC_MPEG4:
		case SSBSIPMFCDEC_H263:
			ioctl_cmd = IOCTL_MFC_MPEG4_DEC_INIT;
			break;

		case SSBSIPMFCDEC_H264:
			ioctl_cmd = IOCTL_MFC_H264_DEC_INIT;
			break;

		case SSBSIPMFCDEC_VC1:
			ioctl_cmd = IOCTL_MFC_VC1_DEC_INIT;
			break;

		default:
			LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeExe", "Undefined codec type.\n");
			return SSBSIP_MFC_DEC_RET_ERR_UNDEF_CODEC;
		}

		/////////////////////////////////////////////////
		/////           (DeviceIoControl)           /////
		/////      	IOCTL_MFC_H264_DEC_EXE          /////
		/////////////////////////////////////////////////
		mfc_args.dec_init.in_strmSize = lengthBufFill;
		r = ioctl(pCTX->hOpen, ioctl_cmd, &mfc_args);
		if ((r < 0) || (mfc_args.dec_init.ret_code < 0)) {
			return SSBSIP_MFC_DEC_RET_ERR_CONFIG_FAIL;
		}
		
		// Output argument (width , height)
		pCTX->width  = mfc_args.dec_init.out_width;
		pCTX->height = mfc_args.dec_init.out_height;

		pCTX->fInit = 1;

		return SSBSIP_MFC_DEC_RET_OK;
	}

	switch (pCTX->decoder) {
	case SSBSIPMFCDEC_MPEG4:
	case SSBSIPMFCDEC_H263:
		ioctl_cmd = IOCTL_MFC_MPEG4_DEC_EXE;
		break;

	case SSBSIPMFCDEC_H264:
		ioctl_cmd = IOCTL_MFC_H264_DEC_EXE;
		break;

	case SSBSIPMFCDEC_VC1:
		ioctl_cmd = IOCTL_MFC_VC1_DEC_EXE;
		break;

	default:
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeExe", "Undefined codec type.\n");
		return SSBSIP_MFC_DEC_RET_ERR_UNDEF_CODEC;
	}

	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////       IOCTL_MFC_H264_DEC_EXE          /////
	/////////////////////////////////////////////////
	mfc_args.dec_exe.in_strmSize = lengthBufFill;
	r = ioctl(pCTX->hOpen, ioctl_cmd, &mfc_args);
	if ((r < 0) || (mfc_args.dec_exe.ret_code < 0)) {
		return SSBSIP_MFC_DEC_RET_ERR_DECODE_FAIL;
	}
	 
	return SSBSIP_MFC_DEC_RET_OK;
}


int SsbSipMfcDecodeDeInit(void *openHandle)
{
	_MFCLIB_DEC  *pCTX;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeExe", "openHandle is NULL\n");
		return SSBSIP_MFC_DEC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_DEC *) openHandle;

	munmap(pCTX->mapped_addr, BUF_SIZE);
	close(pCTX->hOpen);


	return SSBSIP_MFC_DEC_RET_OK;
}


void *SsbSipMfcDecodeGetInBuf(void *openHandle, long *size)
{
	void	*pStrmBuf;
	int		nStrmBufSize; 

	_MFCLIB_DEC	*pCTX;
	MFC_ARGS	mfc_args;
	int			r;

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetInBuf", "openHandle is NULL\n");
		return NULL;
	}
	if (size == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetInBuf", "size is NULL.\n");
		return NULL;
	}

	pCTX  = (_MFCLIB_DEC *) openHandle;

	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_STRM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_RING_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetInBuf", "Failed in get RING_BUF address\n");
		return NULL;
	}
	
	// Output arguments
	pStrmBuf     = (void *) mfc_args.get_buf_addr.out_buf_addr;	
	nStrmBufSize = mfc_args.get_buf_addr.out_buf_size;

	*size = nStrmBufSize;

	return pStrmBuf;
}


void *SsbSipMfcDecodeGetOutBuf(void *openHandle, long *size)
{
	void	*pFramBuf;
	int		nFramBufSize;

	_MFCLIB_DEC	*pCTX;
	MFC_ARGS	mfc_args;
	int		r;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetOutBuf", "openHandle is NULL\n");
		return NULL;
	}
	if (size == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetOutBuf", "size is NULL.\n");
		return NULL;
	}

	pCTX  = (_MFCLIB_DEC *) openHandle;

	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_FRAM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_FRAM_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetOutBuf", "Failed in get FRAM_BUF address\n");
		return NULL;
	}
	
	// Output arguments
	pFramBuf     = (void *) mfc_args.get_buf_addr.out_buf_addr;
	nFramBufSize = mfc_args.get_buf_addr.out_buf_size;

	*size = nFramBufSize;

	return pFramBuf;
}



int SsbSipMfcDecodeSetConfig(void *openHandle, MFC_DEC_CONF conf_type, void *value)
{
	_MFCLIB_DEC  		*pCTX;
	MFC_SET_CONFIG_ARG	set_config;
	int					r = 0;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_TRACE, "SsbSipMfcDecodeSetConfig", "openHandle is NULL\n");
		return SSBSIP_MFC_DEC_RET_ERR_INVALID_HANDLE;
	}
	if (value == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeSetConfig", "value is NULL\n");
		return SSBSIP_MFC_DEC_RET_ERR_INVALID_PARAM;
	}

	pCTX  = (_MFCLIB_DEC *) openHandle;

	switch(conf_type) {
		case MFC_DEC_SETCONF_POST_ROTATE:

		set_config.in_config_param     = MFC_SET_CONFIG_DEC_ROTATE;
		set_config.in_config_value[0]  = *((unsigned int *) value);
		set_config.in_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &set_config);
		if ( (r < 0) || (set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeSetConfig", "Error in MFC_DEC_SETCONF_POST_ROTATE.\n");
			return SSBSIP_MFC_DEC_RET_ERR_SETCONF_FAIL;
		}
		break;

	default:
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeSetConfig", "No such conf_type is supported.\n");
		return SSBSIP_MFC_DEC_RET_ERR_SETCONF_FAIL;
	}

	return SSBSIP_MFC_DEC_RET_OK;
}


int SsbSipMfcDecodeGetConfig(void *openHandle, MFC_DEC_CONF conf_type, void *value)
{
	_MFCLIB_DEC  	*pCTX;
	int				r;
	MFC_ARGS		mfc_args;

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetConfig", "openHandle is NULL\n");
		return SSBSIP_MFC_DEC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_DEC *) openHandle;

	switch (conf_type) {

	case MFC_DEC_GETCONF_STREAMINFO:
		((SSBSIP_MFC_STREAM_INFO *)value)->width  = pCTX->width;
		((SSBSIP_MFC_STREAM_INFO *)value)->height = pCTX->height;
		break;

	case MFC_DEC_GETCONF_PHYADDR_FRAM_BUF:
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_PHY_FRAM_BUF_ADDR, &mfc_args);
		if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetConfig", "Failed in get FRAM_BUF physical address.\n");
			return -1;
		}
		((unsigned int *) value)[0] = mfc_args.get_buf_addr.out_buf_addr;
		((unsigned int *) value)[1] = mfc_args.get_buf_addr.out_buf_size;
		
		break;

	case MFC_DEC_GETCONF_FRAM_NEED_COUNT:
		mfc_args.get_config.in_config_param 	= MFC_GET_CONFIG_DEC_FRAME_NEED_COUNT;
		mfc_args.get_config.out_config_value[0]	= 0;
		mfc_args.get_config.out_config_value[1]	= 0;
		
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r <0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMfcDecodeGetConfig", "Error in MFC_DEC_GETCONF_FRAM_NEED_COUNT.\n");
			return SSBSIP_MFC_DEC_RET_ERR_GETCONF_FAIL;
		}
		((int *) value)[0] = mfc_args.get_config.out_config_value[0];

		break;
		
	default:
		break;
	}

	return SSBSIP_MFC_DEC_RET_OK;
}


