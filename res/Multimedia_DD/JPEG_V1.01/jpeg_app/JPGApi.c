/*
 * Project Name JPEG API for HW JPEG IP IN Linux
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
 * @name JPEG DRIVER MODULE Module (JPGApi.c)
 * @author Jiun Yu (jiun.yu@samsung.com)
 * @date 17-07-07
 */
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
#include <signal.h>
#include <math.h>

#include "JPGApi.h"
#include "LogMsg.h"
//#include "../../PP_V2.5/pp_app/post_test.h"
#include "../../APPLICATIONS/Common/post.h"

//////////////////////////////////////////////////////////////////////////////////////
// Definition																		//
//////////////////////////////////////////////////////////////////////////////////////
#define JPG_DRIVER_NAME		"/dev/s3c-jpg"
#define POST_DRIVER_NAME	"/dev/s3c-pp"


#define IOCTL_JPG_DECODE				0x00000002
#define IOCTL_JPG_ENCODE				0x00000003
#define IOCTL_JPG_GET_STRBUF			0x00000004
#define IOCTL_JPG_GET_FRMBUF			0x00000005
#define IOCTL_JPG_GET_THUMB_STRBUF		0x0000000A
#define IOCTL_JPG_GET_THUMB_FRMBUF		0x0000000B
#define IOCTL_JPG_GET_PHY_FRMBUF		0x0000000C
#define IOCTL_JPG_GET_PHY_THUMB_FRMBUF	0x0000000D


#define R_RGB565(x)		(unsigned char) (((x) >> 8) & 0xF8)
#define G_RGB565(x)		(unsigned char) (((x) >> 3) & 0xFC)
#define B_RGB565(x)		(unsigned char) ((x) << 3)

#define TRUE	1
#define FALSE	0
#define DEBUG	1

//////////////////////////////////////////////////////////////////////////////////////
// Type Define																		//
//////////////////////////////////////////////////////////////////////////////////////
typedef enum tagENCDEC_TYPE_T{
	JPG_MAIN,
	JPG_THUMBNAIL
}ENCDEC_TYPE_T;

typedef struct tagJPG_DEC_PROC_PARAM{
	SAMPLE_MODE_T	sampleMode;
	ENCDEC_TYPE_T	decType;
	UINT32	width;
	UINT32	height;
	UINT32	dataSize;
	UINT32	fileSize;
} JPG_DEC_PROC_PARAM;

typedef struct tagJPG_ENC_PROC_PARAM{
	SAMPLE_MODE_T	sampleMode;
	ENCDEC_TYPE_T	encType;
	IMAGE_QUALITY_TYPE_T quality;
	UINT32	width;
	UINT32	height;
	UINT32	dataSize;
	UINT32	fileSize;
} JPG_ENC_PROC_PARAM;

typedef struct tagJPG_CTX{
	UINT	debugPrint;
	char	*InBuf;
	char	*OutBuf;
	char	*InThumbBuf;
	char	*OutThumbBuf;
	char	*mappedAddr;
	UINT8	thumbnailFlag;
	JPG_DEC_PROC_PARAM	*decParam;
	JPG_ENC_PROC_PARAM	*encParam;
	JPG_ENC_PROC_PARAM	*thumbEncParam;
	ExifFileInfo *ExifInfo;
}JPG_CTX;


UCHAR ExifHeader[6]=
{
	0x45,0x78,0x69,0x66,0x00,0x00
};

UCHAR TIFFHeader[8]=
{
	0x49,0x49,0x2A,0x00,0x08,0x00,0x00,0x00
};

//////////////////////////////////////////////////////////////////////////////////////
// function declair																	//
//////////////////////////////////////////////////////////////////////////////////////
static void initDecodeParam(void);
static void initEncodeParam(void);
static JPEG_ERRORTYPE makeThumbImage(int dev_fd);
static JPEG_ERRORTYPE MCUCheck(SAMPLE_MODE_T sampleMode, UINT32 width, UINT32 height);
static JPEG_ERRORTYPE makeExifFile(char *ExifOut, UINT *totalLen);
static void scalDownYUV422(char *srcBuf, INT32 srcWidth, INT32 srcHeight, 
							char *dstBuf, INT32 dstWidth, INT32 dstHeight);


//////////////////////////////////////////////////////////////////////////////////////
// Variables																		//
//////////////////////////////////////////////////////////////////////////////////////
JPG_CTX *jCtx;

/*----------------------------------------------------------------------------
*Function: initDecodeParam

*Parameters:
*Return Value:
*Implementation Notes: Initialize JPEG Decoder Default value
-----------------------------------------------------------------------------*/
static void initDecodeParam(void)
{
	jCtx = (JPG_CTX *)malloc(sizeof(JPG_CTX));
	memset(jCtx, 0x00, sizeof(JPG_CTX));

	jCtx->decParam = (JPG_DEC_PROC_PARAM *)malloc(sizeof(JPG_DEC_PROC_PARAM));
	memset(jCtx->decParam, 0x00, sizeof(JPG_DEC_PROC_PARAM));

	jCtx->debugPrint = TRUE;
	jCtx->decParam->decType = JPG_MAIN;
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGDecodeInit

*Parameters:
*Return Value:		Decoder Init handle
*Implementation Notes: Initialize JPEG Decoder Deivce Driver
-----------------------------------------------------------------------------*/
int SsbSipJPEGDecodeInit(void)
{
	int dev_fd;

	initDecodeParam();
	dev_fd = open(JPG_DRIVER_NAME, O_RDWR|O_NDELAY);
	if(dev_fd < 0) {
		LOG_MSG(LOG_ERROR, "SsbSipJPEGDecodeInit", "JPEG Driver open failed\n");
		return -1;
	}

	jCtx->mappedAddr = (char *) mmap(0, 
			JPG_TOTAL_BUF_SIZE, 
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			dev_fd,
			0
			);
	if (jCtx->mappedAddr == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipJPEGDecodeInit", "JPEG mmap failed\n");
		return -1;
	}

	LOG_MSG(LOG_TRACE, "SsbSipJPEGDecodeInit", "init decode handle : %d\n", dev_fd);

	return dev_fd;
}

/*----------------------------------------------------------------------------
*Function: initEncodeParam

*Parameters: 
*Return Value:	
*Implementation Notes: Initialize JPEG Encoder Default value
-----------------------------------------------------------------------------*/
static void initEncodeParam(void)
{

	jCtx = (JPG_CTX *)malloc(sizeof(JPG_CTX));
	memset(jCtx, 0x00, sizeof(JPG_CTX));

	jCtx->encParam = (JPG_ENC_PROC_PARAM *)malloc(sizeof(JPG_ENC_PROC_PARAM));
	memset(jCtx->encParam, 0x00, sizeof(JPG_ENC_PROC_PARAM));
	jCtx->encParam->fileSize = MAX_FILE_SIZE;
	
	jCtx->thumbEncParam = (JPG_ENC_PROC_PARAM *)malloc(sizeof(JPG_ENC_PROC_PARAM));
	memset(jCtx->thumbEncParam, 0x00, sizeof(JPG_ENC_PROC_PARAM));
	jCtx->thumbEncParam->dataSize = MAX_FILE_THUMB_SIZE;

	jCtx->debugPrint = TRUE;
	jCtx->encParam->sampleMode = JPG_420;
	jCtx->encParam->encType = JPG_MAIN;
	jCtx->thumbEncParam->sampleMode = JPG_420;
	jCtx->thumbEncParam->encType = JPG_THUMBNAIL;
	jCtx->thumbnailFlag = FALSE;
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGEncodeInit

*Parameters:
*Return Value:		Encoder Init handle
*Implementation Notes: Initialize JPEG Encoder Deivce Driver
-----------------------------------------------------------------------------*/
int SsbSipJPEGEncodeInit(void)
{
	int dev_fd;

	initEncodeParam();
	dev_fd = open(JPG_DRIVER_NAME, O_RDWR|O_NDELAY);
	if(dev_fd < 0) {
		LOG_MSG(LOG_ERROR, "SsbSipJPEGEncodeInit", "JPEG Driver open failed\n");
		return -1;
	}

	jCtx->mappedAddr = (char *) mmap(0, 
			JPG_TOTAL_BUF_SIZE, 
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			dev_fd,
			0
			);
	if (jCtx->mappedAddr == NULL) {
		LOG_MSG(LOG_ERROR, "SsbSipJPEGEncodeInit", "JPEG mmap failed\n");
		return -1;
	}
	
	LOG_MSG(LOG_TRACE, "SsbSipJPEGEncodeInit", "init decode handle : %d\n", dev_fd);

	return dev_fd;
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGDecodeExe

*Parameters: 		*openHandle	: openhandle from SsbSipJPEGDecodeInit
*Return Value:		JPEG_ERRORTYPE
*Implementation Notes: Decode JPEG file 
-----------------------------------------------------------------------------*/
JPEG_ERRORTYPE SsbSipJPEGDecodeExe(int dev_fd)
{
	LOG_MSG(LOG_TRACE, "SsbSipJPEGDecodeExe", "jCtx->decParam->fileSize : %d\n", jCtx->decParam->fileSize);

	if(jCtx->decParam->decType == JPG_MAIN){
		ioctl(dev_fd, IOCTL_JPG_DECODE, jCtx->decParam);

		LOG_MSG(LOG_TRACE, "SsbSipJPEGDecodeExe", "decParam->width : %d decParam->height : %d\n", jCtx->decParam->width, jCtx->decParam->height);
		LOG_MSG(LOG_TRACE, "SsbSipJPEGDecodeExe", "streamSize : %d\n", jCtx->decParam->dataSize);
	}
	else {
		// thumbnail decode, for the future work.
	}

	return JPEG_OK;
}

/*----------------------------------------------------------------------------
*Function: makeThumbImage

*Parameters: 		*openHandle	: openhandle from SsbSipJPEGEncodeInit
*Return Value:		JPEG_ERRORTYPE	
*Implementation Notes: make thumbnail image
-----------------------------------------------------------------------------*/
static JPEG_ERRORTYPE makeThumbImage(int dev_fd)
{
	int					result;


	if((result = MCUCheck(jCtx->thumbEncParam->sampleMode, jCtx->thumbEncParam->width, jCtx->thumbEncParam->height)) != JPEG_OK){
		LOG_MSG(LOG_ERROR, "makeThumbImage", "thumbnail width/height doesn't match with MCU\r\n");
		return JPEG_FAIL;
	}

	// encode thumbnail image
	ioctl(dev_fd, IOCTL_JPG_ENCODE, jCtx->thumbEncParam);

	if(jCtx->thumbEncParam->fileSize > MAX_FILE_THUMB_SIZE){
		LOG_MSG(LOG_ERROR, "makeThumbImage", "thumbnail data is too big\n");
		return JPEG_FAIL;
	}

	LOG_MSG(LOG_TRACE, "makeThumbImage", "thumbFilesize : %d\n", jCtx->thumbEncParam->fileSize);

	jCtx->OutThumbBuf = (char *)ioctl(dev_fd, IOCTL_JPG_GET_THUMB_STRBUF, jCtx->mappedAddr);

	LOG_MSG(LOG_TRACE, "makeThumbImage", "jCtx->OutThumbBuf(addr : 0x%x)\n", jCtx->OutThumbBuf);

	return JPEG_OK;
}

static JPEG_ERRORTYPE MCUCheck(SAMPLE_MODE_T sampleMode, UINT32 width, UINT32 height)
{
	if(width%16 == 0 && height%8 == 0)
		return JPEG_OK;
	else return JPEG_FAIL;
}

/*----------------------------------------------------------------------------
*Function: scalDownYUV422

*Parameters: 		*srcBuf: input yuv buffer
					srcWidth: width of input yuv
					srcHeight: height of input yuv
					*dstBuf: output yuv buffer
					dstWidth: width of output yuv
					dstHeight: height of output yuv
*Return Value:		None	
*Implementation Notes: scaling down YCBYCR format
-----------------------------------------------------------------------------*/
static void scalDownYUV422(char *srcBuf, INT32 srcWidth, INT32 srcHeight, 
				char *dstBuf, INT32 dstWidth, INT32 dstHeight)
{
	INT32	scaleX, scaleY;
	INT32	iXsrc, iXdst;
	INT32	iYsrc;

	scaleX = srcWidth/dstWidth;
	scaleY = srcHeight/dstHeight;

	iXdst = 0;
	for(iYsrc=0; iYsrc < srcHeight; iYsrc++){
		if(iYsrc % scaleY == 0){
			for(iXsrc = iYsrc*srcWidth*2; iXsrc < iYsrc*srcWidth*2+srcWidth*2;){
				if((iXsrc % (4*scaleX)) == 0){
					dstBuf[iXdst++] = srcBuf[iXsrc++];
					dstBuf[iXdst++] = srcBuf[iXsrc++];
					dstBuf[iXdst++] = srcBuf[iXsrc++];
					dstBuf[iXdst++] = srcBuf[iXsrc++];
				}else iXsrc++;
			}
		}
	}
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGEncodeExe

*Parameters: 		*openHandle : openhandle from SsbSipJPEGEncodeInit
					*Exif : Exif file parameter
*Return Value:		JPEG_ERRORTYPE
*Implementation Notes: Encode JPEG file 
-----------------------------------------------------------------------------*/
JPEG_ERRORTYPE SsbSipJPEGEncodeExe(int dev_fd, ExifFileInfo *Exif, JPEG_SCALER scaler)
{
	char 			*ExifBuf;
	UINT 			ExifLen;
	UINT 			bufSize;
	int 			ret;
	int 			pp_fd;
	pp_params 		pp_param;
	JPEG_ERRORTYPE 	result;

	
	// check MCU validation width & height & sampling mode
	if((result = MCUCheck(jCtx->encParam->sampleMode, jCtx->encParam->width, jCtx->encParam->height)) != JPEG_OK){
		LOG_MSG(LOG_ERROR, "SsbSipJPEGEncodeExe", "width/height doesn't match with MCU\r\n");
		return JPEG_FAIL;
	}

	// check validation of input datasize
	if(jCtx->encParam->width*jCtx->encParam->height*2 != jCtx->encParam->dataSize){
		LOG_MSG(LOG_ERROR, "SsbSipJPEGEncodeExe", "data size and width/height doesn't match\r\n");
		return JPEG_FAIL;
	}

	// scale down YUV data for thumbnail encoding
	if ((Exif != NULL) && (jCtx->thumbnailFlag == TRUE)){
		if((jCtx->encParam->width % jCtx->thumbEncParam->width != 0)	\
			|| (jCtx->encParam->height % jCtx->thumbEncParam->height != 0)){
			LOG_MSG(LOG_ERROR, "SsbSipJPEGEncodeExe", "Main JPEG have to be multiple of Thumbnail resolution\n");
			return FALSE;
		}
		jCtx->thumbEncParam->dataSize = jCtx->thumbEncParam->width * jCtx->thumbEncParam->height * 2;

		jCtx->InThumbBuf = (char *)ioctl(dev_fd, IOCTL_JPG_GET_THUMB_FRMBUF, jCtx->mappedAddr);

		
		// If post-processor was loaded, H/W post-processor will be used for scaling
		if(scaler == JPEG_USE_HW_SCALER) {
			LOG_MSG(LOG_TRACE, "SsbSipJPEGEncodeExe", "H/W post-processor is working\n");
			
			pp_fd = open(POST_DRIVER_NAME, O_RDWR|O_NDELAY);
			if(pp_fd < 0) {
				LOG_MSG(LOG_TRACE, "SsbSipJPEGEncodeExe", "There is no H/W post-processor. So, S/W scaler is working\n");
				scalDownYUV422(jCtx->InBuf, jCtx->encParam->width, jCtx->encParam->height, 	\
					jCtx->InThumbBuf, jCtx->thumbEncParam->width, jCtx->thumbEncParam->height);	

				goto Lable;	
			}

			pp_param.SrcFullWidth	= jCtx->encParam->width;
			pp_param.SrcFullHeight	= jCtx->encParam->height;
			pp_param.SrcStartX		= 0;
			pp_param.SrcStartY		= 0;
			pp_param.SrcWidth		= pp_param.SrcFullWidth;
			pp_param.SrcHeight		= pp_param.SrcFullHeight;
			pp_param.SrcCSpace		= YCBYCR;
			pp_param.DstStartX		= 0;
			pp_param.DstStartY		= 0;
			pp_param.DstFullWidth	= jCtx->thumbEncParam->width;
			pp_param.DstFullHeight	= jCtx->thumbEncParam->height;
			pp_param.DstWidth		= pp_param.DstFullWidth;
			pp_param.DstHeight		= pp_param.DstFullHeight;
			pp_param.DstCSpace		= YCBYCR;
			pp_param.OutPath		= 0;
			pp_param.Mode			= 0;
			pp_param.SrcFrmSt		= ioctl(dev_fd, IOCTL_JPG_GET_PHY_FRMBUF);
			pp_param.DstFrmSt		= ioctl(dev_fd, IOCTL_JPG_GET_PHY_THUMB_FRMBUF);

			ioctl(pp_fd, PPROC_SET_PARAMS, &pp_param);
			ioctl(pp_fd, PPROC_START);
		}
		else {
			LOG_MSG(LOG_TRACE, "SsbSipJPEGEncodeExe", "S/W scaler is working\n");
			
			scalDownYUV422(jCtx->InBuf, jCtx->encParam->width, jCtx->encParam->height, 	\
				jCtx->InThumbBuf, jCtx->thumbEncParam->width, jCtx->thumbEncParam->height);
		}
		

	}

Lable:
	// encode main image
	ioctl(dev_fd, IOCTL_JPG_ENCODE, jCtx->encParam);


	// if user want to make Exif file
	if(Exif){
		jCtx->ExifInfo = Exif;

		// encode thumbnail image
		if(jCtx->thumbnailFlag){
			if((ret = makeThumbImage(dev_fd)) != JPEG_OK)
				return JPEG_FAIL;
			
			bufSize = EXIF_FILE_SIZE + jCtx->thumbEncParam->fileSize;
		}else 
			bufSize = EXIF_FILE_SIZE;

		if(jCtx->encParam->fileSize + bufSize > MAX_FILE_SIZE){
			LOG_MSG(LOG_ERROR, "SsbSipJPEGEncodeExe", "Exif Error - out of buffer\n");
			return JPEG_FAIL;
		}

		ExifBuf = (char *)malloc(bufSize);
		memset(ExifBuf, 0x20, bufSize);

		// make Exif file including thumbnail image
		if((result = makeExifFile(ExifBuf, &ExifLen)) != JPEG_OK){
			free(ExifBuf);
			return JPEG_FAIL;
		}

		LOG_MSG(LOG_TRACE, "SsbSipJPEGEncodeExe", "ExifLen : %d jCtx->encParam->filesize : %d\n", ExifLen, jCtx->encParam->fileSize);
		jCtx->OutBuf = (char *)ioctl(dev_fd, IOCTL_JPG_GET_STRBUF, jCtx->mappedAddr);

		LOG_MSG(LOG_TRACE, "SsbSipJPEGEncodeExe", "jCtx->OutBuf(addr : 0x%x)\n", jCtx->OutBuf);
		
		// merge Exif file with encoded JPEG file header
		memmove(&jCtx->OutBuf[ExifLen+2], &jCtx->OutBuf[2], jCtx->encParam->fileSize - 2); 
		memcpy(&jCtx->OutBuf[2], ExifBuf, ExifLen);
		jCtx->encParam->fileSize += ExifLen;
		free(ExifBuf);
	}
	
	return JPEG_OK;
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGGetDecodeInBuf

*Parameters: 		*openHandle : openhandle from SsbSipJPEGDecodeInit
					size : input stream(YUV/RGB) size
*Return Value:		virtual address of Decoder input buffer
*Implementation Notes: allocate decoder input buffer 
-----------------------------------------------------------------------------*/
void *SsbSipJPEGGetDecodeInBuf(int dev_fd, long size)
{
	if(size < 0 || size > MAX_FILE_SIZE){
		LOG_MSG(LOG_ERROR, "SsbSipJPEGGetDecodeInBuf", "Invalid Decoder input buffer size\r\n");
		return NULL;
	}

	jCtx->decParam->fileSize = size;

	jCtx->InBuf = (char *)ioctl(dev_fd, IOCTL_JPG_GET_STRBUF, jCtx->mappedAddr);
	
	return (jCtx->InBuf);
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGGetDecodeOutBuf

*Parameters: 		*openHandle : openhandle from SsbSipJPEGDecodeInit
					size : output JPEG file size
*Return Value:		virtual address of Decoder output buffer
*Implementation Notes: return decoder output buffer 
-----------------------------------------------------------------------------*/
void *SsbSipJPEGGetDecodeOutBuf (int dev_fd, long *size)
{
	jCtx->OutBuf = (char *)ioctl(dev_fd, IOCTL_JPG_GET_FRMBUF, jCtx->mappedAddr);
	
	LOG_MSG(LOG_TRACE, "SsbSipJPEGGetDecodeOutBuf", "DecodeOutBuf : 0x%x  size : %d\n", jCtx->OutBuf, jCtx->decParam->dataSize);
	*size = jCtx->decParam->dataSize;

	return (jCtx->OutBuf);
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGGetEncodeInBuf

*Parameters: 		*openHandle : openhandle from SsbSipJPEGEncodeInit
					size : input JPEG file size
*Return Value:		virtual address of Encoder input buffer
*Implementation Notes: allocate encoder input buffer 
-----------------------------------------------------------------------------*/
void *SsbSipJPEGGetEncodeInBuf(int dev_fd, long size)
{
	if(size < 0 || size > MAX_YUV_SIZE){
		LOG_MSG(LOG_ERROR, "SsbSipJPEGGetEncodeInBuf", "Invalid Encoder input buffer size(%ld)\n", size);
		return NULL;
	}

	jCtx->encParam->dataSize = size;

	jCtx->InBuf = (char *)ioctl(dev_fd, IOCTL_JPG_GET_FRMBUF, jCtx->mappedAddr);

	LOG_MSG(LOG_TRACE, "SsbSipJPEGEncodeInBuf", "EncodeInBuf : 0x%x  size :%d\n", jCtx->InBuf, jCtx->encParam->dataSize);
	return (jCtx->InBuf);
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGGetEncodeOutBuf

*Parameters: 		*openHandle : openhandle from SsbSipJPEGEncodeInit
					*size : output stream(YUV/RGB) size
*Return Value:		virtual address of Encoder output buffer
*Implementation Notes: return encoder output buffer 
-----------------------------------------------------------------------------*/
void *SsbSipJPEGGetEncodeOutBuf (int dev_fd, long *size)
{
	*size = jCtx->encParam->fileSize;
	jCtx->OutBuf = (char *)ioctl(dev_fd, IOCTL_JPG_GET_STRBUF, jCtx->mappedAddr);

	LOG_MSG(LOG_TRACE, "SsbSipJPEGGetEncodeOutBuf", "jCtx->OutBuf(addr : 0x%x  size : %d)\n", jCtx->OutBuf, *size);
	
	return (jCtx->OutBuf);
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGGetConfig

*Parameters: 		type : config type to get
					*value : config value to get
*Return Value:		JPEG_ERRORTYPE
*Implementation Notes: return JPEG config value to user 
-----------------------------------------------------------------------------*/
JPEG_ERRORTYPE SsbSipJPEGGetConfig (JPEGConf type, INT32 *value)
{
	JPEG_ERRORTYPE result = JPEG_OK;

	switch(type){
		case JPEG_GET_DECODE_WIDTH: *value = (INT32)jCtx->decParam->width; break;
		case JPEG_GET_DECODE_HEIGHT: *value = (INT32)jCtx->decParam->height; break;
		case JPEG_GET_SAMPING_MODE: *value = (INT32)jCtx->decParam->sampleMode; break;
		default : LOG_MSG(LOG_ERROR, "SsbSipJPEGGetConfig", "Invalid Config type\n"); result = JPEG_FAIL;
	}

	return result;
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGSetConfig

*Parameters: 		type : config type to set
					*value : config value to set
*Return Value:		JPEG_ERRORTYPE
*Implementation Notes: set JPEG config value from user 
-----------------------------------------------------------------------------*/
JPEG_ERRORTYPE SsbSipJPEGSetConfig (JPEGConf type, INT32 value)
{
	JPEG_ERRORTYPE result = JPEG_OK;

	LOG_MSG(LOG_TRACE, "SsbSipJPEGSetConfig", "SsbSipJPEGSetConfig(%d : %d)\n", type, value);

	switch(type){
		case JPEG_SET_ENCODE_WIDTH: 
			{
				if(value < 0 || value > MAX_JPG_WIDTH){
					LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid width\r\n");
					result = JPEG_FAIL;
				}else
					jCtx->encParam->width = value; 
				break;
			}
		case JPEG_SET_ENCODE_HEIGHT: 
			{
				if(value < 0 || value > MAX_JPG_HEIGHT){
					LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid height\r\n");
					result = JPEG_FAIL;
				}else
					jCtx->encParam->height = value;
				break;
			}
		case JPEG_SET_ENCODE_QUALITY:
			{
				if(value < JPG_QUALITY_LEVEL_1 || value > JPG_QUALITY_LEVEL_4){
					LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid Quality value\n");
					result = JPEG_FAIL;
				}else
					jCtx->encParam->quality = value;
				break;
			}
		case JPEG_SET_ENCODE_THUMBNAIL: 
			{
				if(value != TRUE && value != FALSE){
					LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid thumbnailFlag\r\n");
					result = JPEG_FAIL;
				}else
					jCtx->thumbnailFlag = value;
				break;
			}
		case JPEG_SET_SAMPING_MODE: 
			{
				if(value != JPG_420 && value != JPG_422){
					LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid sampleMode\r\n");
					result = JPEG_FAIL;
				}else {
					jCtx->encParam->sampleMode = value;
					jCtx->thumbEncParam->sampleMode = value;
				}
				break;
			}
		case JPEG_SET_THUMBNAIL_WIDTH: 
			{
				if(value < 0 || value > MAX_JPG_THUMBNAIL_WIDTH){
					LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid thumbWidth\r\n");
					result = JPEG_FAIL;
				}else
					jCtx->thumbEncParam->width = value;
				break;
			}
		case JPEG_SET_THUMBNAIL_HEIGHT: 
			{
				if(value < 0 || value > MAX_JPG_THUMBNAIL_HEIGHT){
					LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid thumbHeight\r\n");
					result = JPEG_FAIL;
				}else
					jCtx->thumbEncParam->height = value;
				break;
			}
		//for the future work....
		//case JPEG_SET_THUMBNAIL_FORMAT: jCtx->encExtendParam->thumbImgtype = value; break;
		//case JPEG_SET_DEBUG_PRINT: jCtx->debugPrint = value; break;
		//case JPEG_SET_DECODE_MODE: jCtx->decParam->decType = value; break;
		default : LOG_MSG(LOG_ERROR, "SsbSipJPEGSetConfig", "Invalid Config type\n"); result = JPEG_FAIL;
	}

	return result;
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGDecodeDeInit

*Parameters: 		*openHandle : openhandle from SsbSipJPEGDecodeInit
*Return Value:		JPEG_ERRORTYPE
*Implementation Notes: Deinitialize JPEG Decoder Device Driver 
-----------------------------------------------------------------------------*/
JPEG_ERRORTYPE SsbSipJPEGDecodeDeInit (int dev_fd)
{
	munmap(jCtx->mappedAddr, JPG_TOTAL_BUF_SIZE);

	close(dev_fd);

	if(jCtx->decParam != NULL)
		free(jCtx->decParam);

	free(jCtx);
	
	return JPEG_OK;
}

/*----------------------------------------------------------------------------
*Function: SsbSipJPEGEncodeDeInit

*Parameters: 		*openHandle : openhandle from SsbSipJPEGEncodeInit
*Return Value:		True/False
*Implementation Notes: Deinitialize JPEG Encoder Device Driver 
-----------------------------------------------------------------------------*/
JPEG_ERRORTYPE SsbSipJPEGEncodeDeInit (int dev_fd)
{
	munmap(jCtx->mappedAddr, JPG_TOTAL_BUF_SIZE);

	close(dev_fd);

	if(jCtx->encParam != NULL)
		free(jCtx->decParam);

	if(jCtx->thumbEncParam != NULL)
		free(jCtx->thumbEncParam);

	free(jCtx);

	return JPEG_OK;
}

/*----------------------------------------------------------------------------
*Function: makeExifFile

*Parameters: 		*EncExtendInfo : Exif file information & thumbnail data
					*ExifOut : result buffer of Exif file
					*totalLen : the length of Exif file
*Return Value:
*Implementation Notes: make Exif file
-----------------------------------------------------------------------------*/
static JPEG_ERRORTYPE makeExifFile(char *ExifOut, UINT *totalLen)
{
	UCHAR *ExifInitialCount;
	UCHAR *tempExif = (UCHAR *)ExifOut;
	INT32 ExifSize;
	UINT santemp;
	UCHAR * startoftiff;
	UCHAR * IFD1OffsetAddress;
	UCHAR APP1Marker[2]=	{0xff,0xe1};
	UCHAR ExifLen[4]={0};
	UCHAR Nentries[2]={8,0};
	UCHAR SubIFDNentries[2]={18,0};
	UCHAR IFD1Nentries[2]={6,0};
	UCHAR EndOfEntry[4]={0};

	//VARIABLES FOR THE MAKE OF THE CAMERA
	UCHAR  maketag[2]={0xf,0x1};	
	UCHAR  makeformat[2]={0x2,0x0};
	UCHAR  Ncomponent[4]={32,0x0,0x0,0x0};
	char  make[32];
	UCHAR makeoffchar[4];
	UCHAR * offset;

	//VARIABLES FOR THE MODEL OF THE CAMERA
	UCHAR  modeltag[2]={0x10,0x1};	
	UCHAR  modelformat[2]={0x2,0x0};
	UCHAR  NcomponentModel[4]={32,0x0,0x0,0x0};		
	char  model[32];
	UCHAR modeloffchar[4];
	
	//VARIABLES FOR THE ORIENTATION OF THE CAMERA
	UCHAR  orientationtag[2]={0x12,0x1};	
	UCHAR  orientationformat[2]={0x3,0x0};
	UCHAR  NcomponentOrientation[4]={0x1,0x0,0x0,0x0};	
	UINT  Orientation[1];
	UCHAR  Orient[4] = {0};

		
	//VARIABLES FOR THE JPEG PROCESS
	UCHAR  Processtag[2]={0x00,0x02};	
	UCHAR  Processformat[2]={0x3,0x0};
	UCHAR  NcomponentProcess[4]={0x1,0x0,0x0,0x0};	
	UINT  Process[1];
	UCHAR  Proc[4] = {0};

	//VARIABLES FOR THE X-RESOLUTION OF THE IMAGE	
	UCHAR  XResolutiontag[2]={0x1A,0x1};	
	UCHAR  XResolutionformat[2]={0x5,0x0};
	UCHAR  NcomponentXResolution[4]={0x1,0x0,0x0,0x0};	
	UINT  XResolutionNum[1];//={0x00000048};
	UINT  XResolutionDen[1];//={0x00000001};
	
	UCHAR XResolutionoffchar[4];
	UCHAR XResolutionNumChar[4];
	UCHAR XResolutionDenChar[4];

	//VARIABLES FOR THE Y-RESOLUTION OF THE IMAGE
	UCHAR  YResolutiontag[2]={0x1B,0x1};	
	UCHAR  YResolutionformat[2]={0x5,0x0};
	UCHAR  NcomponentYResolution[4]={0x1,0x0,0x0,0x0};	
	UINT  YResolutionNum[1];//={0x00000048};
	UINT  YResolutionDen[1];//={0x00000001};
	
	UCHAR YResolutionoffchar[4];
	UCHAR YResolutionNumChar[4];
	UCHAR YResolutionDenChar[4];

	//VARIABLES FOR THE RESOLUTION UNIT OF THE CAMERA
	UCHAR  RUnittag[2]={0x28,0x1};	
	UCHAR  RUnitformat[2]={0x3,0x0};
	UCHAR  NcomponentRUnit[4]={0x1,0x0,0x0,0x0};	
	UINT  RUnit[1];
	UCHAR  RUnitChar[4] = {0};

	
	//VARIABLES FOR THE VERSION NO OF THE SOFTWARE
	UCHAR  Versiontag[2]={0x31,0x1};	
	UCHAR  Versionformat[2]={0x2,0x0};
	UCHAR  NcomponentVersion[4]={32,0x0,0x0,0x0};	
	char  Version[32];//="version 1.2";
	UCHAR Versionoffchar[4];

	//VARIABLES FOR THE DATE/TIME 
	UCHAR  DateTimetag[2]={0x32,0x1};	
	UCHAR  DateTimeformat[2]={0x2,0x0};
	UCHAR  NcomponentDateTime[4]={20,0,0,0};		
	UCHAR  DateTime[32];//="2006:6:09 15:17:32";
	char  DateTimeClose[1]={0};
	UCHAR DateTimeoffchar[4];

	//VARIABLES FOR THE COPYRIGHT
	UCHAR  CopyRighttag[2]={0x98,0x82};	
	UCHAR  CopyRightformat[2]={0x2,0x0};
	UCHAR  NcomponentCopyRight[4]={32,0x0,0x0,0x0};		
	char  CopyRight[32];
	UCHAR CopyRightoffchar[4];

	//VARIABLES FOR THE OFFSET TO SUBIFD 
	UCHAR  SubIFDOffsettag[2]={0x69,0x87};	
	UCHAR  SubIFDOffsetformat[2]={0x4,0x0};
	UCHAR  NcomponentSubIFDOffset[4]={0x1,0x0,0x0,0x0};		
	UCHAR  SubIFDOffsetChar[4] = {0};


	//VARIABLES FOR THE EXPOSURE TIME 	
	UCHAR  ExposureTimetag[2]={0x9A,0x82};	
	UCHAR  ExposureTimeformat[2]={0x5,0x0};
	UCHAR  NcomponentExposureTime[4]={0x1,0x0,0x0,0x0};	
	UINT  ExposureTimeNum[1];
	UINT  ExposureTimeDen[1];
	
	UCHAR ExposureTimeoffchar[4];
	UCHAR ExposureTimeNumChar[4];
	UCHAR ExposureTimeDenChar[4];

	//VARIABLES FOR THE FNUMBER	
	UCHAR  FNumbertag[2]={0x9D,0x82};	
	UCHAR  FNumberformat[2]={0x5,0x0};
	UCHAR  NcomponentFNumber[4]={0x1,0x0,0x0,0x0};	
	UINT  FNumberNum[1];
	UINT  FNumberDen[1];
	
	UCHAR FNumberoffchar[4];
	UCHAR FNumberNumChar[4];
	UCHAR FNumberDenChar[4];

	//VARIABLES FOR THE EXPOSURE PROGRAM OF THE CAMERA
	UCHAR  ExposureProgramtag[2]={0x22,0x88};	
	UCHAR  ExposureProgramformat[2]={0x3,0x0};
	UCHAR  NcomponentExposureProgram[4]={0x1,0x0,0x0,0x0};	
	UINT  ExposureProgram[1];
	UCHAR  ExposureProgramChar[4] = {0};

	//VARIABLES FOR THE ISO SPEED RATINGS OF THE CAMERA
	UCHAR  ISOSpeedRatingstag[2]={0x27,0x88};	
	UCHAR  ISOSpeedRatingsformat[2]={0x3,0x0};
	UCHAR  NcomponentISOSpeedRatings[4]={0x2,0x0,0x0,0x0};	
	unsigned short   ISOSpeedRatings[2];	
	UCHAR  ISOSpeedRatingsChar[4] = {0};

	//VARIABLES FOR THE BRIGHTNESS OF THE IMAGE	
	UCHAR  Brightnesstag[2]={0x03,0x92};	
	UCHAR  Brightnessformat[2]={0xA,0x0};
	UCHAR  NcomponentBrightness[4]={0x1,0x0,0x0,0x0};	
	int BrightnessNum[1];
	int BrightnessDen[1];
	
	UCHAR Brightnessoffchar[4];
	UCHAR BrightnessNumChar[4];
	UCHAR BrightnessDenChar[4];

	//VARIABLES FOR THE EXPOSURE Bias	
	UCHAR  ExposureBiastag[2]={0x04,0x92};	
	UCHAR  ExposureBiasformat[2]={0xA,0x0};
	UCHAR  NcomponentExposureBias[4]={0x1,0x0,0x0,0x0};	
	int ExposureBiasNum[1];//={-8};
	int ExposureBiasDen[1];//={1};
	
	UCHAR ExposureBiasoffchar[4];
	UCHAR ExposureBiasNumChar[4];
	UCHAR ExposureBiasDenChar[4];

	//VARIABLES FOR THE SUBJECT DISTANCE OF THE IMAGE	
	UCHAR  SubjectDistancetag[2]={0x06,0x92};	
	UCHAR  SubjectDistanceformat[2]={0xA,0x0};
	UCHAR  NcomponentSubjectDistance[4]={0x1,0x0,0x0,0x0};	
	int SubjectDistanceNum[1];
	int SubjectDistanceDen[1];
	
	UCHAR SubjectDistanceoffchar[4];
	UCHAR SubjectDistanceNumChar[4];
	UCHAR SubjectDistanceDenChar[4];

	//VARIABLES FOR THE METERING MODE
	UCHAR  MeteringModetag[2]={0x07,0x92};	
	UCHAR  MeteringModeformat[2]={0x3,0x0};
	UCHAR  NcomponentMeteringMode[4]={0x1,0x0,0x0,0x0};	
	UINT   MeteringMode[1];
	UCHAR  MeteringModeChar[4] = {0};

	//VARIABLES FOR THE FLASH
	UCHAR  Flashtag[2]={0x09,0x92};	
	UCHAR  Flashformat[2]={0x3,0x0};
	UCHAR  NcomponentFlash[4]={0x1,0x0,0x0,0x0};	
	UINT   Flash[1]={1};
	UCHAR  FlashChar[4] = {0};

	//VARIABLES FOR THE FOCAL LENGTH	
	UCHAR  FocalLengthtag[2]={0x0A,0x92};	
	UCHAR  FocalLengthformat[2]={0x5,0x0};
	UCHAR  NcomponentFocalLength[4]={0x1,0x0,0x0,0x0};	
	UINT FocalLengthNum[1];
	UINT FocalLengthDen[1];
	
	UCHAR FocalLengthoffchar[4];
	UCHAR FocalLengthNumChar[4];
	UCHAR FocalLengthDenChar[4];

    //VARIABLES FOR THE ISO WIDTH OF THE MAIN IMAGE			
	UCHAR  Widthtag[2]={0x02,0xA0};	
	UCHAR  Widthformat[2]={0x3,0x0};
	UCHAR  NcomponentWidth[4]={0x1,0x0,0x0,0x0};	
	UINT   Width[1];
	UCHAR  WidthChar[4] = {0};

	//VARIABLES FOR THE ISO HEIGHT OF THE MAIN IMAGE		
	UCHAR  Heighttag[2]={0x03,0xA0};	
	UCHAR  Heightformat[2]={0x3,0x0};
	UCHAR  NcomponentHeight[4]={0x1,0x0,0x0,0x0};	
	UINT   Height[1];
	UCHAR  HeightChar[4] = {0};

	//VARIABLES FOR THE COLORSPACE
	UCHAR  ColorSpacetag[2]={0x01,0xA0};	
	//char  ColorSpacetag[2]={0x54,0x56};
	UCHAR  ColorSpaceformat[2]={0x3,0x0};
	UCHAR  NcomponentColorSpace[4]={0x1,0x0,0x0,0x0};	
	UINT   ColorSpace[1];//={1};
	UCHAR  ColorSpaceChar[4] = {0};

	//VARIABLES FOR THE FocalPlaneXResolution	
	UCHAR  FocalPlaneXResolutiontag[2]={0x0E,0xA2};	
	UCHAR  FocalPlaneXResolutionformat[2]={0x5,0x0};
	UCHAR  NcomponentFocalPlaneXResolution[4]={0x1,0x0,0x0,0x0};	
	UINT FocalPlaneXResolutionNum[1];
	UINT FocalPlaneXResolutionDen[1];
	
	UCHAR FocalPlaneXResolutionoffchar[4];
	UCHAR FocalPlaneXResolutionNumChar[4];
	UCHAR FocalPlaneXResolutionDenChar[4];

	//VARIABLES FOR THE FocalPlaneYResolution 	
	UCHAR  FocalPlaneYResolutiontag[2]={0x0F,0xA2};	
	UCHAR  FocalPlaneYResolutionformat[2]={0x5,0x0};
	UCHAR  NcomponentFocalPlaneYResolution[4]={0x1,0x0,0x0,0x0};	
	UINT FocalPlaneYResolutionNum[1];
	UINT FocalPlaneYResolutionDen[1];
	
	UCHAR FocalPlaneYResolutionoffchar[4];
	UCHAR FocalPlaneYResolutionNumChar[4];
	UCHAR FocalPlaneYResolutionDenChar[4];

	//VARIABLES FOR THE FocalPlaneResolutionUnit
	UCHAR  FocalPlaneResolutionUnittag[2]={0x10,0xA2};	
	UCHAR  FocalPlaneResolutionUnitformat[2]={0x3,0x0};
	UCHAR  NcomponentFocalPlaneResolutionUnit[4]={0x1,0x0,0x0,0x0};	
	UINT   FocalPlaneResolutionUnit[1];
	UCHAR  FocalPlaneResolutionUnitChar[4] = {0};

	
	//VARIABLES FOR THE WHITE BALANCE PROGRAM OF THE CAMERA
	UCHAR  WhiteBalancetag[2]={0x07,0x00};	
	UCHAR  WhiteBalanceformat[2]={0x3,0x0};
	UCHAR  NcomponentWhiteBalance[4]={0x1,0x0,0x0,0x0};	
	UINT WhiteBalance[1];
	UCHAR  WhiteBalanceChar[4] = {0};

	//VARIABLES FOR THE USER COMMENTS
	UCHAR  UserCommentstag[2]={0x86,0x92};	
	UCHAR  UserCommentsformat[2]={0x7,0x0};
	UCHAR  NcomponentUserComments[4]={150,0x0,0x0,0x0};		
	UCHAR  UserComments[150];
	UCHAR UserCommentsoffchar[4];

	//VARIABLES FOR THE COMPRESSION TYPE
	UCHAR  Compressiontag[2]={0x03,0x01};	
	UCHAR  Compressionformat[2]={0x3,0x0};
	UCHAR  NcomponentCompression[4]={0x1,0x0,0x0,0x0};	
	UINT   Compression[1]={6};
	UCHAR  CompressionChar[4] = {0};

	//VARIABLES FOR THE JpegIFOffset 
	UCHAR  JpegIFOffsettag[2]={0x01,0x02};	
	UCHAR  JpegIFOffsetformat[2]={0x4,0x0};
	UCHAR  NcomponentJpegIFOffset[4]={0x1,0x0,0x0,0x0};		
	UCHAR  JpegIFOffsetChar[4] = {0};

	//VARIABLES FOR THE JpegIFByteCount 
	UCHAR  JpegIFByteCounttag[2]={0x02,0x02};	
	UCHAR  JpegIFByteCountformat[2]={0x4,0x0};
	UCHAR  NcomponentJpegIFByteCount[4]={0x1,0x0,0x0,0x0};		
	UCHAR  JpegIFByteCountChar[4] = {0};
	//END OF THE VARIABLES
	
	LOG_MSG(LOG_TRACE, "makeExifFile", "makeExif\n");
	ExifInitialCount=tempExif;
	//for APP1 Marker(2 byte) and length(2 byte)
	tempExif += 4; 
	//write an exif header
	memcpy (tempExif, ExifHeader, 6);
	tempExif += 6 ;

	//write a tiff header
	memcpy (tempExif, TIFFHeader, 8);
	startoftiff=tempExif;
	tempExif += 8 ;
	//write no of entries in 1d0
	memcpy (tempExif, Nentries, 2);
	tempExif += 2 ;
	///////////////ENTRY NO 1 :MAKE OF CAMERA////////////////////////
	//write make tag
	memcpy (tempExif, maketag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, makeformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, Ncomponent, 4);
	tempExif += 4 ;
	//write make
	//strcpy(make,tpJEInfo->Make);
	memcpy(make, jCtx->ExifInfo->Make,32);
	offset =(UCHAR *) 0x200;
	santemp=(int)(offset);
	makeoffchar[0]=(unsigned char)santemp;
	makeoffchar[1]=(unsigned char)(santemp>>8);
    makeoffchar[2]=(unsigned char)(santemp>>16);
	makeoffchar[3]=(unsigned char)(santemp>>24);
	//write the make offset into the bitstream
	memcpy (tempExif, makeoffchar, 4);
	tempExif += 4 ;
	memcpy (startoftiff+santemp, make, 32);
	offset+=32;
	
	///////////////ENTRY NO 2 :MODEL OF CAMERA////////////////////////
	//write model tag
	memcpy (tempExif, modeltag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, modelformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentModel, 4); //sanjeev
	tempExif += 4 ;
	//write model
//	strcpy(model,tpJEInfo->Model);
	memcpy(model,jCtx->ExifInfo->Model,32);
	santemp=(int)(offset);
	modeloffchar[0]=(unsigned char)santemp;
	modeloffchar[1]=(unsigned char)(santemp>>8);
    modeloffchar[2]=(unsigned char)(santemp>>16);
	modeloffchar[3]=(unsigned char)(santemp>>24);
	//write the model offset into the bitstream
	memcpy (tempExif, modeloffchar, 4);
	tempExif += 4 ;
	memcpy (startoftiff+santemp, model, 32);
	offset+=32;	


	///////////////ENTRY NO 3 :ORIENTATION OF CAMERA////////////////////////
	//write orientation tag
	memcpy (tempExif, orientationtag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, orientationformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentOrientation, 4);
	tempExif += 4 ;
	//write orientation mode
	Orientation[0] =jCtx->ExifInfo->Orientation;
	Orient[0] = (unsigned char)(Orientation[0]);
	Orient[1] = (unsigned char)(Orientation[0]>>8);
	Orient[2] = (unsigned char)(Orientation[0]>>16);
	Orient[3] = (unsigned char)(Orientation[0]>>24);

	memcpy (tempExif, Orient, 4);
	tempExif += 4 ;

		///////////////ENTRY NO 4 :JPEG PROCESS////////////////////////
	//write orientation tag
	memcpy (tempExif, Processtag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, Processformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentProcess, 4);
	tempExif += 4 ;
	//write orientation mode
	Process[0] =jCtx->ExifInfo->Process;
	Proc[0] = (unsigned char)(Process[0]);
	Proc[1] = (unsigned char)(Process[0]>>8);
	Proc[2] = (unsigned char)(Process[0]>>16);
	Proc[3] = (unsigned char)(Process[0]>>24);

	memcpy (tempExif, Proc, 4);
	tempExif += 4 ;
	
		///////////////ENTRY NO 5 :VERSION OF software////////////////////////
	//write VERSION tag
	memcpy (tempExif, Versiontag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, Versionformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentVersion, 4); //sanjeev
	tempExif += 4 ;
	
	
	santemp=(int)(offset);
	Versionoffchar[0]=(unsigned char)santemp;
	Versionoffchar[1]=(unsigned char)(santemp>>8);
    Versionoffchar[2]=(unsigned char)(santemp>>16);
	Versionoffchar[3]=(unsigned char)(santemp>>24);
	//write the VERSION offset into the bitstream
	memcpy (tempExif, Versionoffchar, 4);
	tempExif += 4 ;
//	strcpy(Version,jCtx->ExifInfo->Version);
	memcpy(Version,jCtx->ExifInfo->Version,32);
	memcpy (startoftiff+santemp, Version, 32);
	offset+=32;	
			///////////////ENTRY NO 6 :Date/Time////////////////////////
	//write model tag
	memcpy (tempExif, DateTimetag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, DateTimeformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentDateTime, 4); //sanjeev
	tempExif += 4 ;
	//write Date/Time
	//strcpy(DateTime,jCtx->ExifInfo->DateTime);
	memcpy(DateTime,jCtx->ExifInfo->DateTime,20);
	
	santemp=(int)(offset);
	DateTimeoffchar[0]=(unsigned char)santemp;
	DateTimeoffchar[1]=(unsigned char)(santemp>>8);
    DateTimeoffchar[2]=(unsigned char)(santemp>>16);
	DateTimeoffchar[3]=(unsigned char)(santemp>>24);
	//write the model offset into the bitstream
	memcpy (tempExif, DateTimeoffchar, 4);
	tempExif += 4 ;
	memcpy (startoftiff+santemp, DateTime, 19);
	memcpy (startoftiff+santemp+19, DateTimeClose, 1);

	offset+=32;	
			///////////////ENTRY NO 7 :COPYRIGHT INFO////////////////////////
	//write model tag
	memcpy (tempExif, CopyRighttag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, CopyRightformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentCopyRight, 4); //sanjeev
	tempExif += 4 ;

//	strcpy(CopyRight,jCtx->ExifInfo->CopyRight);////="copyright 2006";);
	memcpy(CopyRight,jCtx->ExifInfo->CopyRight,32);
	
	santemp=(int)(offset);
	CopyRightoffchar[0]=(unsigned char)santemp;
	CopyRightoffchar[1]=(unsigned char)(santemp>>8);
    CopyRightoffchar[2]=(unsigned char)(santemp>>16);
	CopyRightoffchar[3]=(unsigned char)(santemp>>24);
	//write the model offset into the bitstream
	memcpy (tempExif, CopyRightoffchar, 4);
	tempExif += 4 ;
	memcpy (startoftiff+santemp, CopyRight, 32);	

	offset+=32;	
	///////////////ENTRY NO 8 :OFFSET TO THE SubIFD ////////////////////////
	//write SubIFD tag
	memcpy (tempExif, SubIFDOffsettag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, SubIFDOffsetformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentSubIFDOffset, 4);
	tempExif += 4 ;
	//write the  offset to the SubIFD
	santemp=(int)(tempExif-startoftiff+8);
	SubIFDOffsetChar[0] = (unsigned char)(santemp);
	SubIFDOffsetChar[1] = (unsigned char)(santemp>>8);
	SubIFDOffsetChar[2] = (unsigned char)(santemp>>16);
	SubIFDOffsetChar[3] = (unsigned char)(santemp>>24);

	memcpy (tempExif, SubIFDOffsetChar, 4);
	tempExif += 4 ;


    // since it was the last directory entry, so next 4 bytes contains an offset to the IFD1.

    //since we dont know the offset lets put 0x0000 as an offset, later when get to know the
	//actual offset we will revisit here and put the actual offset.
	santemp=0x0000;
	SubIFDOffsetChar[0] = (unsigned char)(santemp);
	SubIFDOffsetChar[1] = (unsigned char)(santemp>>8);
	SubIFDOffsetChar[2] = (unsigned char)(santemp>>16);
	SubIFDOffsetChar[3] = (unsigned char)(santemp>>24);
	IFD1OffsetAddress = tempExif;
	memcpy (tempExif, SubIFDOffsetChar, 4);
	tempExif += 4 ;
/////////////EXIF SUBIFD STARTS HERE//////////////////////////////////
	//write no of entries in SubIFD
	memcpy (tempExif, SubIFDNentries, 2);
	tempExif += 2 ;
///////////////ENTRY NO 1 : EXPOSURE TIME////////////////////////
	//write EXPOSURE TIME tag
	memcpy (tempExif, ExposureTimetag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, ExposureTimeformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentExposureTime, 4); 
	tempExif += 4 ;
	//write EXPOSURE TIME
	
	santemp=(int)(offset);
	ExposureTimeoffchar[0]=(unsigned char)santemp;
	ExposureTimeoffchar[1]=(unsigned char)(santemp>>8);
    ExposureTimeoffchar[2]=(unsigned char)(santemp>>16);
	ExposureTimeoffchar[3]=(unsigned char)(santemp>>24);
	//write the X-Resolution offset into the bitstream
	memcpy (tempExif, ExposureTimeoffchar, 4);
	tempExif += 4 ;

	ExposureTimeNum[0]=jCtx->ExifInfo->ExposureTimeNum;
	ExposureTimeDen[0]=jCtx->ExifInfo->ExposureTimeDen;
	ExposureTimeNumChar[0]=(unsigned char)ExposureTimeNum[0];
	ExposureTimeNumChar[1]=(unsigned char)(ExposureTimeNum[0]>>8);
	ExposureTimeNumChar[2]=(unsigned char)(ExposureTimeNum[0]>>16);
	ExposureTimeNumChar[3]=(unsigned char)(ExposureTimeNum[0]>>24);

	ExposureTimeDenChar[0]=(unsigned char)ExposureTimeDen[0];
	ExposureTimeDenChar[1]=(unsigned char)(ExposureTimeDen[0]>>8);
	ExposureTimeDenChar[2]=(unsigned char)(ExposureTimeDen[0]>>16);
	ExposureTimeDenChar[3]=(unsigned char)(ExposureTimeDen[0]>>24);

	//WRITE THE EXPOSURE TIME NUMERATOR
	memcpy (startoftiff+santemp, ExposureTimeNumChar, 4);

	memcpy (startoftiff+santemp+4, ExposureTimeDenChar, 4);
	
	offset+=32;
	///////////////ENTRY NO 2 : F NUMBER////////////////////////
	//write FNumber tag
	memcpy (tempExif, FNumbertag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, FNumberformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentFNumber, 4); //sanjeev
	tempExif += 4 ;
	//write F NUMBER
	
	santemp=(int)(offset);
	FNumberoffchar[0]=(unsigned char)santemp;
	FNumberoffchar[1]=(unsigned char)(santemp>>8);
    FNumberoffchar[2]=(unsigned char)(santemp>>16);
	FNumberoffchar[3]=(unsigned char)(santemp>>24);
	//write the F NUMBER into the bitstream
	memcpy (tempExif, FNumberoffchar, 4);
	tempExif += 4 ;

	FNumberNum[0]=jCtx->ExifInfo->FNumberNum;
	FNumberDen[0]=jCtx->ExifInfo->FNumberDen;

	FNumberNumChar[0]=(unsigned char)FNumberNum[0];
	FNumberNumChar[1]=(unsigned char)(FNumberNum[0]>>8);
	FNumberNumChar[2]=(unsigned char)(FNumberNum[0]>>16);
	FNumberNumChar[3]=(unsigned char)(FNumberNum[0]>>24);

	FNumberDenChar[0]=(unsigned char)FNumberDen[0];
	FNumberDenChar[1]=(unsigned char)(FNumberDen[0]>>8);
	FNumberDenChar[2]=(unsigned char)(FNumberDen[0]>>16);
	FNumberDenChar[3]=(unsigned char)(FNumberDen[0]>>24);

	//WRITE THE FNumber NUMERATOR
	memcpy (startoftiff+santemp, FNumberNumChar, 4);

	memcpy (startoftiff+santemp+4, FNumberDenChar, 4);
	
	offset+=32;
	///////////////ENTRY NO 3 :EXPOSURE PROGRAM////////////////////////
	//write ExposureProgram tag
	memcpy (tempExif, ExposureProgramtag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, ExposureProgramformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentExposureProgram, 4);
	tempExif += 4 ;
	//write orientation mode
	ExposureProgram[0] =jCtx->ExifInfo->ExposureProgram;

	ExposureProgramChar[0] = (unsigned char)(ExposureProgram[0]);
	ExposureProgramChar[1] = (unsigned char)(ExposureProgram[0]>>8);
	ExposureProgramChar[2] = (unsigned char)(ExposureProgram[0]>>16);
	ExposureProgramChar[3] = (unsigned char)(ExposureProgram[0]>>24);

	memcpy (tempExif, ExposureProgramChar, 4);
	tempExif += 4 ;

///////////////ENTRY NO 4 :ISOSpeedRatings ////////////////////////
	//write ISOSpeedRatings  tag
	memcpy (tempExif, ISOSpeedRatingstag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, ISOSpeedRatingsformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentISOSpeedRatings, 4);
	tempExif += 4 ;
	//write orientation mode
	ISOSpeedRatings[0] = 1;
	ISOSpeedRatings[1] = 2;

	ISOSpeedRatingsChar[0] = (unsigned char)(ISOSpeedRatings[0]);
	ISOSpeedRatingsChar[1] = (unsigned char)(ISOSpeedRatings[0]>>8);
	ISOSpeedRatingsChar[2] = (unsigned char)(ISOSpeedRatings[1]);
	ISOSpeedRatingsChar[3] = (unsigned char)(ISOSpeedRatings[1]>>8);

	memcpy (tempExif, ISOSpeedRatingsChar, 4);
	tempExif += 4 ;

	
		///////////////ENTRY NO 5 : BRIGHTNESS OF THE IMAGE////////////////////////
	//write BRIGHTNESS tag
	memcpy (tempExif, Brightnesstag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, Brightnessformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentBrightness, 4); //sanjeev
	tempExif += 4 ;
	//write X - Resolution
	
	santemp=(int)(offset);
	Brightnessoffchar[0]=(unsigned char)santemp;
	Brightnessoffchar[1]=(unsigned char)(santemp>>8);
    Brightnessoffchar[2]=(unsigned char)(santemp>>16);
	Brightnessoffchar[3]=(unsigned char)(santemp>>24);
	//write the X-Resolution offset into the bitstream
	memcpy (tempExif, Brightnessoffchar, 4);
	tempExif += 4 ;

	BrightnessNum[0] = jCtx->ExifInfo->BrightnessNum;
	BrightnessDen[0] = jCtx->ExifInfo->BrightnessDen;

	BrightnessNumChar[0]=(unsigned char)BrightnessNum[0];
	BrightnessNumChar[1]=(unsigned char)(BrightnessNum[0]>>8);
	BrightnessNumChar[2]=(unsigned char)(BrightnessNum[0]>>16);
	BrightnessNumChar[3]=(unsigned char)(BrightnessNum[0]>>24);

	BrightnessDenChar[0]=(unsigned char)BrightnessDen[0];
	BrightnessDenChar[1]=(unsigned char)(BrightnessDen[0]>>8);
	BrightnessDenChar[2]=(unsigned char)(BrightnessDen[0]>>16);
	BrightnessDenChar[3]=(unsigned char)(BrightnessDen[0]>>24);

	//WRITE THE X - RESOLUTION NUMERATOR
	memcpy (startoftiff+santemp, BrightnessNumChar, 4);

	memcpy (startoftiff+santemp+4, BrightnessDenChar, 4);
	
	offset+=48;

	///////////////ENTRY NO 6 : EXPOSURE BIAS OF THE IMAGE////////////////////////
	//write BRIGHTNESS tag
	memcpy (tempExif, ExposureBiastag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, ExposureBiasformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentExposureBias, 4); //sanjeev
	tempExif += 4 ;
	//write EXPOSURE BIAS

	
	santemp=(int)(offset);
	ExposureBiasoffchar[0]=(unsigned char)santemp;
	ExposureBiasoffchar[1]=(unsigned char)(santemp>>8);
    ExposureBiasoffchar[2]=(unsigned char)(santemp>>16);
	ExposureBiasoffchar[3]=(unsigned char)(santemp>>24);
	//write the EXPOSURE BIAS offset into the bitstream
	memcpy (tempExif, ExposureBiasoffchar, 4);
	tempExif += 4 ;
	ExposureBiasNum[0]=jCtx->ExifInfo->ExposureBiasNum;
	ExposureBiasDen[0]=jCtx->ExifInfo->ExposureBiasDen;
	ExposureBiasNumChar[0]=(unsigned char)ExposureBiasNum[0];
	ExposureBiasNumChar[1]=(unsigned char)(ExposureBiasNum[0]>>8);
	ExposureBiasNumChar[2]=(unsigned char)(ExposureBiasNum[0]>>16);
	ExposureBiasNumChar[3]=(unsigned char)(ExposureBiasNum[0]>>24);

	ExposureBiasDenChar[0]=(unsigned char)ExposureBiasDen[0];
	ExposureBiasDenChar[1]=(unsigned char)(ExposureBiasDen[0]>>8);
	ExposureBiasDenChar[2]=(unsigned char)(ExposureBiasDen[0]>>16);
	ExposureBiasDenChar[3]=(unsigned char)(ExposureBiasDen[0]>>24);

	//WRITE THE EXPOSURE BIAS NUMERATOR
	memcpy (startoftiff+santemp, ExposureBiasNumChar, 4);

	memcpy (startoftiff+santemp+4, ExposureBiasDenChar, 4);
	
	offset+=48;
///////////////ENTRY NO 7 : SUBJECT DISTANCE////////////////////////
	//write SUBJECT DISTANCE tag
	memcpy (tempExif, SubjectDistancetag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, SubjectDistanceformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentSubjectDistance, 4); //sanjeev
	tempExif += 4 ;
	//write SUBJECT DISTANCE

	
	santemp=(int)(offset);
	SubjectDistanceoffchar[0]=(unsigned char)santemp;
	SubjectDistanceoffchar[1]=(unsigned char)(santemp>>8);
    SubjectDistanceoffchar[2]=(unsigned char)(santemp>>16);
	SubjectDistanceoffchar[3]=(unsigned char)(santemp>>24);
	//write the SUBJECT DISTANCE offset into the bitstream
	memcpy (tempExif, SubjectDistanceoffchar, 4);
	tempExif += 4 ;
	SubjectDistanceNum[0]=jCtx->ExifInfo->SubjectDistanceNum;
	SubjectDistanceDen[0]=jCtx->ExifInfo->SubjectDistanceDen;
	SubjectDistanceNumChar[0]=(unsigned char)SubjectDistanceNum[0];
	SubjectDistanceNumChar[1]=(unsigned char)(SubjectDistanceNum[0]>>8);
	SubjectDistanceNumChar[2]=(unsigned char)(SubjectDistanceNum[0]>>16);
	SubjectDistanceNumChar[3]=(unsigned char)(SubjectDistanceNum[0]>>24);

	SubjectDistanceDenChar[0]=(unsigned char)SubjectDistanceDen[0];
	SubjectDistanceDenChar[1]=(unsigned char)(SubjectDistanceDen[0]>>8);
	SubjectDistanceDenChar[2]=(unsigned char)(SubjectDistanceDen[0]>>16);
	SubjectDistanceDenChar[3]=(unsigned char)(SubjectDistanceDen[0]>>24);

	//WRITE THE SUBJECT DISTANCE NUMERATOR
	memcpy (startoftiff+santemp, SubjectDistanceNumChar, 4);

	memcpy (startoftiff+santemp+4, SubjectDistanceDenChar, 4);
	
	offset+=48;

	///////////////ENTRY NO 8 :METERING MODE////////////////////////
	//write METERING tag
	memcpy (tempExif, MeteringModetag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, MeteringModeformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentMeteringMode, 4);
	tempExif += 4 ;
	//write METERING mode
	MeteringMode[0] = jCtx->ExifInfo->MeteringMode;
	MeteringModeChar[0] = (unsigned char)(MeteringMode[0]);
	MeteringModeChar[1] = (unsigned char)(MeteringMode[0]>>8);
	MeteringModeChar[2] = (unsigned char)(MeteringMode[0]>>16);
	MeteringModeChar[3] = (unsigned char)(MeteringMode[0]>>24);

	memcpy (tempExif, MeteringModeChar, 4);
	tempExif += 4 ;

	///////////////ENTRY NO 9 :FLASH////////////////////////
	//write FLASH tag
	memcpy (tempExif, Flashtag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, Flashformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentFlash, 4);
	tempExif += 4 ;
	//write FLASH mode
	Flash[0]= jCtx->ExifInfo->Flash;
	FlashChar[0] = (unsigned char)(Flash[0]);
	FlashChar[1] = (unsigned char)(Flash[0]>>8);
	FlashChar[2] = (unsigned char)(Flash[0]>>16);
	FlashChar[3] = (unsigned char)(Flash[0]>>24);

	memcpy (tempExif, FlashChar, 4);
	tempExif += 4 ;

	///////////////ENTRY NO 10 : FOCAL LENGTH////////////////////////
	//write FOCAL LENGTH tag
	memcpy (tempExif, FocalLengthtag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, FocalLengthformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentFocalLength, 4); //sanjeev
	tempExif += 4 ;
	//write FOCAL LENGTH
	
	santemp=(int)(offset);
	FocalLengthoffchar[0]=(unsigned char)santemp;
	FocalLengthoffchar[1]=(unsigned char)(santemp>>8);
    FocalLengthoffchar[2]=(unsigned char)(santemp>>16);
	FocalLengthoffchar[3]=(unsigned char)(santemp>>24);
	//write the FOCAL LENGTH offset into the bitstream
	memcpy (tempExif, FocalLengthoffchar, 4);
	tempExif += 4 ;
	FocalLengthNum[0]=jCtx->ExifInfo->FocalLengthNum;
	FocalLengthDen[0]=jCtx->ExifInfo->FocalLengthDen;
	FocalLengthNumChar[0]=(unsigned char)FocalLengthNum[0];
	FocalLengthNumChar[1]=(unsigned char)(FocalLengthNum[0]>>8);
	FocalLengthNumChar[2]=(unsigned char)(FocalLengthNum[0]>>16);
	FocalLengthNumChar[3]=(unsigned char)(FocalLengthNum[0]>>24);

	FocalLengthDenChar[0]=(unsigned char)FocalLengthDen[0];
	FocalLengthDenChar[1]=(unsigned char)(FocalLengthDen[0]>>8);
	FocalLengthDenChar[2]=(unsigned char)(FocalLengthDen[0]>>16);
	FocalLengthDenChar[3]=(unsigned char)(FocalLengthDen[0]>>24);

	//WRITE THE FOCAL LENGTH NUMERATOR
	memcpy (startoftiff+santemp, FocalLengthNumChar, 4);

	memcpy (startoftiff+santemp+4, FocalLengthDenChar, 4);
	
	offset+=48;

	///////////////ENTRY NO 11 :Width////////////////////////
	//write Width tag
	memcpy (tempExif, Widthtag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, Widthformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentWidth, 4);
	tempExif += 4 ;
	//write Width
	Width[0]=jCtx->ExifInfo->Width;
	WidthChar[0] = (unsigned char)(Width[0]);
	WidthChar[1] = (unsigned char)(Width[0]>>8);
	WidthChar[2] = (unsigned char)(Width[0]>>16);
	WidthChar[3] = (unsigned char)(Width[0]>>24);

	memcpy (tempExif, WidthChar, 4);
	tempExif += 4 ;

	///////////////ENTRY NO 12 :Height////////////////////////
	//write Height tag
	memcpy (tempExif, Heighttag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, Heightformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentHeight, 4);
	tempExif += 4 ;
	//write Height 
	Height[0]=jCtx->ExifInfo->Height;
	HeightChar[0] = (unsigned char)(Height[0]);
	HeightChar[1] = (unsigned char)(Height[0]>>8);
	HeightChar[2] = (unsigned char)(Height[0]>>16);
	HeightChar[3] = (unsigned char)(Height[0]>>24);

	memcpy (tempExif, HeightChar, 4);
	tempExif += 4 ;

	///////////////ENTRY NO 13 :COLORSPACE////////////////////////
	//write ExposureProgram tag
	memcpy (tempExif, ColorSpacetag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, ColorSpaceformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentColorSpace, 4);
	tempExif += 4 ;
	//write orientation mode
	ColorSpace [0]= jCtx->ExifInfo->ColorSpace;
	ColorSpaceChar[0] = (unsigned char)(ColorSpace[0]);
	ColorSpaceChar[1] = (unsigned char)(ColorSpace[0]>>8);
	ColorSpaceChar[2] = (unsigned char)(ColorSpace[0]>>16);
	ColorSpaceChar[3] = (unsigned char)(ColorSpace[0]>>24);

	memcpy (tempExif, ColorSpaceChar, 4);
	tempExif += 4 ;
///////////////ENTRY NO 14 : FocalPlaneXResolution////////////////////////
	//write EXPOSURE TIME tag
	memcpy (tempExif, FocalPlaneXResolutiontag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, FocalPlaneXResolutionformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentFocalPlaneXResolution, 4); 
	tempExif += 4 ;
	//write EXPOSURE TIME
	
	santemp=(int)(offset);
	FocalPlaneXResolutionoffchar[0]=(unsigned char)santemp;
	FocalPlaneXResolutionoffchar[1]=(unsigned char)(santemp>>8);
    FocalPlaneXResolutionoffchar[2]=(unsigned char)(santemp>>16);
	FocalPlaneXResolutionoffchar[3]=(unsigned char)(santemp>>24);
	//write the X-Resolution offset into the bitstream
	memcpy (tempExif, FocalPlaneXResolutionoffchar, 4);
	tempExif += 4 ;
	FocalPlaneXResolutionNum[0] = jCtx->ExifInfo->FocalPlaneXResolutionNum;
	FocalPlaneXResolutionDen[0] = jCtx->ExifInfo->FocalPlaneXResolutionDen;

	FocalPlaneXResolutionNumChar[0]=(unsigned char)FocalPlaneXResolutionNum[0];
	FocalPlaneXResolutionNumChar[1]=(unsigned char)(FocalPlaneXResolutionNum[0]>>8);
	FocalPlaneXResolutionNumChar[2]=(unsigned char)(FocalPlaneXResolutionNum[0]>>16);
	FocalPlaneXResolutionNumChar[3]=(unsigned char)(FocalPlaneXResolutionNum[0]>>24);

	FocalPlaneXResolutionDenChar[0]=(unsigned char)FocalPlaneXResolutionDen[0];
	FocalPlaneXResolutionDenChar[1]=(unsigned char)(FocalPlaneXResolutionDen[0]>>8);
	FocalPlaneXResolutionDenChar[2]=(unsigned char)(FocalPlaneXResolutionDen[0]>>16);
	FocalPlaneXResolutionDenChar[3]=(unsigned char)(FocalPlaneXResolutionDen[0]>>24);

	//WRITE THE EXPOSURE TIME NUMERATOR
	memcpy (startoftiff+santemp, FocalPlaneXResolutionNumChar, 4);

	memcpy (startoftiff+santemp+4, FocalPlaneXResolutionDenChar, 4);
	
	offset+=48;

	///////////////ENTRY NO 15 : FocalPlaneYResolution////////////////////////
	//write EXPOSURE TIME tag
	memcpy (tempExif, FocalPlaneYResolutiontag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, FocalPlaneYResolutionformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentFocalPlaneYResolution, 4); //sanjeev
	tempExif += 4 ;
	//write EXPOSURE TIME
	
	santemp=(int)(offset);
	FocalPlaneYResolutionoffchar[0]=(unsigned char)santemp;
	FocalPlaneYResolutionoffchar[1]=(unsigned char)(santemp>>8);
    FocalPlaneYResolutionoffchar[2]=(unsigned char)(santemp>>16);
	FocalPlaneYResolutionoffchar[3]=(unsigned char)(santemp>>24);
	//write the X-Resolution offset into the bitstream
	memcpy (tempExif, FocalPlaneYResolutionoffchar, 4);
	tempExif += 4 ;
	FocalPlaneYResolutionNum[0] = jCtx->ExifInfo->FocalPlaneYResolutionNum;
	FocalPlaneYResolutionDen[0] = jCtx->ExifInfo->FocalPlaneYResolutionDen;

	FocalPlaneYResolutionNumChar[0]=(unsigned char)FocalPlaneYResolutionNum[0];
	FocalPlaneYResolutionNumChar[1]=(unsigned char)(FocalPlaneYResolutionNum[0]>>8);
	FocalPlaneYResolutionNumChar[2]=(unsigned char)(FocalPlaneYResolutionNum[0]>>16);
	FocalPlaneYResolutionNumChar[3]=(unsigned char)(FocalPlaneYResolutionNum[0]>>24);

	FocalPlaneYResolutionDenChar[0]=(unsigned char)FocalPlaneYResolutionDen[0];
	FocalPlaneYResolutionDenChar[1]=(unsigned char)(FocalPlaneYResolutionDen[0]>>8);
	FocalPlaneYResolutionDenChar[2]=(unsigned char)(FocalPlaneYResolutionDen[0]>>16);
	FocalPlaneYResolutionDenChar[3]=(unsigned char)(FocalPlaneYResolutionDen[0]>>24);

	//WRITE THE EXPOSURE TIME NUMERATOR
	memcpy (startoftiff+santemp, FocalPlaneYResolutionNumChar, 4);

	memcpy (startoftiff+santemp+4, FocalPlaneYResolutionDenChar, 4);
	
	offset+=48;

///////////////ENTRY NO 16 :FocalPlaneResolutionUnit////////////////////////
	//write ExposureProgram tag
	memcpy (tempExif, FocalPlaneResolutionUnittag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, FocalPlaneResolutionUnitformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentFocalPlaneResolutionUnit, 4);
	tempExif += 4 ;
	//write FocalPlaneResolutionUnit
	FocalPlaneResolutionUnit[0] = jCtx->ExifInfo->FocalPlaneResolutionUnit;
	FocalPlaneResolutionUnitChar[0] = (unsigned char)(FocalPlaneResolutionUnit[0]);
	FocalPlaneResolutionUnitChar[1] = (unsigned char)(FocalPlaneResolutionUnit[0]>>8);
	FocalPlaneResolutionUnitChar[2] = (unsigned char)(FocalPlaneResolutionUnit[0]>>16);
	FocalPlaneResolutionUnitChar[3] = (unsigned char)(FocalPlaneResolutionUnit[0]>>24);

	memcpy (tempExif, FocalPlaneResolutionUnitChar, 4);
	tempExif += 4 ;
		///////////////ENTRY NO 17 :UserComments////////////////////////
		//write model tag
	memcpy (tempExif, UserCommentstag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, UserCommentsformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentUserComments, 4); //sanjeev
	tempExif += 4 ;
	//write model
//	strcpy(model,tpJEInfo->Model);
	memcpy(UserComments,jCtx->ExifInfo->UserComments,150);
	santemp=(int)(offset);
	UserCommentsoffchar[0]=(unsigned char)santemp;
	UserCommentsoffchar[1]=(unsigned char)(santemp>>8);
    UserCommentsoffchar[2]=(unsigned char)(santemp>>16);
	UserCommentsoffchar[3]=(unsigned char)(santemp>>24);
	//write the User Comments offset into the bitstream
	memcpy (tempExif, UserCommentsoffchar, 4);
	tempExif += 4 ;
	memcpy (startoftiff+santemp, UserComments, 128);
	offset+=128;
///////////////ENTRY NO 18 :WHITE BALANCE////////////////////////
	//write WhiteBalance tag
	memcpy (tempExif, WhiteBalancetag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, WhiteBalanceformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentWhiteBalance, 4);
	tempExif += 4 ;
	//write orientation mode
	WhiteBalance[0] = jCtx->ExifInfo->WhiteBalance;
	WhiteBalanceChar[0] = (unsigned char)(WhiteBalance[0]);
	WhiteBalanceChar[1] = (unsigned char)(WhiteBalance[0]>>8);
	WhiteBalanceChar[2] = (unsigned char)(WhiteBalance[0]>>16);
	WhiteBalanceChar[3] = (unsigned char)(WhiteBalance[0]>>24);

	memcpy (tempExif, WhiteBalanceChar, 4);
	tempExif += 4 ;
	if(jCtx->thumbnailFlag)
	{
//Go back where IFD1 Offset is stored and store there the actual offset of IFD1
	santemp=(int)(tempExif - startoftiff);
	SubIFDOffsetChar[0] = (unsigned char)(santemp);
	SubIFDOffsetChar[1] = (unsigned char)(santemp>>8);
	SubIFDOffsetChar[2] = (unsigned char)(santemp>>16);
	SubIFDOffsetChar[3] = (unsigned char)(santemp>>24);	
	memcpy (IFD1OffsetAddress, SubIFDOffsetChar, 4);
	

/////////////EXIF IFD1 STARTS HERE//////////////////////////////////
	//write no of entries in SubIFD
	memcpy (tempExif, IFD1Nentries, 2);
	tempExif += 2 ;

	///////////////ENTRY NO 1 :Compression Type////////////////////////
	//write Compression tag
	memcpy (tempExif, Compressiontag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, Compressionformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentCompression, 4);
	tempExif += 4 ;
	//write orientation mode
	CompressionChar[0] = (unsigned char)(Compression[0]);
	CompressionChar[1] = (unsigned char)(Compression[0]>>8);
	CompressionChar[2] = (unsigned char)(Compression[0]>>16);
	CompressionChar[3] = (unsigned char)(Compression[0]>>24);

	memcpy (tempExif, CompressionChar, 4);
	tempExif += 4 ;

	
///////////////ENTRY NO 2 : X - RESOLUTION OF THE IMAGE////////////////////////
	//write model tag
	memcpy (tempExif, XResolutiontag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, XResolutionformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentXResolution, 4); //sanjeev
	tempExif += 4 ;
	//write X - Resolution

	
	santemp=(int)(offset);
	XResolutionoffchar[0]=(unsigned char)santemp;
	XResolutionoffchar[1]=(unsigned char)(santemp>>8);
    XResolutionoffchar[2]=(unsigned char)(santemp>>16);
	XResolutionoffchar[3]=(unsigned char)(santemp>>24);
	//write the X-Resolution offset into the bitstream
	memcpy (tempExif, XResolutionoffchar, 4);
	tempExif += 4 ;
	XResolutionNum[0] = jCtx->ExifInfo->XResolutionNum;
	XResolutionDen[0] = jCtx->ExifInfo->XResolutionDen;
	XResolutionNumChar[0]=(unsigned char)XResolutionNum[0];
	XResolutionNumChar[1]=(unsigned char)(XResolutionNum[0]>>8);
	XResolutionNumChar[2]=(unsigned char)(XResolutionNum[0]>>16);
	XResolutionNumChar[3]=(unsigned char)(XResolutionNum[0]>>24);

	XResolutionDenChar[0]=(unsigned char)XResolutionDen[0];
	XResolutionDenChar[1]=(unsigned char)(XResolutionDen[0]>>8);
	XResolutionDenChar[2]=(unsigned char)(XResolutionDen[0]>>16);
	XResolutionDenChar[3]=(unsigned char)(XResolutionDen[0]>>24);

	//WRITE THE X - RESOLUTION NUMERATOR
	memcpy (startoftiff+santemp, XResolutionNumChar, 4);

	memcpy (startoftiff+santemp+4, XResolutionDenChar, 4);
	
	offset+=48;
	///////////////ENTRY NO 3 : Y - RESOLUTION OF THE IMAGE////////////////////////
	//write model tag
	memcpy (tempExif, YResolutiontag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, YResolutionformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentYResolution, 4); //sanjeev
	tempExif += 4 ;
	//write Y - Resolution


	santemp=(int)(offset);
	YResolutionoffchar[0]=(unsigned char)santemp;
	YResolutionoffchar[1]=(unsigned char)(santemp>>8);
    YResolutionoffchar[2]=(unsigned char)(santemp>>16);
	YResolutionoffchar[3]=(unsigned char)(santemp>>24);
	//write the X-Resolution offset into the bitstream
	memcpy (tempExif, YResolutionoffchar, 4);
	tempExif += 4 ;
	YResolutionNum[0] = jCtx->ExifInfo->YResolutionNum;
	YResolutionDen[0] = jCtx->ExifInfo->YResolutionDen;
	YResolutionNumChar[0]=(unsigned char)YResolutionNum[0];
	YResolutionNumChar[1]=(unsigned char)(YResolutionNum[0]>>8);
	YResolutionNumChar[2]=(unsigned char)(YResolutionNum[0]>>8);
	YResolutionNumChar[3]=(unsigned char)(YResolutionNum[0]>>8);

	YResolutionDenChar[0]=(unsigned char)YResolutionDen[0];
	YResolutionDenChar[1]=(unsigned char)(YResolutionDen[0]>>8);
	YResolutionDenChar[2]=(unsigned char)(YResolutionDen[0]>>8);
	YResolutionDenChar[3]=(unsigned char)(YResolutionDen[0]>>8);

	//WRITE THE Y - RESOLUTION NUMERATOR
	memcpy (startoftiff+santemp, YResolutionNumChar, 4);

	memcpy (startoftiff+santemp+4, YResolutionDenChar, 4);
	
	offset+=48;
		///////////////ENTRY NO 4 :RESOLUTION UNIT ////////////////////////
	//write orientation tag
	memcpy (tempExif, RUnittag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, RUnitformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentRUnit, 4);
	tempExif += 4 ;
	//write orientation mode
	RUnit[0] = jCtx->ExifInfo->RUnit;
	RUnitChar[0] = (unsigned char)(RUnit[0]);
	RUnitChar[1] = (unsigned char)(RUnit[0]>>8);
	RUnitChar[2] = (unsigned char)(RUnit[0]>>16);
	RUnitChar[3] = (unsigned char)(RUnit[0]>>24);

	memcpy (tempExif, RUnitChar, 4);
	tempExif += 4 ;

		///////////////ENTRY NO 5 :JpegIFByteCount ////////////////////////
	//write SubIFD tag
	memcpy (tempExif, JpegIFByteCounttag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, JpegIFByteCountformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentJpegIFByteCount, 4);
	tempExif += 4 ;
	//write the  offset to the SubIFD
	santemp=jCtx->thumbEncParam->fileSize;
	JpegIFByteCountChar[0] = (unsigned char)(santemp);
	JpegIFByteCountChar[1] = (unsigned char)(santemp>>8);
	JpegIFByteCountChar[2] = (unsigned char)(santemp>>16);
	JpegIFByteCountChar[3] = (unsigned char)(santemp>>24);

	memcpy (tempExif, JpegIFByteCountChar, 4);
	tempExif += 4 ;
	///////////////ENTRY NO 6 :JpegIFOffset ////////////////////////
	//write JpegIFOffset tag
	memcpy (tempExif, JpegIFOffsettag, 2);
	tempExif += 2 ;
	//write format
	memcpy (tempExif, JpegIFOffsetformat, 2);
	tempExif += 2 ;
	//write no of component
	memcpy (tempExif, NcomponentJpegIFOffset, 4);
	tempExif += 4 ;
	//write the  offset to the SubIFD
	santemp=(int)(offset);
	JpegIFOffsetChar[0] = (unsigned char)(santemp);
	JpegIFOffsetChar[1] = (unsigned char)(santemp>>8);
	JpegIFOffsetChar[2] = (unsigned char)(santemp>>16);
	JpegIFOffsetChar[3] = (unsigned char)(santemp>>24);

	memcpy (tempExif, JpegIFOffsetChar, 4);
	tempExif += 4 ;
	//COPY  THE THUMBNAIL DATA
	memcpy (startoftiff+santemp, jCtx->OutThumbBuf, jCtx->thumbEncParam->fileSize);
}

	//////////////WRITE END OF ENTRY FLAG//////////////////
	memcpy (tempExif, EndOfEntry, 4);
	tempExif += 4 ;
	if(jCtx->thumbnailFlag)
	{
		offset+=jCtx->thumbEncParam->fileSize;
	}

	santemp=(unsigned int)(offset);

	//////////////////////ENTRIES ARE OVER/////////////////////////////////
	ExifSize = (santemp)+8;
	ExifLen[1] = (unsigned char)(ExifSize);
	ExifLen[0] = (unsigned char)(ExifSize>>8);


	if(ExifSize > EXIF_FILE_SIZE + MAX_FILE_THUMB_SIZE - 2 || ExifSize < 0){
		LOG_MSG(LOG_ERROR, "makeExifFile", "Invalid Exif size\r\n");
		tempExif = NULL;
		*totalLen = 0;
		return JPEG_FAIL;
	}

	tempExif = ExifInitialCount;
	memcpy (tempExif, APP1Marker, 2);
	tempExif += 2 ;
	memcpy (tempExif, ExifLen, 2);
	*totalLen = ExifSize + 2;
	LOG_MSG(LOG_TRACE, "makeExifFile", "totalLen : %d\n", *totalLen);
	return JPEG_OK;
}


