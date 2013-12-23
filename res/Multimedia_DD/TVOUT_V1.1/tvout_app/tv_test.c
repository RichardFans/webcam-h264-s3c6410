#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/vt.h>
#include <pthread.h>
#include <signal.h>

#include <linux/videodev2.h>
#include "s3c-tvenc.h"
#include "s3c-tvscaler.h"
#include "test.h"
#include "SsbSipH264Decode.h"
#include "FrameExtractor.h"
#include "H264Frames.h"
#include "SsbSipLogMsg.h"
#include "MfcDriver.h"

static unsigned char delimiter_h264[4]  = {0x00, 0x00, 0x00, 0x01};

#define MFC_NODE	"/dev/misc/s3c-mfc"
#define INPUT_BUFFER_SIZE		(204800)

static int mfc_fd, file_fd;

static int		file_size;	
static void	*handle;

#define TVOUT_NODE	"/dev/video/tvout"
static int tvout_fp = -1;

#define FALSE 	0
#define TRUE 	1

#undef QVGA
#define LANDSCAPE

#ifdef QVGA

#ifdef LANDSCAPE
#define LCD_WIDTH	320
#define LCD_HEIGHT	240
#else // Portrait
#define LCD_WIDTH	240
#define LCD_HEIGHT	320
#endif

#else
#define LCD_WIDTH	800
#define LCD_HEIGHT	480
#endif

char key = 0;
unsigned int key_flag = FALSE;
unsigned int exit_flag = FALSE;
unsigned int tvout_status = 0;

struct v4l2_capability cap;
struct v4l2_control ctrl;
struct v4l2_input source;
struct v4l2_output sink;
struct v4l2_format source_fmt;
struct v4l2_standard tvout_std;

void get_value_from_str(unsigned int* val, unsigned int limit)
{
	scanf("%d" ,val);
	fflush(stdin);
	if(*val > limit)
		printf("Input number over limit!!\n");
}

static void sig_del(void)
{
	if(tvout_fp) { 
		ioctl(tvout_fp, VIDIOC_S_TVOUT_OFF, NULL);
		close(tvout_fp);
	}
	if(file_fd) {
		SsbSipH264DecodeDeInit(handle);
		close(file_fd);
	}
	#if 0
	if(mfc_fd)
		close(mfc_fd);
	#endif	
	
	exit(0);
}

void get_key(void)
{
	unsigned int val;

	printf("\nEnter the key: b(for bright), c(contrast), h(hue), j(horizontal), v(vertical) \n");
	printf("               g(gamma), x(exit) \n");
	
	while(1) {
		if(key_flag == FALSE) {
			
			fflush(stdin);				
			key = getchar();
			fflush(stdin);
			switch(key) {
			case 'b':	// brightness control
			{
				printf("BRIGHTNESS CONTROL\n");
				printf("Enter value (0~255) : \n");
				ctrl.id = V4L2_CID_BRIGHTNESS;
				get_value_from_str(&val, 255);
				ctrl.value = val;
				printf("value= %d \n", val);
				key_flag = TRUE;			
				break;
			}

			case 'c':	// contrast
			{
				printf("CONTRAST CONTROL\n");
				printf("Enter value (0~255) : \n");
				ctrl.id = V4L2_CID_CONTRAST;
				get_value_from_str(&val, 255);
				ctrl.value = val;
				printf("value= %d \n", val);
				key_flag = TRUE;			
				break;
			}
			
			case 'h':	// Hue
			{
				printf("HUE CONTROL\n");
				printf("Enter value (0~255) : \n");
				ctrl.id = V4L2_CID_HUE;
				get_value_from_str(&val, 255);
				ctrl.value = val;
				printf("value= %d \n", val);
				key_flag = TRUE;			
				break;
			}	

			case 'j':	// center position (h)
			{
				printf("CENTER POSITION H CONTROL\n");
				printf("Enter value (0~255) : \n");
				ctrl.id = V4L2_CID_HCENTER;
				get_value_from_str(&val, 255);
				ctrl.value = val;
				printf("value= %d \n", val);
				key_flag = TRUE;			
				break;
			}

			case 'v':	// center position (v)
			{
				printf("CENTER POSITION V CONTROL\n");
				printf("Enter value (0~255) : \n");
				ctrl.id = V4L2_CID_VCENTER;
				get_value_from_str(&val, 255);
				ctrl.value = val;
				printf("value= %d \n", val);
				key_flag = TRUE;			
				break;
			}

			case 'g':	// gamma
			{
				printf("GAMMA CONTROL\n");
				printf("Enter value (0~3) : \n");
				ctrl.id = V4L2_CID_GAMMA;
				get_value_from_str(&val, 3);
				ctrl.value = val;
				printf("value= %d \n", val);
				key_flag = TRUE;			
				break;
			}
			
			case 'x':	// Exit program
			{
				printf("Exiting program \n");
				exit_flag = TRUE;
				break;
			}
			
			default:
				printf("Check INPUT key function\n");
				key_flag = FALSE;
				break;
				
			}
			fflush(stdin);	
		}
		
	}
}


/*
* tv_test [0/1] [Inputfile]
* 0: LCD/TV Dual mode, 1: LCD/TV Different image mode
* ex)./tv_test 1 test.h264
*/

int main(int argc, char **argv)
{
	unsigned char *file_addr;
	FRAMEX_CTX *pCTX;	// frame extractor context
	FRAMEX_STRM_PTR file_strm;	
		
	tv_out_params_t tv_param;
	tv_out_params_t *ptv_param;
	int ret, cmd;
	char *pPreBuff;
	pthread_t thread1;

	struct stat s;
	void	*pStrmBuf;
	int	nFrameLeng = 0;
	SSBSIP_H264_STREAM_INFO stream_info;
	unsigned int	pYUVBuf[2];	
	
	ptv_param = &tv_param;

	if (argc == 1) {
		printf("Usage : tv_test 0 or tv_test 1 <H.264 inputfilename> \n");	
		exit(1);
	}	

	if(sscanf(argv[1], "%d", &cmd ) != 1 ) {
		fprintf(stderr, "Check first argument!!! \n");
		exit(1);
	}

	if(cmd == 0) {
		
		if(argc != 2 ) {
			fprintf(stderr, "Check argument!!! \n");
			fprintf(stderr, "USAGE : \n");
			fprintf(stderr, "tv_test [cmd] [Inputfile]\n");
			fprintf(stderr, "cmd : 0(LCD/TV Dual), 1(LCD/TV different)\n");
			fprintf(stderr, "Inputfile : This option is only available\n");
			fprintf(stderr, "            when cmd is 1 \n");
			exit(1);
		}
		
	} else if(cmd == 1) {		
	
		printf("This mode works with MFC decoder\n");
		
		if(argc != 3 ) {
			fprintf(stderr, "Check argument!!! \n");
			fprintf(stderr, "USAGE : \n");
			fprintf(stderr, "tv_test [cmd] [Inputfile]\n");
			fprintf(stderr, "cmd : 0(LCD/TV Dual), 1(LCD/TV different)\n");
			fprintf(stderr, "Inputfile : This option is only available\n");
			fprintf(stderr, "            when cmd is 1 \n");
			fprintf(stderr, "            h.264 sample clip file name \n");
			exit(1);
		}

		// Input h.264 sample file open
		file_fd = open(argv[2], O_RDONLY);
		if(file_fd < 0) {
			printf("file open error \n");
			exit(1);
		}

		// get input file size
		fstat(file_fd, &s);
		file_size = s.st_size;	

		// input file memory mapping
		file_addr = (unsigned char *) mmap(0, file_size, PROT_READ, MAP_SHARED, file_fd, 0);
		if (file_addr == NULL) {
			printf("file addr mmap failed\n");
			return -1;
		}
		
		// FrameExtractor Initialization 
		pCTX = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_h264, sizeof(delimiter_h264), 1);   
		file_strm.p_start = file_strm.p_cur = (unsigned char *)file_addr;
		file_strm.p_end = (unsigned char *)(file_addr + file_size);
		FrameExtractorFirst(pCTX, &file_strm);
		
		// 1. Create new instance     	
		handle = SsbSipH264DecodeInit();
		if (handle == NULL) {
			printf("H264_Dec_Init Failed.\n");
			return -1;
		}
		
		// 2. Obtaining the Input Buffer  (SsbSipH264DecodeGetInBuf)     
		pStrmBuf = SsbSipH264DecodeGetInBuf(handle, nFrameLeng);
		if (pStrmBuf == NULL) {
			printf("SsbSipH264DecodeGetInBuf Failed.\n");
			SsbSipH264DecodeDeInit(handle);
			return -1;
		}
	
		//  H264 CONFIG stream extraction 
		nFrameLeng = ExtractConfigStreamH264(pCTX, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
	
		// 3. Configuring the instance with the config stream  (SsbSipH264DecodeExe)                            
		if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK) {
			printf("H.264 Decoder Configuration Failed.\n");
			return -1;
		}

		//   4. Get stream information   
		SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_STREAMINFO, &stream_info);
		
		printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);	
			
	} else {
		fprintf(stderr, "Check argument!!! \n");
		exit(1);
	}

	fflush(stdin);

	// ctrl + c signal handling
	if(signal(SIGINT, sig_del) == SIG_ERR) {
		printf("Signal Error\n");
	}

	printf("Device file open\n");
	
	tvout_fp = open(TVOUT_NODE, O_RDWR);
	if(tvout_fp < 0) {
		printf("peter Device file open error =%d\n", tvout_fp);
		exit(0);
	}

	/* Get capability */
	ret = ioctl(tvout_fp, VIDIOC_QUERYCAP, &cap);
	if(ret < 0) {
		printf("V4L2 APPL : ioctl(VIDIOC_QUERYCAP) failed\n");
		//exit(1);
		goto err;
	}
	printf("V4L2 APPL : Name of the interface is %s\n",cap.driver);

	if(cmd == 0) {
		/* Check the type of device driver */
		if(!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
			printf("V4L2 APPL : Check capability !!!\n");
			goto err;
		}

		/* 1. Set source path : index 0(LCD FIFO-IN), 1(MSDMA) */
		source.index = 0;
		while(1) {
			ret = ioctl(tvout_fp, VIDIOC_ENUMINPUT, &source);
			if(ret < 0) {
				printf("V4L2 APPL : ioctl(VIDIOC_ENUMINPUT) failed\n");
				//exit(1);
				goto err;
			}

			printf("V4L2 APPL : [%d]: IN channel name %s\n", source.index, source.name);

			/* Test channel.type */
			if(source.type & V4L2_INPUT_TYPE_FIFO) {
				printf("V4L2 APPL : LCD FIFO INPUT \n");
				break;
			}
			source.index++;
		}

		/* 2. Setting for source channel 0 which is channel of LCD FIFO OUT */
		ret = ioctl(tvout_fp, VIDIOC_S_INPUT, &source);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_INPUT) failed\n");
			goto err;
		}
		
		
		/* 3. Set sink path : index 0(TV-OUT/FIFO-OUT), 1(MSDMA)*/
		sink.index = 0;
		while(1) {
			ret = ioctl(tvout_fp, VIDIOC_ENUMOUTPUT, &sink);
			if(ret < 0) {
				printf("V4L2 APPL : ioctl(VIDIOC_ENUMOUTPUT) failed\n");
				//exit(1);
				goto err;
			}

			printf("V4L2 APPL : [%d]: OUT channel name %s\n", sink.index, sink.name);

			/* Test channel.type */
			if(sink.type & V4L2_OUTPUT_TYPE_ANALOG) {
				printf("V4L2 APPL : TV-OUT \n");
				break;
			}
			sink.index++;
		}

		/* 4. Setting for sink channel 0 which is channel of ANALOG TV-OUT */
		ret = ioctl(tvout_fp, VIDIOC_S_OUTPUT, &sink);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_OUTPUT) failed\n");
			goto err;
		}

		/* 5. Set source format */
		source_fmt.fmt.pix.width = LCD_WIDTH;
		source_fmt.fmt.pix.height = LCD_HEIGHT;
		source_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;		
				
		ret = ioctl(tvout_fp, VIDIOC_S_FMT, &source_fmt);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_FMT) failed\n");
			goto err;
		}

		/* 6. Set TV standard format */
		tvout_std.index = 0;

		ret = ioctl(tvout_fp, VIDIOC_ENUMSTD, &tvout_std);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_ENUMSTD) failed\n");
			goto err;
		}

		tvout_std.id = V4L2_STD_NTSC_M;
		ret = ioctl(tvout_fp, VIDIOC_S_STD, &tvout_std.id);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_STD) failed\n");
			goto err;
		}

		/* 7. Set specific ioctl - TVOUT connect type */
		ctrl.id = V4L2_CID_CONNECT_TYPE;
		ctrl.value = 0;	// COMPOSITE

		ret = ioctl(tvout_fp, VIDIOC_S_CTRL, &ctrl);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_CTRL) failed\n");
			goto err;
		}

		/* 8. Set specific ioctl - Src address */
		ctrl.id = V4L2_CID_MPEG_STREAM_PID_VIDEO;
		ctrl.value = POST_BUFF_BASE_ADDR;		// COMPOSITE

		ret = ioctl(tvout_fp, VIDIOC_S_CTRL, &ctrl);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_CTRL) failed\n");
			goto err;
		}		

		/* 9. Start TV_OUT, peter, displayed in TV */		
		ret = ioctl(tvout_fp, VIDIOC_S_TVOUT_ON, NULL);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_TVOUT_ON) failed\n");
			goto err;
		}
		tvout_status = 1;

		if(pthread_create(&thread1, NULL, &get_key, NULL)) {
			printf("Error while creating thread!\n");
			goto err;	
		}

		while(exit_flag == FALSE) {
			if(key_flag == TRUE) {
				ret = ioctl(tvout_fp, VIDIOC_S_CTRL, &ctrl);
				if(ret < 0) {
					printf("V4L2 APPL : ioctl(VIDIOC_S_CTRL) failed\n");
					goto err;
				}
				key_flag = FALSE;
			}
		}

		/* Stop TV_OUT */
		ret = ioctl(tvout_fp, VIDIOC_S_TVOUT_OFF, NULL);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_TVOUT_OFF) failed\n");
			goto err;
		}	
	} else if(cmd == 1) {
	
		/* Check the type of device driver */
		if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
			printf("V4L2 APPL : Check capability !!!\n");
			goto err;
		}

		/* Set source path : index 0(LCD FIFO-IN), 1(MSDMA)*/
		source.index = 1;
		while(1) {
			ret = ioctl(tvout_fp, VIDIOC_ENUMINPUT, &source);
			if(ret < 0) {
				printf("V4L2 APPL : ioctl(VIDIOC_ENUMINPUT) failed\n");
				//exit(1);
				goto err;
			}

			printf("V4L2 APPL : [%d]: IN channel name %s\n", source.index, source.name);

			/* Test channel.type */
			if(source.type & V4L2_INPUT_TYPE_MSDMA) {
				printf("V4L2 APPL : DMA INPUT \n");
				break;
			}
			source.index++;
		}

		/* Setting for source channel 1 which is channel of DMA */
		ret = ioctl(tvout_fp, VIDIOC_S_INPUT, &source);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_INPUT) failed\n");
			goto err;
		}
		
		/* Set sink path : index 0(TV-OUT/FIFO-OUT), 1(MSDMA)*/
		sink.index = 0;
		while(1) {
			ret = ioctl(tvout_fp, VIDIOC_ENUMOUTPUT, &sink);
			if(ret < 0) {
				printf("V4L2 APPL : ioctl(VIDIOC_ENUMOUTPUT) failed\n");
				//exit(1);
				goto err;
			}

			printf("V4L2 APPL : [%d]: OUT channel name %s\n", sink.index, sink.name);

			/* Test channel.type */
			if(sink.type & V4L2_OUTPUT_TYPE_ANALOG) {
				printf("V4L2 APPL : TV OUT \n");
				break;
			}
			sink.index++;
		}

		/* Setting for sink channel 0 which is channel of TV OUT */
		ret = ioctl(tvout_fp, VIDIOC_S_OUTPUT, &sink);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_OUTPUT) failed\n");
			goto err;
		}

		/* Set source format */
		source_fmt.fmt.pix.width = stream_info.width;
		source_fmt.fmt.pix.height = stream_info.height;
		source_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;		
		
		ret = ioctl(tvout_fp, VIDIOC_S_FMT, &source_fmt);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_FMT) failed\n");
			goto err;
		}

		/* Set TV standard format */
		tvout_std.index = 0;

		ret = ioctl(tvout_fp, VIDIOC_ENUMSTD, &tvout_std);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_ENUMSTD) failed\n");
			goto err;
		}

		tvout_std.id = V4L2_STD_NTSC_M;
		ret = ioctl(tvout_fp, VIDIOC_S_STD, &tvout_std.id);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_STD) failed\n");
			goto err;
		}

		/* Set specific ioctl - TVOUT connect type */
		ctrl.id = V4L2_CID_CONNECT_TYPE;
		ctrl.value = 0;	// COMPOSITE

		ret = ioctl(tvout_fp, VIDIOC_S_CTRL, &ctrl);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_CTRL) failed\n");
			goto err;
		}

		//fb_size = ptv_param->sp.DstFullWidth * ptv_param->sp.DstFullHeight * 2;
		pPreBuff = (char *) mmap(0, RESERVE_POST_MEM, PROT_READ | PROT_WRITE, MAP_SHARED, 	tvout_fp, 0);
		if(pPreBuff == NULL) {
			printf("TV out frame buffer mmap failed\n");
			close(tvout_fp);
			exit(1);	
		}	
		
		do {
			// 5. DECODE (SsbSipH264DecodeExe)  
			if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK)
				break;	

			// 6. Obtaining the Output Buffer (SsbSipH264DecodeGetOutBuf)    			
			SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);
	
			// Next H.264 VIDEO stream 
			nFrameLeng = NextFrameH264(pCTX, &file_strm, pStrmBuf, INPUT_BUFFER_SIZE, NULL);
			if (nFrameLeng < 4)
				break;			
			
			//ptv_param->sp.SrcFrmSt = pYUVBuf[0];
			//ptv_param->sp.DstFrmSt = POST_BUFF_BASE_ADDR + PRE_BUFF_SIZE;	
			//printf("ptv_param->sp.SrcFrmSt = 0x%x\n", pYUVBuf[0]);
			//printf("ptv_param->sp.DstFrmSt = 0x%x\n", ptv_param->sp.DstFrmSt);			

			/* 7. Set specific ioctl - MFC output physical address */
			ctrl.id = V4L2_CID_MPEG_STREAM_PID_VIDEO;
			ctrl.value = pYUVBuf[0];	// COMPOSITE

			ret = ioctl(tvout_fp, VIDIOC_S_CTRL, &ctrl);
			if(ret < 0) {
				printf("V4L2 APPL : ioctl(VIDIOC_S_CTRL) failed\n");
				goto err;
			}
			usleep(25000);	// 25ms delay for natural video display
			
			/* Start TV_OUT */			
			if (!tvout_status) {	// peter added				
				ret = ioctl(tvout_fp, VIDIOC_S_TVOUT_ON, NULL);	// Just once executed because of the free-run mode		
				if(ret < 0) {
					printf("V4L2 APPL : ioctl(VIDIOC_S_TVOUT_ON) failed\n");
					goto err;
				}
				tvout_status = 1;
			}

		} while( nFrameLeng > 0 );
	
		/* Stop TV_OUT */
		ret = ioctl(tvout_fp, VIDIOC_S_TVOUT_OFF, NULL);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_TVOUT_OFF) failed\n");
			goto err;
		}		

		SsbSipH264DecodeDeInit(handle);
		
		if(file_addr)
			munmap(file_addr, file_size);		
		if(file_fd)
			close(file_fd);
	} else {
		printf("V4L2 APPL : Check cmd argument \n");
		goto err;
	}
	
err:
	if(tvout_status == 1) {
		/* Stop TV_OUT */
		ret = ioctl(tvout_fp, VIDIOC_S_TVOUT_OFF, NULL);
		if(ret < 0) {
			printf("V4L2 APPL : ioctl(VIDIOC_S_TVOUT_OFF) failed\n");
		}	
	}
	munmap(pPreBuff, RESERVE_POST_MEM);

	ret = close(tvout_fp);
	printf("ret = %08x\n", ret);
	
	return 0;
}
