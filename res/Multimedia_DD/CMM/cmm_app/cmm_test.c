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
#include <pthread.h>

#include "../cmm_drv/s3c-cmm.h"
#define CMM_DRIVER_NAME		"/dev/misc/s3c-cmm"


int main()
{
	int 			cmm_fd;
	int				ret;
	unsigned char	*cached_addr, *non_cached_addr;

	CODEC_MEM_ALLOC_ARG		codec_mem_alloc_arg;
	CODEC_MEM_FREE_ARG		codec_mem_free_arg;
	CODEC_CACHE_FLUSH_ARG	codec_cache_flush_arg;
	CODEC_GET_PHY_ADDR_ARG	codec_get_phy_addr_arg;


	/* Open CMM(Codec Memory Management) driver which supports multi-instances*/
	cmm_fd = open(CMM_DRIVER_NAME, O_RDWR | O_NDELAY);
	if(cmm_fd < 0) {
		printf("CMM driver open error\n");
		return -1;
	}


	/* Mapping cacheable memory area */
	cached_addr = (unsigned char *)mmap(0, CODEC_CACHED_MEM_SIZE, 			\
		PROT_READ | PROT_WRITE, MAP_SHARED, cmm_fd, 0);
	if(cached_addr == NULL) {
		printf("CMM driver buffer mapping failed\n");
		return -1;
	}


	/* Mapping non-cacheable memory area */
	non_cached_addr = (unsigned char *)mmap(0, CODEC_NON_CACHED_MEM_SIZE, 	\
		PROT_READ | PROT_WRITE, MAP_SHARED, cmm_fd, CODEC_CACHED_MEM_SIZE);
	if(non_cached_addr == NULL) {
		printf("CMM driver buffer mapping failed\n");
		return -1;
	}
	printf("[USER] Cached address : 0x%X, Non-cached address : 0x%X\n", 	\
		(unsigned int)cached_addr, (unsigned int)non_cached_addr);


	/* Request memory allocation */
	codec_mem_alloc_arg.buffSize = 1*1024;
	codec_mem_alloc_arg.cached_mapped_addr = (unsigned int)cached_addr;
	codec_mem_alloc_arg.non_cached_mapped_addr = (unsigned int)non_cached_addr;
	codec_mem_alloc_arg.cacheFlag = 1;	/* 1: cacheable, 0: non-cacheable */
	ret = ioctl(cmm_fd, IOCTL_CODEC_MEM_ALLOC, &codec_mem_alloc_arg);
	if (ret == 0) {
		printf("Memory allocation failed, ret = %d\n", ret);
		return -1;
	}
	printf("Allocated memory address : 0x%X, size : %d, cached flag : %d\n", 	\
		codec_mem_alloc_arg.out_addr, codec_mem_alloc_arg.buffSize, codec_mem_alloc_arg.cacheFlag);


	/* Clean the cacheable memory area */
	codec_cache_flush_arg.u_addr = codec_mem_alloc_arg.out_addr;
	codec_cache_flush_arg.size = codec_mem_alloc_arg.buffSize;
	ioctl(cmm_fd, IOCTL_CODEC_CACHE_FLUSH, &codec_cache_flush_arg);

	/* Get physical address */
	codec_get_phy_addr_arg.u_addr = codec_mem_alloc_arg.out_addr;
	ioctl(cmm_fd, IOCTL_CODEC_GET_PHY_ADDR, &codec_get_phy_addr_arg);
	printf("User address : 0x%X, --> Physical address : 0x%X\n", 	\
		codec_get_phy_addr_arg.u_addr, codec_get_phy_addr_arg.p_addr);


	/* Free memory */
	codec_mem_free_arg.u_addr = codec_mem_alloc_arg.out_addr;
	ret = ioctl(cmm_fd, IOCTL_CODEC_MEM_FREE, &codec_mem_free_arg);


	close(cmm_fd);

	return 0;
}
