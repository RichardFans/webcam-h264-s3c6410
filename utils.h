#ifndef __UTIL_H__
#define __UTIL_H__

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define FABS(a)         ((a) < 0 ? -(a) : (a))

#define DIV_ROUND_CLOSEST(x, divisor)(          \
{                           \
    typeof(divisor) __divisor = divisor;        \
    (((x) + ((__divisor) / 2)) / (__divisor));  \
}                           \
)

int readn(int fd, void *pbuf, size_t n);
int writen(int fd, void *pbuf, size_t n);
char* strnchr(char *str, char ch);
#include <sys/ioctl.h>
static inline int xioctl(int fd, int request, void *arg)
{
	int r;

	do {
		r = ioctl (fd, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

#define LISTEN_QUEUE_LEN  5	

int tcp_srv_sock(int port);
int tcp_cli_sock(char *ip, int port);
int setnonblocking(int sfd);

void zoom_rgb(unsigned char *pSrcImg, unsigned char *pDstImg, 
        int nWidth, int nHeight, float fRateW,float fRateH);
int convert_yuv422_to_rgb_pixel(int y, int u, int v);
int convert_yuv422_to_rgb_buffer(const unsigned char *yuv, unsigned char *rgb, 
                                 unsigned int width, unsigned int height);
int convert_yuv422_to_yuv444_buffer(const unsigned char *yuv422, unsigned char *yuv444, 
                                    unsigned int width, unsigned int height);

typedef struct tst *tst_t;
tst_t tst_create();
void tst_free(tst_t tst);
void tst_start(tst_t tst);
void tst_print(tst_t tst, const char *info);
void tst_print_and_start(tst_t tst, const char *info);

#endif

