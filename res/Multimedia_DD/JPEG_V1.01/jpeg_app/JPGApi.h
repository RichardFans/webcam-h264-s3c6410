/*
 * Project Name JPEG API for HW JPEG IP IN WINCE
 * Copyright  2007 Samsung Electronics Co, Ltd. All Rights Reserved. 
 *
 * This software is the confidential and proprietary information
 * of Samsung Electronics  ("Confidential Information").   
 * you shall not disclose such Confidential Information and shall use
 * it only in accordance with the terms of the license agreement
 * you entered into with Samsung Electronics 
 *
 * This file implements JPEG driver.
 *
 * @name JPEG DRIVER MODULE Module (JPGApi.h)
 * @author Jiyoung Shin (idon.shin@samsung.com)
 * @date 28-05-07
 */
#ifndef __JPG_API_H__
#define __JPG_API_H__
 

#define MAX_JPG_WIDTH				1600
#define MAX_JPG_HEIGHT				1200
#define MAX_YUV_SIZE				(MAX_JPG_WIDTH * MAX_JPG_HEIGHT * 3)
#define	MAX_FILE_SIZE				(MAX_JPG_WIDTH * MAX_JPG_HEIGHT)
#define MAX_JPG_THUMBNAIL_WIDTH		320
#define MAX_JPG_THUMBNAIL_HEIGHT	240
#define MAX_YUV_THUMB_SIZE			(MAX_JPG_THUMBNAIL_WIDTH * MAX_JPG_THUMBNAIL_HEIGHT * 3)
#define	MAX_FILE_THUMB_SIZE			(MAX_JPG_THUMBNAIL_WIDTH * MAX_JPG_THUMBNAIL_HEIGHT)
#define EXIF_FILE_SIZE				28800

#define JPG_STREAM_BUF_SIZE			(MAX_JPG_WIDTH * MAX_JPG_HEIGHT)
#define JPG_STREAM_THUMB_BUF_SIZE	(MAX_JPG_THUMBNAIL_WIDTH * MAX_JPG_THUMBNAIL_HEIGHT)
#define JPG_FRAME_BUF_SIZE			(MAX_JPG_WIDTH * MAX_JPG_HEIGHT * 3)
#define JPG_FRAME_THUMB_BUF_SIZE	(MAX_JPG_THUMBNAIL_WIDTH * MAX_JPG_THUMBNAIL_HEIGHT * 3)

#define JPG_TOTAL_BUF_SIZE			(JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE \
									+ JPG_FRAME_BUF_SIZE + JPG_FRAME_THUMB_BUF_SIZE)

typedef	unsigned char	UCHAR;
typedef unsigned long	ULONG;
typedef	unsigned int	UINT;
typedef unsigned long	DWORD;
typedef unsigned int	UINT32;
typedef int				INT32;
typedef unsigned char	UINT8;
typedef enum {FALSE, TRUE} BOOL;


typedef enum tagSAMPLE_MODE_T{
	JPG_444,
	JPG_422,
	JPG_420, 
	JPG_411,
	JPG_400,
	JPG_SAMPLE_UNKNOWN
}SAMPLE_MODE_T;

typedef enum tagIMAGE_QUALITY_TYPE_T{
	JPG_QUALITY_LEVEL_1 = 0, /*high quality*/
	JPG_QUALITY_LEVEL_2,
	JPG_QUALITY_LEVEL_3,
	JPG_QUALITY_LEVEL_4     /*low quality*/
}IMAGE_QUALITY_TYPE_T;

typedef enum tagJPEGConf{
	JPEG_GET_DECODE_WIDTH,
	JPEG_GET_DECODE_HEIGHT,
	JPEG_GET_SAMPING_MODE,
	JPEG_SET_ENCODE_WIDTH,
	JPEG_SET_ENCODE_HEIGHT,
	JPEG_SET_ENCODE_QUALITY,
	JPEG_SET_ENCODE_THUMBNAIL,
	JPEG_SET_SAMPING_MODE,
	JPEG_SET_THUMBNAIL_WIDTH,
	JPEG_SET_THUMBNAIL_HEIGHT
}JPEGConf;

typedef enum tagJPEG_ERRORTYPE{
	JPEG_FAIL,
	JPEG_OK,
	JPEG_ENCODE_FAIL,
	JPEG_ENCODE_OK,
	JPEG_DECODE_FAIL,
	JPEG_DECODE_OK,
	JPEG_HEADER_PARSE_FAIL,
	JPEG_HEADER_PARSE_OK,
	JPEG_UNKNOWN_ERROR
}JPEG_ERRORTYPE;

typedef enum tagJPEG_SCALER{
	JPEG_USE_HW_SCALER=1,
	JPEG_USE_SW_SCALER=2
}JPEG_SCALER;

typedef struct tagExifFileInfo{
	char	Make[32]; 
	char	Model[32]; 
	char	Version[32]; 
	char	DateTime[32]; 
	char	CopyRight[32]; 

	UINT	Height; 
	UINT	Width;
	UINT	Orientation; 
	UINT	ColorSpace; 
	UINT	Process;
	UINT	Flash; 

	UINT	FocalLengthNum; 
	UINT	FocalLengthDen; 

	UINT	ExposureTimeNum; 
	UINT	ExposureTimeDen; 

	UINT	FNumberNum; 
	UINT	FNumberDen; 

	UINT	ApertureFNumber; 

	int		SubjectDistanceNum; 
	int		SubjectDistanceDen; 

	UINT	CCDWidth;

	int		ExposureBiasNum; 
	int		ExposureBiasDen; 


	int		WhiteBalance; 

	UINT	MeteringMode; 

	int		ExposureProgram;

	UINT	ISOSpeedRatings[2]; 
	
	UINT	FocalPlaneXResolutionNum;
	UINT	FocalPlaneXResolutionDen;

	UINT	FocalPlaneYResolutionNum;
	UINT	FocalPlaneYResolutionDen;

	UINT	FocalPlaneResolutionUnit;

	UINT	XResolutionNum;
	UINT	XResolutionDen;
	UINT	YResolutionNum;
	UINT	YResolutionDen;
	UINT	RUnit; 

	int		BrightnessNum; 
	int		BrightnessDen; 

	char	UserComments[150];
}ExifFileInfo;


#ifdef __cplusplus
extern "C" {
#endif


int SsbSipJPEGDecodeInit(void);
int SsbSipJPEGEncodeInit(void);
JPEG_ERRORTYPE SsbSipJPEGDecodeExe(int dev_fd);
JPEG_ERRORTYPE SsbSipJPEGEncodeExe(int dev_fd, ExifFileInfo *Exif, JPEG_SCALER scaler);
void *SsbSipJPEGGetDecodeInBuf(int dev_fd, long size);
void *SsbSipJPEGGetDecodeOutBuf (int dev_fd, long *size);
void *SsbSipJPEGGetEncodeInBuf(int dev_fd, long size);
void *SsbSipJPEGGetEncodeOutBuf (int dev_fd, long *size);
JPEG_ERRORTYPE SsbSipJPEGSetConfig (JPEGConf type, INT32 value);
JPEG_ERRORTYPE SsbSipJPEGGetConfig (JPEGConf type, INT32 *value);
JPEG_ERRORTYPE SsbSipJPEGDecodeDeInit (int dev_fd);
JPEG_ERRORTYPE SsbSipJPEGEncodeDeInit (int dev_fd);


#ifdef __cplusplus
}
#endif

#endif

