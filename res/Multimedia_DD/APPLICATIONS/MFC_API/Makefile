OBJECTS = SsbSipH264Decode.o	\
		  SsbSipH264Encode.o	\
		  SsbSipMfcDecode.o		\
		  SsbSipMpeg4Decode.o	\
		  SsbSipMpeg4Encode.o	\
		  SsbSipVC1Decode.o	

SRCS = $(OBJECTS:.o=.c)
DEPENDENCY = SsbSipH264Decode.h		\
		     SsbSipH264Encode.h		\
		     SsbSipMfcDecode.h		\
		     SsbSipMpeg4Decode.h	\
		     SsbSipMpeg4Encode.h	\
		     SsbSipVC1Decode.h	

CC = arm-linux-gcc
CFLAGS = -g -c -Os -Wall
INC = -I../Common

TARGET = app

$(TARGET) : $(OBJECTS)
		#$(CC) -o $(TARGET) $(OBJECTS)

.c.o :
		$(CC) $(INC) $(CFLAGS) $<

clean :
		rm -rf $(OBJECTS) $(TARGET) core

SsbSipH264Decode.o : $(DEPENDENCY) SsbSipH264Decode.c
SsbSipH264Encode.o : $(DEPENDENCY) SsbSipH264Encode.c
SsbSipMfcDecode.o : $(DEPENDENCY) SsbSipMfcDecode.c
SsbSipMpeg4Decode.o : $(DEPENDENCY) SsbSipMpeg4Decode.c
SsbSipMpeg4Encode.o : $(DEPENDENCY) SsbSipMpeg4Encode.c
SsbSipVC1Decode.o : $(DEPENDENCY) SsbSipVC1Decode.c

