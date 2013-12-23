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

#include "SsbSipVC1Decode.h"
#include "SsbSipLogMsg.h"

#define _MFCLIB_VC1_DEC_MAGIC_NUMBER		0x92241002

typedef struct
{
	int		magic;
	int		hOpen;
	void	*p_buf;
	int		size;
	int     fInit;

	unsigned int	width, height;
	unsigned char	*mapped_addr;	
} _MFCLIB_VC1_DEC;



void *SsbSipVC1DecodeInit()
{
	_MFCLIB_VC1_DEC		*pCTX;
	int					hOpen;
	unsigned char		*addr;

	
	//////////////////////////////
	/////     CreateFile     /////
	//////////////////////////////
	hOpen = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (hOpen < 0) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeInit", "MFC Open failure.\n");
		return NULL;
	}

	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (unsigned char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hOpen, 0);
	if (addr == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeInit", "MFC Mmap failure.\n");
		return NULL;
	}

	pCTX = (_MFCLIB_VC1_DEC *) malloc(sizeof(_MFCLIB_VC1_DEC));
	if (pCTX == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeInit", "malloc failed.\n");
		close(hOpen);
		return NULL;
	}
	memset(pCTX, 0, sizeof(_MFCLIB_VC1_DEC));

	pCTX->magic   		= _MFCLIB_VC1_DEC_MAGIC_NUMBER;
	pCTX->hOpen   		= hOpen;
	pCTX->fInit   		= 0;
	pCTX->mapped_addr	= addr;
	
	return (void *) pCTX;
}


int SsbSipVC1DecodeExe(void *openHandle, long lengthBufFill)
{
	_MFCLIB_VC1_DEC		*pCTX;
	MFC_ARGS			mfc_args;
	int					r;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeExe", "openHandle is NULL\n");
		return SSBSIP_VC1_DEC_RET_ERR_INVALID_HANDLE;
	}
	if ((lengthBufFill < 0) || (lengthBufFill > 0x100000)) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeExe", "lengthBufFill is invalid. (lengthBufFill=%d)\n", lengthBufFill);
		return SSBSIP_VC1_DEC_RET_ERR_INVALID_PARAM;
	}


	pCTX  = (_MFCLIB_VC1_DEC *) openHandle;


	if (!pCTX->fInit) {

		/////////////////////////////////////////////////
		/////           (DeviceIoControl)           /////
		/////       IOCTL_MFC_VC1_DEC_EXE         /////
		/////////////////////////////////////////////////
		mfc_args.dec_init.in_strmSize = lengthBufFill;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_VC1_DEC_INIT, &mfc_args);
		if ((r < 0) || (mfc_args.dec_init.ret_code < 0)) {
			return SSBSIP_VC1_DEC_RET_ERR_CONFIG_FAIL;
		}

		// Output argument (width , height)
		pCTX->width  = mfc_args.dec_init.out_width;
		pCTX->height = mfc_args.dec_init.out_height;

		pCTX->fInit = 1;

		return SSBSIP_VC1_DEC_RET_OK;
	}


	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////       IOCTL_MFC_VC1_DEC_EXE         /////
	/////////////////////////////////////////////////
	mfc_args.dec_exe.in_strmSize = lengthBufFill;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_VC1_DEC_EXE, &mfc_args);
	if ((r < 0) || (mfc_args.dec_exe.ret_code < 0)) {
		return SSBSIP_VC1_DEC_RET_ERR_DECODE_FAIL;
	}

	return SSBSIP_VC1_DEC_RET_OK;
}


int SsbSipVC1DecodeDeInit(void *openHandle)
{
	_MFCLIB_VC1_DEC  *pCTX;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeDeInit", "openHandle is NULL\n");
		return SSBSIP_VC1_DEC_RET_ERR_INVALID_HANDLE;
	}


	pCTX  = (_MFCLIB_VC1_DEC *) openHandle;

	munmap(pCTX->mapped_addr, BUF_SIZE);
	close(pCTX->hOpen);


	return SSBSIP_VC1_DEC_RET_OK;
}


void *SsbSipVC1DecodeGetInBuf(void *openHandle, long size)
{
	void	*pStrmBuf;
	int		nStrmBufSize; 

	_MFCLIB_VC1_DEC	*pCTX;
	MFC_ARGS		mfc_args;
	int				r;

	
	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetInBuf", "openHandle is NULL\n");
		return NULL;
	}
	if ((size < 0) || (size > 0x100000)) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetInBuf", "size is invalid. (size=%d)\n", size);
		return NULL;
	}

	pCTX  = (_MFCLIB_VC1_DEC *) openHandle;



	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_STRM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_LINE_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetInBuf", "Failed in get LINE_BUF address\n");
		return NULL;
	}

	// Output arguments
	pStrmBuf     = (void *) mfc_args.get_buf_addr.out_buf_addr;	
	nStrmBufSize = mfc_args.get_buf_addr.out_buf_size;

	if ((long)nStrmBufSize < size) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetInBuf", "Requested size is greater than available buffer. (size=%d, avail=%d)\n",
				size, nStrmBufSize);

		return NULL;
	}

	return pStrmBuf;
}


void *SsbSipVC1DecodeGetOutBuf(void *openHandle, long *size)
{
	void	*pFramBuf;
	int		nFramBufSize;

	_MFCLIB_VC1_DEC	*pCTX;
	MFC_ARGS		mfc_args;
	int				r;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetOutBuf", "openHandle is NULL\n");
		return NULL;
	}
	if (size == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetOutBuf", "size is NULL\n");
		return NULL;
	}

	pCTX  = (_MFCLIB_VC1_DEC *) openHandle;



	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_STRM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_FRAM_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetOutBuf", "Failed in get FRAM_BUF address.\n");
		return NULL;
	}

	// Output arguments
	pFramBuf     = (void *) mfc_args.get_buf_addr.out_buf_addr;
	nFramBufSize = mfc_args.get_buf_addr.out_buf_size;

	*size = nFramBufSize;

	return pFramBuf;
}



int SsbSipVC1DecodeSetConfig(void *openHandle, VC1_DEC_CONF conf_type, void *value)
{
	_MFCLIB_VC1_DEC  *pCTX;
	int					r = 0;
	MFC_SET_CONFIG_ARG	set_config;
	

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeSetConfig", "openHandle is NULL\n");
		return SSBSIP_VC1_DEC_RET_ERR_INVALID_HANDLE;
	}
	if (value == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeSetConfig", "value is NULL\n");
		return SSBSIP_VC1_DEC_RET_ERR_INVALID_PARAM;
	}

	pCTX  = (_MFCLIB_VC1_DEC *) openHandle;

	switch(conf_type) {

	case VC1_DEC_SETCONF_POST_ROTATE:
		
		set_config.in_config_param     = MFC_SET_CONFIG_DEC_ROTATE;
		set_config.in_config_value[0]  = *((unsigned int *) value);
		set_config.in_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &set_config);
		if ( (r < 0) || (set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeSetConfig", "Error in VC1_DEC_SETCONF_POST_ROTATE.\n");
			return SSBSIP_VC1_DEC_RET_ERR_SETCONF_FAIL;
		}
		break;

	default:
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeSetConfig", "No such conf_type is supported.\n");
		return SSBSIP_VC1_DEC_RET_ERR_SETCONF_FAIL;
	}

	return SSBSIP_VC1_DEC_RET_OK;
}




int SsbSipVC1DecodeGetConfig(void *openHandle, VC1_DEC_CONF conf_type, void *value)
{
	_MFCLIB_VC1_DEC  *pCTX;
	int					r;
	MFC_ARGS			mfc_args;

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetConfig", "openHandle is NULL\n");
		return SSBSIP_VC1_DEC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_VC1_DEC *) openHandle;


	switch (conf_type) {

	case VC1_DEC_GETCONF_STREAMINFO:
		((SSBSIP_VC1_STREAM_INFO *)value)->width  = pCTX->width;
		((SSBSIP_VC1_STREAM_INFO *)value)->height = pCTX->height;
		break;
		
	case VC1_DEC_GETCONF_PHYADDR_FRAM_BUF:
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_PHY_FRAM_BUF_ADDR, &mfc_args);
		if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetConfig", "Failed in get FRAM_BUF physical address.\n");
			return -1;
		}
		((unsigned int *) value)[0] = mfc_args.get_buf_addr.out_buf_addr;
		((unsigned int *) value)[1] = mfc_args.get_buf_addr.out_buf_size;
		
		break;

	case VC1_DEC_GETCONF_FRAM_NEED_COUNT:
		mfc_args.get_config.in_config_param 	= MFC_GET_CONFIG_DEC_FRAME_NEED_COUNT;
		mfc_args.get_config.out_config_value[0]	= 0;
		mfc_args.get_config.out_config_value[1]	= 0;
		
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r <0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetConfig", "Error in VC1_DEC_GETCONF_FRAM_NEED_COUNT.\n");
			return SSBSIP_VC1_DEC_RET_ERR_GETCONF_FAIL;
		}
		((int *) value)[0] = mfc_args.get_config.out_config_value[0];

		break;
		
	default:
		LOG_MSG(LOG_ERROR, "SsbSipVC1DecodeGetConfig", "No such conf_type is supported.\n");
		return SSBSIP_VC1_DEC_RET_ERR_GETCONF_FAIL;
	}


	return SSBSIP_VC1_DEC_RET_OK;
}


