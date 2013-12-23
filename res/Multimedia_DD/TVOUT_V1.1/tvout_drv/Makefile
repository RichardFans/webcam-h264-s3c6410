#################################################
# Makefile for TV-OUT on S3C6410
# 2008 (C) Samsung Electronics
# Author : peter. Oh <jaeryul.oh@samsung.com>
#################################################

# where the kernel sources are located
##KERNEL_DIR := /home/jaeryul/eldk_samba/kernel/s3c-linux-2[1].6.24_rel-2-1-0_20080820
KERNEL_DIR := /home/jaeryul/eldk_samba/kernel/s3c-linux-2[1].6.24_rel-2-1-2_080829_mod
##KERNEL_DIR := /home/jaeryul/eldk_samba/kernel/s3c-linux-2.6.24_fromjiun

obj-m   += s3c-tvscaler.o
obj-m	+= s3c-tvenc.o

here:
	(cd $(KERNEL_DIR); make SUBDIRS=$(PWD) modules) 
	cp s3c-tvenc.ko /home/jaeryul/eldk_nfs/GTK-DFB/peter/tvout_test/s3c-tvenv_for2.6.24_2-1-2.ko
	cp s3c-tvscaler.ko /home/jaeryul/eldk_nfs/GTK-DFB/peter/tvout_test/s3c-tvscaler_for2.6.24_2-1.2.ko
	##cp s3c-tvenc.ko /home/jaeryul/eldk_nfs/cramfs/peter/driver/s3c-tvenv_for2.6.24_2-1-2.ko
	##cp s3c-tvscaler.ko /home/jaeryul/eldk_nfs/cramfs/peter/driver/s3c-tvscaler_for2.6.24_2-1.2.ko

clean:
	-rm -f *.o *.ko *.mod.c .*.o.flags .*.cmd *~
	-rm -rf .tmp*
