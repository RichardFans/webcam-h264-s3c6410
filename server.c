#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include "app.h"
#include "capture.h"
#include "vcompress.h"
#include "sender.h"
#include "utils.h"

/** webcam_server: 打开 /dev/video0, 获取图像, 压缩, 发送到 localhost:3020 端口
 *
 * 	使用 320x240, fps=10
 */
#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 240

#ifdef HW_VC   /* 软件编码fps=10，硬件编码fps=20 */
#define VIDEO_FPS 15.0
#else
#define VIDEO_FPS 1.0
#endif

#define TARGET_IP "192.168.1.5"
//#define TARGET_IP "127.0.0.1"
#define TARGET_PORT 3021

struct Ctx {
    app_t a;
    void *capture;  
    void *encoder;  
    void *sender;  
    tst_t t;
};

typedef struct Ctx Ctx;

struct app_timer {
    struct timeval exp_tv, inter_tv;  /* 超时时间，间隔时间 */
    void (*efunc)(void*); /* 超时函数 */
    void *arg;      /* 超时函数参数 */
    bool one_shot;        /* one shot */
    bool active;
};

void timer_handler(void *arg)
{
    Ctx *c= (Ctx*)arg;
    // 抓
    Picture pic;
    capture_get_picture(c->capture, &pic);

    // 压
    const void *outdata;
    int outlen;
    tst_start(c->t);  //test
    int rc = vc_compress(c->encoder, pic.data, pic.stride, &outdata, &outlen);
    if (rc < 0) return;
    //tst_print_and_start(c->t, "vc_compress"); //test

    // 发
    sender_send(c->sender, outdata, outlen);
}

int main (int argc, char **argv)
{
	Ctx c;
    c.a = app_create(8);

	c.capture = capture_open(c.a, argv[1], VIDEO_WIDTH, VIDEO_HEIGHT, PIX_FMT_YUV420P);
	if (c.capture == NULL) {
		fprintf(stderr, "ERR: %s\n", strerror(errno));
		exit(-1);
	}

	c.encoder = vc_open(VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
	if (!c.encoder) {
		fprintf(stderr, "ERR: can't open x264 encoder\n");
		exit(-1);
	}

	c.sender = sender_open(TARGET_IP, TARGET_PORT);
	if (!c.sender) {
		fprintf(stderr, "ERR: can't open sender for %s:%d\n", TARGET_IP, TARGET_PORT);
		exit(-1);
	}

    c.t = tst_create();

	int fms = 1000 / VIDEO_FPS;
    app_timer_t timer = app_timer_create(fms, false, timer_handler, &c);
    app_add_timer(c.a, timer);

    app_exec(c.a);
    
    app_del_timer(c.a, timer);
    app_timer_del(timer);
    
	sender_close(c.sender);
	vc_close(c.encoder);
	capture_close(c.capture);

    tst_free(c.t);
    
    app_free(c.a);
	return 0;
}

