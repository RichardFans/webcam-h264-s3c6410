CFLAGS:=-c -Wall -g -Os -std=gnu99 

ifeq ($(tag),s)
CROSS_COMPILE?=arm-linux-

ifeq ($(CROSS_COMPILE),)
LIBS :=   $(shell pkg-config --libs libavformat libavcodec libswscale libavutil) \
-lm -lpthread
INC:= $(shell pkg-config --cflags libavcodec libswscale libavutil) 
else
LIBS:=	-lavformat -lavcodec -lasound -lmp3lame -lfdk-aac \
-lz -ldl -lswscale -lavutil -lm -lx264 -lpthread
INC 	:= 	-I./Common -I./FrameExtractor -I./MFC_API
endif

OBJS 	:=	capture.o sender.o server.o utils.o v4l2.o app.o hashmap.o heap.o

#CFLAGS += -DDBG_V4L
#CFLAGS += -DDBG_APP

ifeq ($(enc), hw)
OBJS 	+=  vcompress_hw.o
OBJS 	+= 	./Common/libCommon.a		\
	  	  	./FrameExtractor/libFrameExtractor.a \
		  	./MFC_API/libMFC_API.a

CFLAGS 	+= 	-DHW_VC

all: common frame_extractor mfc_api webcam_server
common : 
	cd Common; $(MAKE)
frame_extractor : 
	cd FrameExtractor; $(MAKE)
mfc_api : 
	cd MFC_API; $(MAKE)
else
OBJS 	+=  vcompress.o

all: webcam_server
endif


else
OBJS:= 	vshow.o recver.o shower.o
LIBS:=  $(shell pkg-config --libs libavformat libavcodec libswscale libavutil) -lX11 -lXext -lm -lpthread
INC:= 	$(shell pkg-config --cflags libavcodec libswscale libavutil)

all: webcam_shower
	make tag=s
endif

CC:=$(CROSS_COMPILE)gcc

webcam_server: $(OBJS)
	$(CC) -o $@ $^ $(LIBS) 

webcam_shower: $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $< $(INC)

clean:
	rm -f *.o 
	rm -f webcam_server
	rm -f webcam_shower
	$(MAKE) clean -C Common
	$(MAKE) clean -C FrameExtractor
	$(MAKE) clean -C MFC_API

install:
	install webcam_server /nfs/rootfs
