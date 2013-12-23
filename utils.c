#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.h"

#if defined(DBG_UTI)
#define pr_debug(fmt, ...) \
    printf("[%s][%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) \
    do {} while(0)
#endif

int readn(int fd, void *pbuf, size_t n)
{
	int nread;
	int nleft;       
	
    nleft = n;
	while (nleft > 0) {
		if ((nread = read(fd, pbuf, nleft)) < 0) {
		    if (nleft == n)
                return -1;
            else
                break;
        } else if (nread == 0) {
            break;          /* EOF */
		} 
        nleft -= nread;
        pbuf  += nread;
	} 
	return n - nleft;
}

int writen(int fd, void *pbuf, size_t n)
{
	int nwritten;
	int nleft;       
	
    nleft = n;
	while (nleft > 0) {
        if ((nwritten = write(fd, pbuf, nleft)) < 0) {
            if (nleft == n)
                return -1;
            if (errno == EAGAIN) 
                continue;
            break;
        } else if (nwritten == 0) {
            break;          /* EOF */
        } 
        nleft -= nwritten;
        pbuf  += nwritten;
	} 
	return n - nleft;
}

/*
 * 在字符串中找到第一个不是指定字符的位置
 */
char* strnchr(char *str, char ch)
{
    char *pstr = str;
    while (*pstr) {
        if (*pstr!=ch)
            return pstr;
        pstr++;
    }
    return NULL;
}

int setnonblocking(int sfd)
{
    int flags, s;

    if (-1 == (flags = fcntl (sfd, F_GETFL, 0))) {
        perror ("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    if (-1 == (s = fcntl (sfd, F_SETFL, flags))) {
        perror ("fcntl");
        return -1;
    }
    return 0;
}

static int 
setsockaddr(struct sockaddr_in *addr, char *ip, int port)
{  
	int siz = 0;
	
	addr->sin_addr.s_addr = INADDR_ANY;	
	if (ip) {
		siz = strlen(ip);
		if(siz < 7 || siz > 15) {
			pr_debug("sock_addr is illegal!\n");
			return -1;
		}
		addr->sin_addr.s_addr = inet_addr(ip);
	} 

	addr->sin_family = AF_INET;
  	addr->sin_port = htons (port);
   	bzero(&addr->sin_zero, 8);
	return 0;
}

int tcp_srv_sock(int port)
{
	struct sockaddr_in addr;
	int on = 1;
	int sock;
	
	if (-1 == (sock = socket(AF_INET, SOCK_STREAM, 0))) {
        perror("tcp_srv_sock: socket");
        return -1;
    }
		
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
	                      &on, sizeof(int))) {
        perror("tcp_srv_sock: setsockopt");
        goto err_sock;
    }
		
	if (-1 == setsockaddr(&addr, NULL, port)) {
        perror("tcp_srv_sock: setsockaddr");
        goto err_sock;
    }
	
	if (-1 == bind(sock, (struct sockaddr*)&addr, 
                   sizeof(struct sockaddr))) {
        perror("tcp_srv_sock: bind");
        goto err_sock;
    }
		
	if (-1 == listen(sock, LISTEN_QUEUE_LEN)) {
        perror("tcp_srv_sock: listen");
        goto err_sock;
    }
		
    return sock;
err_sock:
    close(sock);
	return -1;
}

int tcp_cli_sock(char *ip, int port)
{
	struct sockaddr_in addr;
	int sock;

	if (-1 == (sock = socket(AF_INET, SOCK_STREAM, 0))) {
        perror("tcp_cli_sock: socket");
        return -1;
    }

	if (-1 == setsockaddr(&addr, ip, port)) {
        perror("tcp_cli_sock: setsockaddr");
        goto err_sock;
    }

	if (-1 == connect(sock, (struct sockaddr*)&addr,
	                  sizeof(struct sockaddr))) {
        perror("tcp_cli_sock: connect");
        goto err_sock;
    }
		
    return sock;
err_sock:
    close(sock);
	return -1;
}

void zoom_rgb(unsigned char *pSrcImg, unsigned char *pDstImg, 
        int nWidth, int nHeight, float fRateW,float fRateH)  
{  
    int i = 0;  
    int j = 0;  

    float fX, fY;  

    int iStepSrcImg = nWidth;  
    int iStepDstImg = nWidth * fRateW;  

    int iX, iY;  

    unsigned char bUpLeft, bUpRight, bDownLeft, bDownRight;  
    unsigned char gUpLeft, gUpRight, gDownLeft, gDownRight;  
    unsigned char rUpLeft, rUpRight, rDownLeft, rDownRight;  

    unsigned char b, g, r;  

    for(i = 0; i < nHeight * fRateH; i++) {  
        for(j = 0; j < nWidth * fRateW; j++) {  
            fX = ((float)j) /fRateW;  
            fY = ((float)i) /fRateH;  

            iX = (int)fX;  
            iY = (int)fY;  

            bUpLeft  = pSrcImg[iY * iStepSrcImg * 3 + iX * 3 + 0];  
            bUpRight = pSrcImg[iY * iStepSrcImg * 3 + (iX + 1) * 3 + 0];  

            bDownLeft  = pSrcImg[(iY + 1) * iStepSrcImg * 3 + iX * 3 + 0];  
            bDownRight = pSrcImg[(iY + 1) * iStepSrcImg * 3 + (iX + 1) * 3 + 0];  

            gUpLeft  = pSrcImg[iY * iStepSrcImg * 3 + iX * 3 + 1];  
            gUpRight = pSrcImg[iY * iStepSrcImg * 3 + (iX + 1) * 3 + 1];  

            gDownLeft  = pSrcImg[(iY + 1) * iStepSrcImg * 3 + iX * 3 + 1];  
            gDownRight = pSrcImg[(iY + 1) * iStepSrcImg * 3 + (iX + 1) * 3 + 1];  

            rUpLeft  = pSrcImg[iY * iStepSrcImg * 3 + iX * 3 + 2];  
            rUpRight = pSrcImg[iY * iStepSrcImg * 3 + (iX + 1) * 3 + 2];  

            rDownLeft  = pSrcImg[(iY + 1) * iStepSrcImg * 3 + iX * 3 + 2];  
            rDownRight = pSrcImg[(iY + 1) * iStepSrcImg * 3 + (iX + 1) * 3 + 2];  

            b = bUpLeft * (iX + 1 - fX) * (iY + 1 - fY) + bUpRight * (fX - iX) * (iY + 1 - fY)  
                + bDownLeft * (iX + 1 - fX) * (fY - iY) + bDownRight * (fX - iX) * (fY - iY);  

            g = gUpLeft * (iX + 1 - fX) * (iY + 1 - fY) + gUpRight * (fX - iX) * (iY + 1 - fY)  
                + gDownLeft * (iX + 1 - fX) * (fY - iY) + gDownRight * (fX - iX) * (fY - iY);  

            r = rUpLeft * (iX + 1 - fX) * (iY + 1 - fY) + rUpRight * (fX - iX) * (iY + 1 - fY)  
                + rDownLeft * (iX + 1 - fX) * (fY - iY) + rDownRight * (fX - iX) * (fY - iY);  

            if (iY >= 0 && iY <= nHeight * 2 && iX >= 0 && iX <= nWidth * 2) {  
                pDstImg[i * iStepDstImg * 3 + j * 3 + 0] = b;        //B  
                pDstImg[i * iStepDstImg * 3 + j * 3 + 1] = g;        //G  
                pDstImg[i * iStepDstImg * 3 + j * 3 + 2] = r;        //R  
            }  
        }  
    }  
}  

int convert_yuv422_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
    int r, g, b;
    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;
    pixel[0] = r ;
    pixel[1] = g ;
    pixel[2] = b ;
    return pixel32;
}
 
int convert_yuv422_to_rgb_buffer(const unsigned char *yuv, unsigned char *rgb, 
                                 unsigned int width, unsigned int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;

    for (in = 0; in < width * height * 2; in += 4) {
        pixel_16 = yuv[in + 3] << 24 |
            yuv[in + 2] << 16 |
            yuv[in + 1] <<  8 |
            yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;
        pixel32 = convert_yuv422_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
        pixel32 = convert_yuv422_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }
    return 0;
}

int convert_yuv422_to_yuv444_buffer(const unsigned char *yuv422, unsigned char *yuv444, 
                                   unsigned int width, unsigned int height)
{
    const unsigned char *psrc = yuv422;
    unsigned char *pdst = yuv444;
    int pixel_total = width * height;
    int i;
    for (i = 0; i < pixel_total; i++) 
        pdst[3*i] = psrc[2*i];
    pixel_total /= 2;
    for (i = 0; i < pixel_total; i++) { 
        pdst[6*i + 1] = psrc[4*i + 1];
        pdst[6*i + 4] = psrc[4*i + 1];
        pdst[6*i + 2] = psrc[4*i + 3];
        pdst[6*i + 5] = psrc[4*i + 3];
    } 
    return 0;
}

struct tst {
    clockid_t start_real, end_real;
    struct tms start, end;
    long clktck;
};

tst_t tst_create()
{
    struct tst *c = calloc(1, sizeof(struct tst));
    c->clktck = sysconf(_SC_CLK_TCK);
    return c;
}

void tst_free(tst_t tst)
{
    struct tst *c = tst;
    free(c);
}

void tst_start(tst_t tst)
{
    struct tst *c = tst;
    c->start_real = times(&c->start);
}

void tst_print(tst_t tst, const char *info)
{
    struct tst *c = tst;
    double t, stamp;
    c->end_real = times(&c->end);
    if (c->end_real == -1)
        pr_debug("times");
    t = (c->end_real - c->start_real)/((double)c->clktck);
    stamp = c->end_real / (double)c->clktck;
    printf("%s(%f): %f\n", info, stamp, t);
}

void tst_print_and_start(tst_t tst, const char *info)
{
    tst_print(tst, info);
    tst_start(tst);
}
