/////
///   H263Frames.h
///
///   Written by Simon Chun (simon.chun@samsung.com)
///   2007/08/01
///

#ifndef __SAMSUNG_SYSLSI_AP_H263_FRAMES_H__
#define __SAMSUNG_SYSLSI_AP_H263_FRAMES_H__


typedef struct
{
	int  width, height;

} H263_CONFIG_DATA;


#define H263_CODING_TYPE_I		0	// I-frame
#define H263_CODING_TYPE_P		1	// P-frame


#ifdef __cplusplus
extern "C" {
#endif

int ExtractConfigStreamH263(void *fp, unsigned char buf[], int buf_size, H263_CONFIG_DATA *conf_data);
int NextFrameH263(void *fp, unsigned char buf[], int buf_size, unsigned int *coding_type);

#ifdef __cplusplus
}
#endif




#endif /* __SAMSUNG_SYSLSI_AP_H263_FRAMES_H__ */
