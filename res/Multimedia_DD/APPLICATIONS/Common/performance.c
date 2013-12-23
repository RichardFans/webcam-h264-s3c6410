#include <sys/time.h>

#ifdef FPS
unsigned int measureTime(struct timeval *start, struct timeval *stop)
{
	unsigned int sec, usec, time;

	sec = stop->tv_sec - start->tv_sec;
	if(stop->tv_usec >= start->tv_usec)
	{
		usec = stop->tv_usec - start->tv_usec;
	}
   	else
	{	
		usec = stop->tv_usec + 1000000 - start->tv_usec;
		sec--;
  	}
	time = sec*1000 + ((double)usec)/1000;
	return time;
}
#endif
