#################################################
# Makefile for 2D on S3C6400
# 2007 (C) Samsung Electronics
# Author : yr.ohm <yr.ohm@samsung.com>
#################################################

# where the kernel sources are located
KERNEL_DIR := ../../s3c-linux-2.6.21

obj-m   += s3c_2d_driver.o

here:
	(cd $(KERNEL_DIR); make SUBDIRS=$(PWD) modules)

clean:
	-rm -f *.o *.ko *.mod.c .*.o.flags .*.cmd *~
	-rm -rf .tmp*
