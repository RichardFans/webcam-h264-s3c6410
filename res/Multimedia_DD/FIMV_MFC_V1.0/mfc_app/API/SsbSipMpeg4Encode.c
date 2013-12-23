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

#include "SsbSipMpeg4Encode.h"
#include "SsbSipLogMsg.h"


#define _MFCLIB_MPEG4_ENC_MAGIC_NUMBER		0x92242002

typedef struct
{
	int   	magic;
	int  	hOpen;
	int     fInit;
	int     enc_strm_size;
	int		enc_hdr_size;
	int		strmType;
	
	unsigned int  	width, height;
	unsigned int  	framerate, bitrate;
	unsigned int  	gop_num;
	unsigned char	*mapped_addr;
} _MFCLIB_MPEG4_ENC;


typedef struct {
	int width;
	int height;
	int frameRateRes;
	int frameRateDiv;
	int gopNum;
	int bitrate;
} enc_info_t;


void *SsbSipMPEG4EncodeInit(int strmType, unsigned int uiWidth,     unsigned int uiHeight,
                           unsigned int uiFramerate, unsigned int uiBitrate_kbps,
                           unsigned int uiGOPNum)
{
	_MFCLIB_MPEG4_ENC	*pCTX;
	int					hOpen;
	int					cmd;
	unsigned char		*addr;


	switch(strmType) {
		case SSBSIPMFCENC_MPEG4:
			cmd = IOCTL_MFC_MPEG4_ENC_INIT;
			break;
		case SSBSIPMFCENC_H263:
			cmd = IOCTL_MFC_H263_ENC_INIT;
			break;
		default:
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeInit", "stream type is not defined\n");
			return NULL;
	}

	//////////////////////////////
	/////     CreateFile     /////
	//////////////////////////////
	hOpen = open(MFC_DEV_NAME, O_RDWR|O_NDELAY);
	if (hOpen < 0) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeInit", "MFC Open failure.\n");
		return NULL;
	}

	//////////////////////////////////////////
	//	Mapping the MFC Input/Output Buffer	//
	//////////////////////////////////////////
	// mapping shared in/out buffer between application and MFC device driver
	addr = (unsigned char *) mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hOpen, 0);
	if (addr == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeInit", "MFC Mmap failure.\n");
		return NULL;
	}

	pCTX = (_MFCLIB_MPEG4_ENC *) malloc(sizeof(_MFCLIB_MPEG4_ENC));
	if (pCTX == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeInit", "malloc failed.\n");
		close(hOpen);
		return NULL;
	}
	memset(pCTX, 0, sizeof(_MFCLIB_MPEG4_ENC));

	pCTX->magic = _MFCLIB_MPEG4_ENC_MAGIC_NUMBER;
	pCTX->hOpen = hOpen;
	pCTX->fInit = 0;
	pCTX->mapped_addr	= addr;
	pCTX->strmType		= strmType;

	pCTX->width     = uiWidth;
	pCTX->height    = uiHeight;
	pCTX->framerate = uiFramerate;
	pCTX->bitrate   = uiBitrate_kbps;
	pCTX->gop_num   = uiGOPNum;

	pCTX->enc_strm_size = 0;

	return (void *) pCTX;
}


int SsbSipMPEG4EncodeExe(void *openHandle)
{
	_MFCLIB_MPEG4_ENC	*pCTX;
	int					r;
	int					cmd_init, cmd_exe;
	MFC_ARGS			mfc_args;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeExe", "openHandle is NULL\n");
		return SSBSIP_MPEG4_ENC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_MPEG4_ENC *) openHandle;

	switch(pCTX->strmType) {
		case SSBSIPMFCENC_MPEG4:
			cmd_init 	= IOCTL_MFC_MPEG4_ENC_INIT;
			cmd_exe		= IOCTL_MFC_MPEG4_ENC_EXE;
			break;
		case SSBSIPMFCENC_H263:
			cmd_init	= IOCTL_MFC_H263_ENC_INIT;
			cmd_exe 	= IOCTL_MFC_H263_ENC_EXE;
			break;
		default:
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeExe", "stream type is not defined\n");
			return -1;
	}

	if (!pCTX->fInit) {
		mfc_args.enc_init.in_width		= pCTX->width;
		mfc_args.enc_init.in_height		= pCTX->height;
		mfc_args.enc_init.in_bitrate	= pCTX->bitrate;
		mfc_args.enc_init.in_gopNum		= pCTX->gop_num;
		mfc_args.enc_init.in_frameRateRes	= pCTX->framerate;
		mfc_args.enc_init.in_frameRateDiv	= 0;


		////////////////////////////////////////////////
		/////          (DeviceIoControl)           /////
		/////       IOCTL_MFC_MPEG4_DEC_INIT        /////
		////////////////////////////////////////////////
		r = ioctl(pCTX->hOpen, cmd_init, &mfc_args); 
		if ((r < 0) || (mfc_args.enc_init.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeInit", "IOCTL_MFC_MPEG4_ENC_INIT failed.\n");
			return SSBSIP_MPEG4_ENC_RET_ERR_ENCODE_FAIL;
		}

		pCTX->fInit = 1;

		return SSBSIP_MPEG4_ENC_RET_OK;
	}

	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////       IOCTL_MFC_MPEG4_DEC_EXE         /////
	/////////////////////////////////////////////////
	r = ioctl(pCTX->hOpen, cmd_exe, &mfc_args);
	if ((r < 0) || (mfc_args.enc_exe.ret_code < 0)) {
		return SSBSIP_MPEG4_ENC_RET_ERR_ENCODE_FAIL;
	}

	// Encoded stream size is saved. (This value is returned in SsbSipMPEG4EncodeGetOutBuf)
	pCTX->enc_strm_size = mfc_args.enc_exe.out_encoded_size;

	if((mfc_args.enc_exe.out_header_size > 0) && (cmd_exe == IOCTL_MFC_MPEG4_ENC_EXE)) {
		pCTX->enc_hdr_size = mfc_args.enc_exe.out_header_size;
		LOG_MSG(LOG_TRACE, "SsbSipMPEG4EncodeExe", "HEADER SIZE = %d\n", pCTX->enc_hdr_size);
	}
	

	return SSBSIP_MPEG4_ENC_RET_OK;
}


int SsbSipMPEG4EncodeDeInit(void *openHandle)
{
	_MFCLIB_MPEG4_ENC  *pCTX;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeDeInit", "openHandle is NULL\n");
		return SSBSIP_MPEG4_ENC_RET_ERR_INVALID_HANDLE;
	}

	pCTX  = (_MFCLIB_MPEG4_ENC *) openHandle;

	munmap(pCTX->mapped_addr, BUF_SIZE);
	close(pCTX->hOpen);


	return SSBSIP_MPEG4_ENC_RET_OK;
}


void *SsbSipMPEG4EncodeGetInBuf(void *openHandle, long size)
{
	_MFCLIB_MPEG4_ENC	*pCTX;
	int					r;
	MFC_ARGS			mfc_args;
	

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeGetInBuf", "openHandle is NULL\n");
		return NULL;
	}
	if ((size < 0) || (size > 0x100000)) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeGetInBuf", "size is invalid. (size=%d)\n", size);
		return NULL;
	}

	pCTX  = (_MFCLIB_MPEG4_ENC *) openHandle;


	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////     IOCTL_MFC_GET_INPUT_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_FRAM_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeGetInBuf", "Failed in get FRAM_BUF address\n");
		return NULL;
	}

	return (void *)mfc_args.get_buf_addr.out_buf_addr;
}


void *SsbSipMPEG4EncodeGetOutBuf(void *openHandle, long *size)
{
	_MFCLIB_MPEG4_ENC	*pCTX;
	int					r;
	MFC_ARGS			mfc_args;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeGetOutBuf", "openHandle is NULL\n");
		return NULL;
	}
	if (size == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeGetOutBuf", "size is NULL.\n");
		return NULL;
	}

	pCTX  = (_MFCLIB_MPEG4_ENC *) openHandle;

	/////////////////////////////////////////////////
	/////           (DeviceIoControl)           /////
	/////      IOCTL_MFC_GET_STRM_BUF_ADDR      /////
	/////////////////////////////////////////////////
	mfc_args.get_buf_addr.in_usr_data = (int)pCTX->mapped_addr;
	r = ioctl(pCTX->hOpen, IOCTL_MFC_GET_LINE_BUF_ADDR, &mfc_args);
	if ((r < 0) || (mfc_args.get_buf_addr.ret_code < 0)) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4EncodeGetOutBuf", "Failed in get LINE_BUF address.\n");
		return NULL;
	}

	*size = pCTX->enc_strm_size;

	return (void *)mfc_args.get_buf_addr.out_buf_addr;
}


int SsbSipMPEG4EncodeSetConfig(void *openHandle, MPEG4_ENC_CONF conf_type, void *value)
{
	_MFCLIB_MPEG4_ENC  	*pCTX;
	MFC_ARGS			mfc_args;
	int					r;

	unsigned int		num_slices[2];
	

	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "openHandle is NULL\n");
		return SSBSIP_MPEG4_ENC_RET_ERR_INVALID_HANDLE;
	}
	if (value == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "value is NULL\n");
		return SSBSIP_MPEG4_ENC_RET_ERR_INVALID_PARAM;
	}

	pCTX  = (_MFCLIB_MPEG4_ENC *) openHandle;

	switch (conf_type) {
	case SET_H263_MULTIPLE_SLICE:
		ioctl(pCTX->hOpen, IOCTL_MFC_SET_H263_MULTIPLE_SLICE);
		break;

	case MPEG4_ENC_SETCONF_H263_NUM_SLICES:

		if (pCTX->strmType == SSBSIPMFCENC_MPEG4) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "MPEG4_ENC_SETCONF_H263_NUM_SLICES is only for H.263 encoding.\n");
			return SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL;
		}

		num_slices[0] = ((unsigned int *) value)[0];
		num_slices[1] = ((unsigned int *) value)[1];
		mfc_args.set_config.in_config_param     = MFC_SET_CONFIG_ENC_SLICE_MODE;
		mfc_args.set_config.in_config_value[0]  = num_slices[0];
		mfc_args.set_config.in_config_value[1]  = num_slices[1];

		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.set_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_ENC_SETCONF_H263_NUM_SLICES.\n");
			return SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case MPEG4_ENC_SETCONF_H263_ANNEX:

		if (pCTX->strmType == SSBSIPMFCENC_MPEG4) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "MPEG4_ENC_SETCONF_H263_ANNEX is only for H.263 encoding.\n");
			return SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL;
		}

		mfc_args.set_config.in_config_param     = MFC_SET_CONFIG_ENC_H263_PARAM;
		mfc_args.set_config.in_config_value[0]  = *((unsigned int *) value);
		mfc_args.set_config.in_config_value[1]  = 0;

		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.set_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_ENC_SETCONF_H263_ANNEX.\n");
			return SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case MPEG4_ENC_SETCONF_PARAM_CHANGE:

		mfc_args.set_config.in_config_param     = MFC_SET_CONFIG_ENC_PARAM_CHANGE;
		mfc_args.set_config.in_config_value[0]  = ((unsigned int *) value)[0];
		mfc_args.set_config.in_config_value[1]  = ((unsigned int *) value)[1];

		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.set_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_ENC_SETCONF_PARAM_CHANGE.\n");
			return SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL;
		}
		break;

	case MPEG4_ENC_SETCONF_CUR_PIC_OPT:

		mfc_args.set_config.in_config_param     = MFC_SET_CONFIG_ENC_CUR_PIC_OPT;
		mfc_args.set_config.in_config_value[0]  = ((unsigned int *) value)[0];
		mfc_args.set_config.in_config_value[1]  = ((unsigned int *) value)[1];

		r = ioctl(pCTX->hOpen, IOCTL_MFC_SET_CONFIG, &mfc_args);
		if ((r < 0) || (mfc_args.set_config.ret_code < 0)) {
			LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "Error in MPEG4_ENC_SETCONF_CUR_PIC_OPT.\n");
			return SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL;
		}
		break;

	default:
		LOG_MSG(LOG_ERROR, "SsbSipMPEG4DecodeSetConfig", "No such conf_type is supported.\n");
		return SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL;
	}

	return SSBSIP_MPEG4_ENC_RET_OK;
}




int SsbSipMPEG4EncodeGetConfig(void *openHandle, MPEG4_ENC_CONF conf_type, void *value)
{
	_MFCLIB_MPEG4_ENC  *pCTX;


	////////////////////////////////
	//  Input Parameter Checking  //
	////////////////////////////////
	if (openHandle == NULL) {
		return -1;
	}

	pCTX  = (_MFCLIB_MPEG4_ENC *) openHandle;


	switch (conf_type) {

	case MPEG4_ENC_GETCONF_HEADER_SIZE:
		*((int *)value) = pCTX->enc_hdr_size;
		break;
	
	default:
		break;
	}


	return SSBSIP_MPEG4_ENC_RET_OK;
}


