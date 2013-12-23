.SUFFIXES : .c .o

OBJECTS = display_test.o 				\
		  display_4_windows.o			\
		  display_optimization1.o		\
		  display_optimization2.o		\
		  cam_encoder_test.o			\
		  cam_enc_dec_test.o			\
		  cam_dec_preview.o				\
		  capture.o						\
		  jpeg_display.o				\
		  test.o						\
		  ./Common/performance.o		\
		  ./Common/LogMsg.o				\
		  ./FrameExtractor/FileRead.o 		\
	  	  ./FrameExtractor/FrameExtractor.o \
		  ./FrameExtractor/H263Frames.o		\
		  ./FrameExtractor/H264Frames.o		\
		  ./FrameExtractor/MPEG4Frames.o	\
		  ./FrameExtractor/VC1Frames.o		\
		  ./JPEG_API/JPGApi.o				\
		  ./MFC_API/SsbSipH264Decode.o	\
		  ./MFC_API/SsbSipH264Encode.o	\
		  ./MFC_API/SsbSipMfcDecode.o	\
		  ./MFC_API/SsbSipMpeg4Decode.o	\
		  ./MFC_API/SsbSipMpeg4Encode.o	\
		  ./MFC_API/SsbSipVC1Decode.o
SRCS = $(OBJECTS:.o=.c)
DEPENDENCY = ./Common/lcd.h 			\
		     ./Common/LogMsg.h 			\
		     ./Common/mfc.h 			\
		     ./Common/MfcDriver.h 		\
		     ./Common/MfcDrvParams.h 	\
		     ./Common/performance.h 	\
			 ./Common/post.h			\
			 ./Common/videodev2.h		\
			 ./Common/videodev2_s3c.h	\
			 ./FrameExtractor/FileRead.h 		\
		     ./FrameExtractor/FrameExtractor.h 	\
		     ./FrameExtractor/H263Frames.h 		\
		     ./FrameExtractor/H264Frames.h 		\
		     ./FrameExtractor/MPEG4Frames.h 	\
		     ./FrameExtractor/VC1Frames.h 		\
			 ./JPEG_APIJPGApi.h					\
			 ./MFC_API/SsbSipH264Decode.h		\
		     ./MFC_API/SsbSipH264Encode.h		\
		     ./MFC_API/SsbSipMfcDecode.h		\
		     ./MFC_API/SsbSipMpeg4Decode.h		\
		     ./MFC_API/SsbSipMpeg4Encode.h		\
		     ./MFC_API/SsbSipVC1Decode.h		\
			 display_test.h						\
			 display_4_windows.h				\
			 display_optimization1.h			\
   			 display_optimization2.h			\
	  		 cam_encoder_test.h					\
	  		 cam_enc_dec_test.h					\
	  		 cam_dec_preview.h					\
	  		 capture.h							\
	  		 jpeg_display.h

CC = arm-linux-gcc
#CFLAGS = -g -c -DRGB24BPP -Os -Wall
#CFLAGS = -g -c -Os -Wall
CFLAGS = -g -c -Os -Wall
INC = -I./Common -I./FrameExtractor -I./MFC_API -I./JPEG_API


TARGET = multimediatest

all : common frame_extractor jpeg_api mfc_api multimediatest
common : 
		cd Common; $(MAKE)

frame_extractor : 
		cd FrameExtractor; $(MAKE)

jpeg_api : 
		cd JPEG_API; $(MAKE)

mfc_api : 
		cd MFC_API; $(MAKE)

$(TARGET) : $(OBJECTS)
		$(CC) -o $(TARGET) $(OBJECTS) -lpthread

.c.o :
		$(CC) $(INC) $(CFLAGS) $<

clean :
		rm -rf $(OBJECTS) $(TARGET) core

