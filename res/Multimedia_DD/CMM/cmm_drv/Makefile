#################################################
# Makefile for JPEG Driver
# 2007 (C) Samsung Electronics 
# Author : Jiun. Yu <jiun.yu@samsung.com>
#################################################

KERNEL_DIR := /home/mobile/workspace/s3c-linux-2.6.24
TOPDIR     := /home/mobile/workspace/s3c-linux-2.6.24

obj-m		:= s3c_cmm.o 

s3c_cmm-y := LogMsg.o CMMMisc.o s3c-cmm.o 

PWD			:= $(shell pwd)

here:
	(cd $(KERNEL_DIR); make SUBDIRS=$(PWD) modules)

clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -rf Module.*
