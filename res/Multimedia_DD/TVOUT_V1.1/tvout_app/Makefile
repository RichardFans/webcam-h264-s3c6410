#CC=/usr/local/arm/4.0.3/bin/arm-linux-gcc
#LD=/usr/local/arm/4.0.3/bin/arm-linux-ld
#AS=/usr/local/arm/4.0.3/bin/arm-linux-as

##KERNEL_PATH = /home/jaeryul/eldk_samba/kernel/s3c-linux-2[1].6.24_rel-2-1-0_20080820
KERNEL_PATH = /home/jaeryul/eldk_samba/kernel/s3c-linux-2[1].6.24_rel-2-1-2_080829_mod

INCLUDE_DIR = -I$(KERNEL_PATH)/include
INCLUDE_DIR += -I../tvout_drv

CC=/usr/local/arm/4.2.2-eabi/usr/bin/arm-linux-gcc

##CFLAGS = -Wall -O2 -static -lpthread -I $(INCLUDE_DIR) -g
CFLAGS = -Wall -O0 -static -lpthread $(INCLUDE_DIR) -g

CSRCS = ./tv_test.c \
		./FrameExtractor.c \
		./SsbSipH264Decode.c \
		./H264Frames.c \
		./SsbSipMfcDecode.c \
		./SsbSipLogMsg.c 

OBJS = $(CSRCS:.c=.o)

.SUFFIXES:.c.o

all: tv_test
	cp tv_test /home/jaeryul/eldk_nfs/GTK-DFB/peter/tvout_test 
	##cp tv_test /home/jaeryul/eldk_nfs/cramfs/peter/driver

##tv_test: $(OBJS)
##	$(CC) $(CFLAGS) -o $@ $(OBJS) /usr/local/arm/4.1.2/lib/libpthread.a
tv_test: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) /usr/local/arm/4.2.2-eabi/usr/lib/libpthread.a

clean:
	rm -f tv_test $(OBJS)

