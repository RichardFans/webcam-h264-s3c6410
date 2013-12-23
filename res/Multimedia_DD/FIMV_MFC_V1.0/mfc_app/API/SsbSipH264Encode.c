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

#include "SsbSipH264Encode.h"
#include "SsbSipLogMsg.h"


#define _MFCLIB_H264_ENC_MAGIC_NUMBER		0x92242002

typedef struct
{
	int   	magic;
	int  	hOpen;
	int     fInit;
	int     enc_strm_size;
	int		enc_hdr_size;
	
	unsigned int  	width, height;
	unsigned int  	framerate, bitrate;
	unsigned int  	gop_num;
	unsigned char	*mapped_addr;
} _MFCLIB_H264_ENC;


typedef struct {
	int width;
	int height;
	int frameRateRes;
	int frameRateDiv;
	int gopNum;
	int bitrate;
} enc_info_t;


void *SsbSipH264EncodeInit(unsigned int uiWidth,     unsigned int uiHeight,
                           unsigned int uiFramerate, unsigned int uiBitrate_kbps,
                           unsigned int uiGOPNum)
{
	_MFCLIB_H264_ENC	*pCTX;
	int					hOpen;
	unsigned char		*addr;


	//////////////////////////////
	/////     CreateFile     /////
	//////////////////////////////
	hOpen = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (hOpen < 0) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeInit", "MFC Open failure.\n");
		return NULL;
	}

	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (unsigned char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hOpen, 0);
	if (addr == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeInit", "MFC Mmap failure.\n");
		return NULL;
	}

	pCTX = (_MFCLIB_H264_ENC *) malloc(sizeof(_MFCLIB_H264_ENC));
	if (pCTX == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeInit", "malloc failed.\n");
		close(hOpen);
		return NULL;
	}
	memset(pCTX, 0, sizeof(_MFCLIB_H264_ENC));

	pCTX->magic = _MFCLIB_H264_ENC_MAGIC_NUMBER;
	pCTX->hOpen = hOpen;
	pCTX->fInit = 0;
	pCTX->mapped_addr	= addr;

	pCTX->width     = uiWidth;
	pCTX->height    = uiHeight;
	pCTX->framerate = uiFramerate;
	pCTX->bitrate   = uiBitrate_kbps;
	pCTX->gop_num   = uiGOPNum;

	pCTX->enc_strm_size = 0;

	return (void *) pCTX;
}


int SsbSipH264EncodeExe(void *openHandle)
{
	_MFCLIB_H264_ENC	*pCTX;
	int					r;
	MFC_ARGS			mfc_args;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeExe", "openHandle is NULL\n");
		return SSBSIP_H264_ENC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_H264_ENC *) openHandle;

	if (!pCTX->fInit) {
		mfc_args.enc_init.in_width		= pCTX->width;
		mfc_args.enc_init.in_height		= pCTX->height;
		mfc_args.enc_init.in_bitrate	= pCTX->bitrate;
		mfc_args.enc_init.in_gopNum		= pCTX->gop_num;
		mfc_args.enc_init.in_frameRateRes	= pCTX->framerate;
		mfc_args.enc_init.in_frameRateDiv	= 0;


		////////////////////////////////////////////////
		/////          (DeviceIoControl)           /////
		/////       IOCTL_MFC_H264_DEC_INIT        /////
		////////////////////////////////////////////////
		r = ioctl(pCTX->hOpen, IOCTL_MFC_H264_ENC_INIT, &mfc_args); 
		if ((r < 0) || (mfc_args.enc_init.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipH264EncodeInit", "IOCTL_MFC_H264_ENC_INIT failed.\n");
			return SSBSIP_H264_ENC_RET_ERR_ENCODE_FAIL;
		}

		pCTX->fInit = 1;

		return SSBSIP_H264_ENC_RET_OK;
	}
	


	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////       IOCTL_MFC_MPEG4_DEC_EXE         /////
	/////////////////////////////////////////////////
	r = ioctl(pCTX->hOpen, IOCTL_MFC_H264_ENC_EXE, &mfc_args);
	if ((r < 0) || (mfc_args.enc_exe.ret_code < 0)) {
		return SSBSIP_H264_ENC_RET_ERR_ENCODE_FAIL;
	}

	// Encoded stream size is saved. (This value is returned in SsbSipH264EncodeGetOutBuf)
	pCTX->enc_strm_size = mfc_args.enc_exe.out_encoded_size;

	if(mfc_args.enc_exe.out_header_size > 0) {
		pCTX->enc_hdr_size = mfc_args.enc_exe.out_header_size;
		LOG_MSG(LOG_TRACE, "SsbSipH264EncodeExe", "HEADER SIZE = %d\n", pCTX->enc_hdr_size);
	}
	

	return SSBSIP_H264_ENC_RET_OK;
}


int SsbSipH264EncodeDeInit(void *openHandle)
{
	_MFCLIB_H264_ENC  *pCTX;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeDeInit", "openHandle is NULL\n");
		return SSBSIP_H264_ENC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_H264_ENC *) openHandle;


	munmap(pCTX->mapped_addr, BUF_SIZE);
	close(pCTX->hOpen);


	return SSBSIP_H264_ENC_RET_OK;
}


void *SsbSipH264EncodeGetInBuf(void *openHandle, long size)
{
	_MFCLIB_H264_ENC	*pCTX;
	int					r;
	MFC_ARGS			mfc_args;
	

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeGetInBuf", "openHandle is NULL\n");
		return NULL;
	}
	if ((size < 0) || (size > 0x100000)) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeGetInBuf", "size is invalid. (size=%d)\n", size);
		return NULL;
	}

	pCTX  = (_MFCLIB_H264_ENC *) openHandle;


	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////     IOCTL_MFC_GET_INPUT_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_FRAM_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeGetInBuf", "Failed in get FRAM_BUF address\n");
		return NULL;
	}

	return (void *)mfc_args.get_buf_addr.out_buf_addr;
}


void *SsbSipH264EncodeGetOutBuf(void *openHandle, long *size)
{
	_MFCLIB_H264_ENC	*pCTX;
	int					r;
	MFC_ARGS			mfc_args;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeGetOutBuf", "openHandle is NULL\n");
		return NULL;
	}
	if (size == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeGetOutBuf", "size is NULL.\n");
		return NULL;
	}

	pCTX  = (_MFCLIB_H264_ENC *) openHandle;

	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_STRM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_LINE_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeGetOutBuf", "Failed in get LINE_BUF address.\n");
		return NULL;
	}

	*size = pCTX->enc_strm_size;

	return (void *)mfc_args.get_buf_addr.out_buf_addr;
}


int SsbSipH264EncodeSetConfig(void *openHandle, H264_ENC_CONF conf_type, void *value)
{
	_MFCLIB_H264_ENC  	*pCTX;
	MFC_ARGS			mfc_args;
	int					r;

	unsigned int		num_slices[2];


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeSetConfig", "openHandle is NULL\n");
		return SSBSIP_H264_ENC_RET_ERR_INVALID_HANDLE;
	}
	if (value == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeSetConfig", "value is NULL\n");
		return SSBSIP_H264_ENC_RET_ERR_INVALID_PARAM;
	}

	pCTX  = (_MFCLIB_H264_ENC *) openHandle;

	switch (conf_type) {
	case H264_ENC_SETCONF_NUM_SLICES:
			
		num_slices[0] = ((unsigned int *)value)[0];
		num_slices[1] = ((unsigned int *)value)[1];
		printf("num slices[0] = %d\n", num_slices[0]);
		printf("num slices[1] = %d\n", num_slices[1]);
		
		mfc_args.set_config.in_config_param		= MFC_SET_CONFIG_ENC_SLICE_MODE;
		mfc_args.set_config.in_config_value[0]	= num_slices[0];
		mfc_args.set_config.in_config_value[1]	= num_slices[1];
		
		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &mfc_args);
		if ( (r < 0) || (mfc_args.set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipH264EncodeSetConfig", "Error in H264_ENC_SETCONF_NUM_SLICES.\n");
			return SSBSIP_H264_ENC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case H264_ENC_SETCONF_PARAM_CHANGE:

		mfc_args.set_config.in_config_param		= MFC_SET_CONFIG_ENC_PARAM_CHANGE;
		mfc_args.set_config.in_config_value[0]	= ((unsigned int *) value)[0];
		mfc_args.set_config.in_config_value[1]	= ((unsigned int *) value)[1];

		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &mfc_args);
		if ( (r < 0) || (mfc_args.set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipH264EncodeSetConfig", "Error in H264_ENC_SETCONF_PARA_CHANGE.\n");
			return SSBSIP_H264_ENC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case H264_ENC_SETCONF_CUR_PIC_OPT:

		mfc_args.set_config.in_config_param     = MFC_SET_CONFIG_ENC_CUR_PIC_OPT;
		mfc_args.set_config.in_config_value[0]  = ((unsigned int *) value)[0];
		mfc_args.set_config.in_config_value[1]  = ((unsigned int *) value)[1];

		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &mfc_args);
		if ( (r < 0) || (mfc_args.set_config.ret_code < 0) ) {
			LOG_MSG(LOG_ERROR, "SsbSipH264EncodeSetConfig", "Error in H264_ENC_SETCONF_CUR_PIC_OPT.\n");
			return SSBSIP_H264_ENC_RET_ERR_SETCONF_FAIL;
		}
		break;
		
	default:
		LOG_MSG(LOG_ERROR, "SsbSipH264EncodeSetConfig", "No such conf_type is supported.\n");
		return SSBSIP_H264_ENC_RET_ERR_SETCONF_FAIL;
	}

	
	return SSBSIP_H264_ENC_RET_OK;
}


int SsbSipH264EncodeGetConfig(void *openHandle, H264_ENC_CONF conf_type, void *value)
{
	_MFCLIB_H264_ENC  *pCTX;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		return -1;
	}

	pCTX  = (_MFCLIB_H264_ENC *) openHandle;


	switch (conf_type) {

	case H264_ENC_GETCONF_HEADER_SIZE:
		*((int *)value) = pCTX->enc_hdr_size;
		break;

	default:
		break;
	}


	return SSBSIP_H264_ENC_RET_OK;
}

