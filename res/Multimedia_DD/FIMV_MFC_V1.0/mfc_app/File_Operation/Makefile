CC = /usr/local/arm/4.2.2-eabi/usr/bin/arm-linux-gcc

#[Definitions]
#FPS : performance measurement. It doesn't make the output file
#ROTATE_ENABLE : rotation mode enable
CFLAGS = -Wall -Os 

CSRCS = ./MPEG4Frames.c 	\
		./H264Frames.c 		\
		./VC1Frames.c 		\
		./H263Frames.c 		\
		./FrameExtractor.c 	\
		./line_buf_test.c 	\
		./ring_buf_test.c 	\
		./encoder_test.c 	\
		./display_test.c 	\
		./demo.c			\
		./performance.c 	\
		./test.c 

OBJS = $(CSRCS:.c=.o)

.SUFFIXES:.c.o

all:	mfc

mfc: $(OBJS)
	$(CC) $(CFLAGS) -g -o $@ $(OBJS)
	
.c.o:
	$(CC) $(CFLAGS) -g -c -o $@ $<

clean:
	rm -f mfc $(OBJS) 
