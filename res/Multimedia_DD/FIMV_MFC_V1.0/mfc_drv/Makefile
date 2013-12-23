#################################################
# Makefile for MFC Driver
# 2007 (C) Samsung Electronics 
# Author : Jiun. Yu <jiun.yu@samsung.com>
#################################################

KERNEL_DIR := /home/mobile/workspace/s3c-linux-2.6.24
TOPDIR     := /home/mobile/workspace/s3c-linux-2.6.24

CFLAGS_MODULE += -DLINUX
CFLAGS_MODULE += -DDIVX_ENABLE

obj-m		:= s3c_mfc.o 

s3c_mfc-y := Prism_0503.o BitProcBuf.o DataBuf.o FramBufMgr.o \
			 LogMsg.o MFC_HW_Init.o MFC_Inst_Pool.o MFC_Instance.o MfcMemory.o MfcMutex.o MfcSfr.o 	\
			 s3c-mfc.o MfcIntrNotification.o MfcSetConfig.o

PWD			:= $(shell pwd)

here:
	(cd $(KERNEL_DIR); make SUBDIRS=$(PWD) modules)

clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -rf Module.*
