/////
///   VC1Frames.h
///
///   Written by Simon Chun (simon.chun@samsung.com)
///   2007/06/18
///

#ifndef __SAMSUNG_SYSLSI_AP_VC1_FRAMES_H__
#define __SAMSUNG_SYSLSI_AP_VC1_FRAMES_H__

typedef struct
{
	int  width, height;

} VC1_CONFIG_DATA;

#ifdef __cplusplus
extern "C" {
#endif

int ExtractConfigStreamVC1(FILE *fp, unsigned char buf[], int buf_size, VC1_CONFIG_DATA *conf_data);
int NextFrameVC1(FILE *fp, unsigned char buf[], int buf_size, unsigned int *coding_type);

#ifdef __cplusplus
}
#endif




#endif /* __SAMSUNG_SYSLSI_AP_VC1_FRAMES_H__ */
