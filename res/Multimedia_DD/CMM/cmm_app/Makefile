CC = /usr/local/arm/4.2.2-eabi/usr/bin/arm-linux-gcc

CFLAGS = -Wall -Os 

CSRCS = ./cmm_test.c

OBJS = $(CSRCS:.c=.o)

.SUFFIXES:.c.o

all:	cmm_test

cmm_test: $(OBJS)
	$(CC) $(CFLAGS) -g -o $@ $(OBJS) 
	
.c.o:
	$(CC) $(CFLAGS) -g -c -o $@ $<

clean:
	rm -f cmm_test $(OBJS) 

