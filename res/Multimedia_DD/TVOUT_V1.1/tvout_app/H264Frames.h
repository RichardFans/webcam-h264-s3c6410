/////
///   H264Frames.h
///
///   Written by Simon Chun (simon.chun@samsung.com)
///   2007/03/13
///

#ifndef __SAMSUNG_SYSLSI_AP_H264_FRAMES_H__
#define __SAMSUNG_SYSLSI_AP_H264_FRAMES_H__


typedef struct
{
	int  width, height;

} H264_CONFIG_DATA;


#define H264_CODING_TYPE_I		7	// I-slice
#define H264_CODING_TYPE_P		5	// P-slice


#ifdef __cplusplus
extern "C" {
#endif

int ExtractConfigStreamH264(FRAMEX_CTX  *pFrameExCtx, void *fp, unsigned char buf[], int buf_size, H264_CONFIG_DATA *conf_data);
int NextFrameH264(FRAMEX_CTX  *pFrameExCtx, void *fp, unsigned char buf[], int buf_size, unsigned int *coding_type);

#ifdef __cplusplus
}
#endif




#endif /* __SAMSUNG_SYSLSI_AP_H264_FRAMES_H__ */
