
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

performance.o : $(DEPENDENCY) performance.c
LogMsg.o : $(DEPENDENCY) LogMsg.c

