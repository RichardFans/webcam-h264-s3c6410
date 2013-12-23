#################################################
# Makefile for JPEG Driver
# 2007 (C) Samsung Electronics 
# Author : Jiun. Yu <jiun.yu@samsung.com>
#################################################

KERNEL_DIR := /home/mobile/workspace/s3c-linux-2.6.24
TOPDIR     := /home/mobile/workspace/s3c-linux-2.6.24

obj-m		:= s3c_jpeg.o 

s3c_jpeg-y := LogMsg.o JPGOpr.o JPGMisc.o JPGMem.o s3c-jpeg.o

PWD			:= $(shell pwd)

here:
	(cd $(KERNEL_DIR); make SUBDIRS=$(PWD) modules)

clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -rf Module.*
