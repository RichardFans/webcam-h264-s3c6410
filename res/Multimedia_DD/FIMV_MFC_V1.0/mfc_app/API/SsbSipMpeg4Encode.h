#ifndef __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPMPEG4ENCODE_H__
#define __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPMPEG4ENCODE_H__

typedef unsigned int	MPEG4_ENC_CONF;

#define MPEG4_ENC_SETCONF_H263_NUM_SLICES			0x00007001
#define MPEG4_ENC_SETCONF_H263_ANNEX				0x00007002
#define MPEG4_ENC_SETCONF_PARAM_CHANGE				0x00007010
#define MPEG4_ENC_SETCONF_CUR_PIC_OPT				0x00007011


#define MPEG4_ENC_PARAM_GOP_NUM				(0x7000A001)
#define MPEG4_ENC_PARAM_INTRA_QP			(0x7000A002)
#define MPEG4_ENC_PARAM_BITRATE				(0x7000A003)
#define MPEG4_ENC_PARAM_F_RATE				(0x7000A004)
#define MPEG4_ENC_PARAM_INTRA_REF			(0x7000A005)
#define MPEG4_ENC_PARAM_SLICE_MODE			(0x7000A006)

#define MPEG4_ENC_PIC_OPT_IDR				(0x7000B001)
#define MPEG4_ENC_PIC_OPT_SKIP				(0x7000B002)

#define SSBSIPMFCENC_MPEG4		0x3035
#define SSBSIPMFCENC_H263		0x3036


#define MPEG4_ENC_GETCONF_HEADER_SIZE		0x00001001
#define SET_H263_MULTIPLE_SLICE				0x00001002

// ENC_SEQ_263_PARA
#define ANNEX_T_OFF						(0<<0)
#define ANNEX_T_ON						(1<<0)
#define ANNEX_K_OFF						(0<<1)
#define ANNEX_K_ON						(1<<1)
#define ANNEX_J_OFF						(0<<2)
#define ANNEX_J_ON						(1<<2)
#define ANNEX_I_OFF						(0<<3)
#define ANNEX_I_ON						(1<<3)

#ifdef __cplusplus
extern "C" {
#endif


void *SsbSipMPEG4EncodeInit(int strmType, unsigned int uiWidth,     unsigned int uiHeight,
                            unsigned int uiFramerate, unsigned int uiBitrate_kbps,
                            unsigned int uiGOPNum);
int   SsbSipMPEG4EncodeExe(void *openHandle);
int   SsbSipMPEG4EncodeDeInit(void *openHandle);

int   SsbSipMPEG4EncodeSetConfig(void *openHandle, MPEG4_ENC_CONF conf_type, void *value);
int   SsbSipMPEG4EncodeGetConfig(void *openHandle, MPEG4_ENC_CONF conf_type, void *value);


void *SsbSipMPEG4EncodeGetInBuf(void *openHandle, long size);
void *SsbSipMPEG4EncodeGetOutBuf(void *openHandle, long *size);



#ifdef __cplusplus
}
#endif


// Error codes
#define SSBSIP_MPEG4_ENC_RET_OK						(0)
#define SSBSIP_MPEG4_ENC_RET_ERR_INVALID_HANDLE		(-1)
#define SSBSIP_MPEG4_ENC_RET_ERR_INVALID_PARAM		(-2)

#define SSBSIP_MPEG4_ENC_RET_ERR_CONFIG_FAIL		(-100)
#define SSBSIP_MPEG4_ENC_RET_ERR_ENCODE_FAIL		(-101)
#define SSBSIP_MPEG4_ENC_RET_ERR_GETCONF_FAIL		(-102)
#define SSBSIP_MPEG4_ENC_RET_ERR_SETCONF_FAIL		(-103)

#endif /* __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPMPEG4ENCODE_H__ */

