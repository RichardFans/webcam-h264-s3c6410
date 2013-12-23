
OBJECTS = JPGApi.o

SRCS = $(OBJECTS:.o=.c)
DEPENDENCY = JPGApi.h

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

JPGApi.o : $(DEPENDENCY) JPGApi.c

