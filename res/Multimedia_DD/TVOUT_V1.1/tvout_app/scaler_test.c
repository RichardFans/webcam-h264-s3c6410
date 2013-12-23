#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/vt.h>
#include <pthread.h>
#include "../s3c-tvenc.h"
#include "../s3c-tvscaler.h"

#define TVENC_FILENAME	"/dev/s3c-tvenc"
#define SCALER_FILENAME	"/dev/s3c-tvscaler"

#define FALSE 	0
#define TRUE 	1

char key = 0;
unsigned int key_flag = FALSE;

void get_key(void)
{
	while(1) {
		key = getchar();
		key_flag = TRUE;
	}
}

/*
*
* 	
* tv_test [src_width] [src_height] [src_format] [dst_width] [dst_height] [dst_format]
* 
*
*/

int main(int argc, char **argv)
{
	scaler_params p_param;
	scaler_params *pp_param;
	int dev;
	int ret;
	char *pPreBuff;
	char *pPostBuff;
	pthread_t thread1;
	
	pp_param = &p_param;

	if(argc !=10 ) {
		fprintf(stderr, "Check Number of argument!!! \n");
		fprintf(stderr, "USAGE : \n");
		fprintf(stderr, "tv_test [src_width] [src_height] [src_format] [in_path]");
		fprintf(stderr, " [dst_width] [dst_height] [dst_format] [out_path] [mode]\n");
		fprintf(stderr, "[src/dst_format] : 0(PAL1), 1(PAL2), 2(PAL4), 3(PAL8)\n");
		fprintf(stderr, "                   4(RGB8), 5(ARGB8), 6(RGB16), 7(ARGB16)\n");
		fprintf(stderr, "                   8(RGB18), 9(RGB24), 10(RGB30), 11(ARGB24)\n");
		fprintf(stderr, "                   12(YC420), 13(YC422), 14(CRYCBY), 15(CBYCRY)\n");
		fprintf(stderr, "                   16(YCRYCB), 17(YUV444)\n");
		fprintf(stderr, "[in/out_path] : 0(DMA), 1(FIFO)\n");
		fprintf(stderr, "[mode] : 0(ONE-SHOT), 1(FREE-RUN)\n");
		exit(1);
		exit(1);
	}
	if(sscanf(argv[1], "%d", &pp_param->SrcFullWidth ) !=1 ) {
		fprintf(stderr, "Check first argument!!! \n");
		exit(1);
	}
	if(sscanf(argv[2], "%d", &pp_param->SrcFullHeight) !=1 ) {
		fprintf(stderr, "Check second argument!!! \n");
		exit(1);
	}
	
	if(sscanf(argv[3], "%d", &pp_param->SrcCSpace) !=1 ) {
		fprintf(stderr, "Check 3rd argument!!! \n");
		exit(1);
	}
	
	if(sscanf(argv[4], "%d", &pp_param->InPath) !=1 ) {
		fprintf(stderr, "Check 4th argument!!! \n");
		exit(1);
	}
	
	if(sscanf(argv[5], "%d", &pp_param->DstFullWidth) !=1 ) {
		fprintf(stderr, "Check 5th argument!!! \n");
		exit(1);
	}
	
	if(sscanf(argv[6], "%d", &pp_param->DstFullHeight) !=1 ) {
		fprintf(stderr, "Check 6th argument!!! \n");
		exit(1);
	}
	
	if(sscanf(argv[7], "%d", &pp_param->DstCSpace) !=1 ) {
		fprintf(stderr, "Check 7th argument!!! \n");
		exit(1);
	}
	
	if(sscanf(argv[8], "%d", &pp_param->OutPath) !=1 ) {
		fprintf(stderr, "Check 8th argument!!! \n");
		exit(1);
	}
	
	if(sscanf(argv[9], "%d", &pp_param->Mode) !=1 ) {
		fprintf(stderr, "Check 9th argument!!! \n");
		exit(1);
	}
	
	printf("Device file open\n");
	
	dev = open(SCALER_FILENAME, O_RDWR|O_NDELAY);
	if(dev >= 0) {
		pPreBuff = (char *) mmap(0, RESERVE_POST_MEM, \
					PROT_READ | PROT_WRITE, \
					MAP_SHARED, \
					dev, \
					0);
		if(pPreBuff == NULL) {
			printf("mmap failed\n");
			close(dev);
			exit(1);	
		}
		printf("pPreBuff = 0x%x\n", pPreBuff);
			
		pPostBuff = pPreBuff + PRE_BUFF_SIZE;
		
		printf("pPostBuff = 0x%x\n", pPostBuff);

		pp_param->SrcFrmSt = POST_BUFF_BASE_ADDR;
		pp_param->DstFrmSt = POST_BUFF_BASE_ADDR + PRE_BUFF_SIZE;				
		
		ioctl(dev, PPROC_SET_PARAMS, pp_param);

		ioctl(dev, PPROC_START, NULL);		
		
		if(pp_param->Mode == FREE_RUN) {	
			if(pthread_create(&thread1, NULL, &get_key, NULL)) {
				printf("Error while creating thread!\n");
				goto err;	
			}			
			while(key_flag == FALSE);
			ioctl(dev, PPROC_STOP, NULL);
		}

err:
		munmap(pPreBuff, RESERVE_POST_MEM);

		ret = close(dev);
		printf("ret = %08x\n", ret);
	}
	
	return 0;
}
