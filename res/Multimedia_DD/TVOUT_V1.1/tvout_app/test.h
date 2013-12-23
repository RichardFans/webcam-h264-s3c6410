#ifndef __SAMSUNG_SYSLSI_APDEV_S3C_TEST_H__
#define __SAMSUNG_SYSLSI_APDEV_S3C_TEST_H__


#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

typedef struct {
	int disp_width;
	int disp_height;
} disp_info_t;

typedef struct {
	int width;
	int height;
	int frameRateRes;
	int frameRateDiv;
	int gopNum;
	int bitrate;
} enc_info_t;

typedef struct
{
	int  width;
	int  height;
} MFC_DECODED_FRAME_INFO;

typedef struct
{
	int	width;
	int	height;
} frame_info;


#endif /* __SAMSUNG_SYSLSI_APDEV_S3C_TEST_H__ */
