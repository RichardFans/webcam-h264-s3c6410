#ifndef __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPH264DECODE_H__
#define __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPH264DECODE_H__



typedef struct
{
	int width;
	int height;
} SSBSIP_H264_STREAM_INFO;



typedef unsigned int	H264_DEC_CONF;

#define H264_DEC_GETCONF_STREAMINFO			0x00001001
#define H264_DEC_GETCONF_PHYADDR_FRAM_BUF	0x00001002
#define H264_DEC_GETCONF_FRAM_NEED_COUNT	0x00001003

#define H264_DEC_SETCONF_POST_ROTATE		0x00002001

#ifdef __cplusplus
extern "C" {
#endif


void *SsbSipH264DecodeInit();
int   SsbSipH264DecodeExe(void *openHandle, long lengthBufFill);
int   SsbSipH264DecodeDeInit(void *openHandle);

int   SsbSipH264DecodeSetConfig(void *openHandle, H264_DEC_CONF conf_type, void *value);
int   SsbSipH264DecodeGetConfig(void *openHandle, H264_DEC_CONF conf_type, void *value);


void *SsbSipH264DecodeGetInBuf(void *openHandle, long size);
void *SsbSipH264DecodeGetOutBuf(void *openHandle, long *size);



#ifdef __cplusplus
}
#endif


// Error codes
#define SSBSIP_H264_DEC_RET_OK						(0)
#define SSBSIP_H264_DEC_RET_ERR_INVALID_HANDLE		(-1)
#define SSBSIP_H264_DEC_RET_ERR_INVALID_PARAM		(-2)

#define SSBSIP_H264_DEC_RET_ERR_CONFIG_FAIL		(-100)
#define SSBSIP_H264_DEC_RET_ERR_DECODE_FAIL		(-101)
#define SSBSIP_H264_DEC_RET_ERR_GETCONF_FAIL	(-102)
#define SSBSIP_H264_DEC_RET_ERR_SETCONF_FAIL	(-103)

#endif /* __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPH264DECODE_H__ */

