CC = /usr/local/arm/4.2.2-eabi/usr/bin/arm-linux-gcc

#[Definitions]
#FPS : performance measurement. It doesn't make the output file
#ROTATE_ENABLE : Rotation mode enable when testing line buffer and ring buffer.
#                and if you want to test rotation, you must modify definition as "#define MFC_ROTATE_ENABLE 1" in 
#                MfcConfig.h file 
#RGB24BPP : display as 24bpp. default is 16bpp
#DIVX_ENABLE : test for hybrid divx decoder

CFLAGS = -Wall -Os 

CSRCS = ./MPEG4Frames.c 		\
		./H264Frames.c 			\
		./VC1Frames.c 			\
		./H263Frames.c 			\
		./FrameExtractor.c 		\
		./line_buf_test.c 		\
		./ring_buf_test.c 		\
		./display_test.c		\
		./display_optimization1.c	\
		./display_optimization2.c	\
		./encoder_test.c		\
		./SsbSipH264Decode.c 	\
		./SsbSipMpeg4Decode.c 	\
		./SsbSipVC1Decode.c 	\
		./SsbSipMfcDecode.c		\
		./SsbSipH264Encode.c	\
		./SsbSipMpeg4Encode.c	\
		./SsbSipLogMsg.c 		\
		./performance.c 		\
		./FileRead.c			\
		./demo.c				\
		./test.c 

OBJS = $(CSRCS:.c=.o)

.SUFFIXES:.c.o

all:	mfc

mfc: $(OBJS)
	$(CC) $(CFLAGS) -g -o $@ $(OBJS) -lpthread
	
.c.o:
	$(CC) $(CFLAGS) -g -c -o $@ $<

clean:
	rm -f mfc $(OBJS) 

