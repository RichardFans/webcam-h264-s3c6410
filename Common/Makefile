
OBJECTS = performance.o \
	  	  LogMsg.o 

SRCS = $(OBJECTS:.o=.c)
DEPENDENCY = lcd.h 				\
		     LogMsg.h 			\
		     mfc.h 				\
		     MfcDriver.h 		\
		     MfcDrvParams.h 	\
		     performance.h 		\
			 post.h				\
			 videodev2.h		\
			 videodev2_s3c.h	

AR = arm-linux-ar
AFLAGS = crv
CC = arm-linux-gcc
CFLAGS = -c -Wall -g -Os -std=gnu99 
INC = -I../Common

TARGET = libCommon.a

$(TARGET) : $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

.c.o :
	$(CC) $(INC) $(CFLAGS) $<

clean :
	rm -rf $(OBJECTS) $(TARGET) 

performance.o : $(DEPENDENCY) performance.c
LogMsg.o : $(DEPENDENCY) LogMsg.c

