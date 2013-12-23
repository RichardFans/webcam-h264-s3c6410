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

#include "SsbSipMpeg4Decode.h"
#include "SsbSipLogMsg.h"

#define _MFCLIB_MPEG4_DEC_MAGIC_NUMBER		0x92241001

typedef struct
{
	int   	magic;
	int		hOpen;
	void   *p_buf;
	int     size;
	int     fInit;

	unsigned char	*mapped_addr;
	unsigned int     width, height;	
} _MFCLIB_MPEG4_DEC;


void *SsbSipMPEG4DecodeInit()
{
	_MFCLIB_MPEG4_DEC  	*pCTX;
	int	 	            hOpen;
	unsigned char		*addr;


	//////////////////////////////	
	/////     CreateFile     /////
	//////////////////////////////
	hOpen = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (hOpen < 0) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeInit", "MFC Open failure.\n");
		return NULL;
	}

	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (unsigned char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hOpen, 0);
	if (addr == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeInit", "MFC Mmap failure.\n");
		return NULL;
	}

	pCTX = (_MFCLIB_MPEG4_DEC *) malloc(sizeof(_MFCLIB_MPEG4_DEC));
	if (pCTX == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeInit", "malloc failed.\n");
		close(hOpen);
		return NULL;
	}
	memset(pCTX, 0, sizeof(_MFCLIB_MPEG4_DEC));

	pCTX->magic   		= _MFCLIB_MPEG4_DEC_MAGIC_NUMBER;
	pCTX->hOpen   		= hOpen;
	pCTX->fInit   		= 0;
	pCTX->mapped_addr	= addr;
	
	return (void *) pCTX;
}


int SsbSipMPEG4DecodeExe(void *openHandle, long lengthBufFill)
{
	_MFCLIB_MPEG4_DEC   *pCTX;
	MFC_ARGS            mfc_args;
	int                r;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeExe", "openHandle is NULL\n");
		return SSBSIP_MPEG4_DEC_RET_ERR_INVALID_HANDLE;
	}
	if ((lengthBufFill < 0) || (lengthBufFill > 0x100000)) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeExe", "lengthBufFill is invalid. (lengthBufFill=%d)\n", lengthBufFill);
		return SSBSIP_MPEG4_DEC_RET_ERR_INVALID_PARAM;
	}

	pCTX  = (_MFCLIB_MPEG4_DEC *) openHandle;


	if (!pCTX->fInit) {

		/////////////////////////////////////////////////
		/////           (DeviceIoControl)           /////
		/////       IOCTL_MFC_MPEG4_DEC_INIT        /////
		/////////////////////////////////////////////////
		mfc_args.dec_init.in_strmSize = lengthBufFill;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_MPEG4_DEC_INIT, &mfc_args);
		if ((r < 0) || (mfc_args.dec_init.ret_code < 0)) {
			return SSBSIP_MPEG4_DEC_RET_ERR_CONFIG_FAIL;
		}
		
		// Output argument (width , height)
		pCTX->width  = mfc_args.dec_init.out_width;
		pCTX->height = mfc_args.dec_init.out_height;

		pCTX->fInit = 1;

		return SSBSIP_MPEG4_DEC_RET_OK;
	}


	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////       IOCTL_MFC_MPEG4_DEC_EXE         /////
	/////////////////////////////////////////////////
	mfc_args.dec_exe.in_strmSize = lengthBufFill;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_MPEG4_DEC_EXE, &mfc_args);
	if ((r < 0) || (mfc_args.dec_exe.ret_code < 0)) {
		return SSBSIP_MPEG4_DEC_RET_ERR_DECODE_FAIL;
	}

	return SSBSIP_MPEG4_DEC_RET_OK;
}


int SsbSipMPEG4DecodeDeInit(void *openHandle)
{
	_MFCLIB_MPEG4_DEC  *pCTX;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeDeInit", "openHandle is NULL\n");
		return SSBSIP_MPEG4_DEC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_MPEG4_DEC *) openHandle;

	munmap(pCTX->mapped_addr, BUF_SIZE);
	close(pCTX->hOpen);


	return SSBSIP_MPEG4_DEC_RET_OK;
}


void *SsbSipMPEG4DecodeGetInBuf(void *openHandle, long size)
{
	void	*pStrmBuf;
	int		nStrmBufSize; 

	_MFCLIB_MPEG4_DEC	*pCTX;
	MFC_ARGS           	mfc_args;
	int               	r;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetInBuf", "openHandle is NULL\n");
		return NULL;
	}
	if ((size < 0) || (size > 0x100000)) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetInBuf", "size is invalid. (size=%d)\n", size);
		return NULL;
	}

	pCTX  = (_MFCLIB_MPEG4_DEC *) openHandle;

	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_STRM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_LINE_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetInBuf", "Failed in get LINE_BUF address\n");
		return NULL;
	}

	// Output arguments
	pStrmBuf     = (void *) mfc_args.get_buf_addr.out_buf_addr;	
	nStrmBufSize = mfc_args.get_buf_addr.out_buf_size;

	if ((long)nStrmBufSize < size) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetInBuf",	\
			"Requested size is greater than available buffer. (size=%d, avail=%d)\n", size, nStrmBufSize);
		return NULL;
	}

	return pStrmBuf;
}


void *SsbSipMPEG4DecodeGetOutBuf(void *openHandle, long *size)
{
	void	*pFramBuf;
	int		nFramBufSize;

	_MFCLIB_MPEG4_DEC  *pCTX;
	MFC_ARGS           mfc_args;
	int               r;

	
	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetOutBuf", "openHandle is NULL\n");
		return NULL;
	}
	if (size == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetOutBuf", "size is NULL\n");
		return NULL;
	}

	pCTX  = (_MFCLIB_MPEG4_DEC *) openHandle;



	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_FRAM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_FRAM_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetOutBuf", "Failed in get FRAM_BUF address.\n");
		return NULL;
	}

	// Output arguments
	pFramBuf     = (void *) mfc_args.get_buf_addr.out_buf_addr;
	nFramBufSize = mfc_args.get_buf_addr.out_buf_size;

	*size = nFramBufSize;

	return pFramBuf;
}


int SsbSipMPEG4DecodeSetConfig(void *openHandle, MPEG4_DEC_CONF conf_type, void *value)
{
	_MFCLIB_MPEG4_DEC  *pCTX;
	int					r = 0;
	MFC_SET_CONFIG_ARG	set_config;
	int					*inParam;	


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "openHandle is NULL\n");
		return SSBSIP_MPEG4_DEC_RET_ERR_INVALID_HANDLE;
	}
	if (value == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264DecodeSetConfig", "value is NULL\n");
		return SSBSIP_MPEG4_DEC_RET_ERR_INVALID_PARAM;
	}

	pCTX  = (_MFCLIB_MPEG4_DEC *) openHandle;

	switch(conf_type) {

	case MPEG4_DEC_SETCONF_POST_ROTATE:
		
		set_config.in_config_param     = MFC_SET_CONFIG_DEC_ROTATE;
		set_config.in_config_value[0]  = *((unsigned int *) value);
		set_config.in_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &set_config);
		if ( (r < 0) || (set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_DEC_SETCONF_POST_ROTATE.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case MPEG4_DEC_SETCONF_CACHE_CLEAN:
		inParam = (int *)value;
		set_config.in_config_param     = MFC_SET_CACHE_CLEAN;
		set_config.in_config_value[0]  = (int)inParam[0];
		set_config.in_config_value[1]  = (int)inParam[1];
		set_config.in_config_value[2]  = pCTX->mapped_addr;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &set_config);
		if ( (r < 0) || (set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_DEC_SETCONF_CACHE_CLEAN.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case MPEG4_DEC_SETCONF_CACHE_INVALIDATE:
		inParam = (int *)value;
		set_config.in_config_param     = MFC_SET_CACHE_INVALIDATE;
		set_config.in_config_value[0]  = (int)inParam[0];
		set_config.in_config_value[1]  = (int)inParam[1];
		set_config.in_config_value[2]  = pCTX->mapped_addr;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &set_config);
		if ( (r < 0) || (set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_DEC_SETCONF_INVALIDATE.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case MPEG4_DEC_SETCONF_CACHE_CLEAN_INVALIDATE:
		inParam = (int *)value;
		set_config.in_config_param     = MFC_SET_CACHE_CLEAN_INVALIDATE;
		set_config.in_config_value[0]  = (int)inParam[0];
		set_config.in_config_value[1]  = (int)inParam[1];
		set_config.in_config_value[2]  = pCTX->mapped_addr;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &set_config);
		if ( (r < 0) || (set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_DEC_SETCONF_CACHE_INVALIDATE.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case MPEG4_DEC_SETCONF_PADDING_SIZE:
		inParam = (int *)value;
		set_config.in_config_param     = MFC_SET_PADDING_SIZE;
		set_config.in_config_value[0]  = (int)inParam[0];
		set_config.in_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &set_config);
		if ( (r < 0) || (set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_DEC_SETCONF_PADDING_SIZE.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_SETCONF_FAIL;
		}
		break;
		
	default:
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "No such conf_type is supported.\n");
		return SSBSIP_MPEG4_DEC_RET_ERR_SETCONF_FAIL;
	}

	return SSBSIP_MPEG4_DEC_RET_OK;
}




int SsbSipMPEG4DecodeGetConfig(void *openHandle, MPEG4_DEC_CONF conf_type, void *value)
{
	_MFCLIB_MPEG4_DEC  *pCTX;
	int					r;
	MFC_ARGS			mfc_args;

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "openHandle is NULL\n");
		return SSBSIP_MPEG4_DEC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_MPEG4_DEC *) openHandle;


	switch (conf_type) {
	case MPEG4_DEC_GETCONF_STREAMINFO:
		((SSBSIP_MPEG4_STREAM_INFO *)value)->width  = pCTX->width;
		((SSBSIP_MPEG4_STREAM_INFO *)value)->height = pCTX->height;
		break;
		
	case MPEG4_DEC_GETCONF_PHYADDR_FRAM_BUF:
		
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_PHY_FRAM_BUF_ADDR, &mfc_args);
		if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Failed in get FRAM_BUF physical address. %d, %d\n", r, mfc_args.get_buf_addr.ret_code);
			return -1;
		}
		((unsigned int*) value)[0] = mfc_args.get_buf_addr.out_buf_addr;
		((unsigned int*) value)[1] = mfc_args.get_buf_addr.out_buf_size;
		
		break;

	case MPEG4_DEC_GETCONF_FRAM_NEED_COUNT:

		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_FRAME_NEED_COUNT;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;

		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MPEG4_DEC_GETCONF_FRAM_NEED_COUNT.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		((unsigned int *) value)[0] = mfc_args.get_config.out_config_value[0];
		break;

	case MPEG4_DEC_GETCONF_MPEG4_MV_ADDR:

		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_MP4ASP_MV;
		mfc_args.get_config.in_config_param2	 = (int)pCTX->mapped_addr;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;

		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_MP4ASP_MV.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((unsigned int*) value)[0] = mfc_args.get_config.out_config_value[0];
		((unsigned int*) value)[1] = mfc_args.get_config.out_config_value[1];
		
		break;

	case MPEG4_DEC_GETCONF_MPEG4_MBTYPE_ADDR:

		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_MP4ASP_MBTYPE;
		mfc_args.get_config.in_config_param2	 = (int)pCTX->mapped_addr;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;

		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_MP4ASP_MBTYPE_ADDR.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((unsigned int*) value)[0] = mfc_args.get_config.out_config_value[0];
		((unsigned int*) value)[1] = mfc_args.get_config.out_config_value[1];

		break;

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
	case MPEG4_DEC_GETCONF_BYTE_CONSUMED:
		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_BYTE_CONSUMED;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_BYTE_CONSUMED.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((unsigned int*) value)[0] = mfc_args.get_config.out_config_value[0];

		break;

	case MPEG4_DEC_GETCONF_MPEG4_FCODE:
		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_MP4ASP_FCODE;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_MP4ASP_FCODE.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((int*) value)[0] = mfc_args.get_config.out_config_value[0];

		break;

	case MPEG4_DEC_GETCONF_MPEG4_VOP_TIME_RES:
		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_MP4ASP_VOP_TIME_RES;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_MP4ASP_VOP_TIME_RES.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((int*) value)[0] = mfc_args.get_config.out_config_value[0];

		break;

	case MPEG4_DEC_GETCONF_MPEG4_TIME_BASE_LAST:
		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_MP4ASP_TIME_BASE_LAST;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_MP4ASP_TIME_BASE_LAST.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((int*) value)[0] = mfc_args.get_config.out_config_value[0];

		break;

	case MPEG4_DEC_GETCONF_MPEG4_NONB_TIME_LAST:
		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_MP4ASP_NONB_TIME_LAST;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_MP4ASP_NONB_TIME_LAST.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((int*) value)[0] = mfc_args.get_config.out_config_value[0];

		break;

	case MPEG4_DEC_GETCONF_MPEG4_TRD:
		mfc_args.get_config.in_config_param      = MFC_GET_CONFIG_DEC_MP4ASP_TRD;
		mfc_args.get_config.out_config_value[0]  = 0;
		mfc_args.get_config.out_config_value[1]  = 0;
		r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.get_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "Error in MFC_GET_CONFIG_DEC_MP4ASP_TRD.\n");
			return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
		}

		// Output arguments
		((int*) value)[0] = mfc_args.get_config.out_config_value[0];

		break;


#endif

	default:
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeGetConfig", "No such conf_type is supported.\n");
		return SSBSIP_MPEG4_DEC_RET_ERR_GETCONF_FAIL;
	}


	return SSBSIP_MPEG4_DEC_RET_OK;
}



