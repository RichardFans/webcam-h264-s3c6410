CC = /usr/local/arm/4.2.2-eabi/usr/bin/arm-linux-gcc

CFLAGS = -Wall -O2 

CSRCS = ./performance.c ./LogMsg.c ./JPGApi.c ./test.c

OBJS = $(CSRCS:.c=.o)

.SUFFIXES:.c.o

all:	test

test: $(OBJS)
	$(CC) $(CFLAGS) -g -o $@ $(OBJS)
	
.c.o:
	$(CC) $(CFLAGS) -g -c -o $@ $<

clean:
	rm -f test $(OBJS) 
