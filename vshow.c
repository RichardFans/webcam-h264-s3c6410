#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

typedef enum PixelFormat PixelFormat;

struct Ctx
{
	Display *display;
	int screen;
	Window window;
	GC gc;
	XVisualInfo vinfo;
	XImage *image;
	XShmSegmentInfo segment;

	struct SwsContext *sws;
	PixelFormat target_pixfmt;
	AVPicture pic_target;

	int v_width, v_height;
	int curr_width, curr_height;
};
typedef struct Ctx Ctx;

void *vs_open (int v_width, int v_height)
{
	Ctx *ctx = malloc(sizeof(Ctx));
	ctx->v_width = v_width;
	ctx->v_height = v_height;

	// window
	ctx->display = XOpenDisplay(0);
	if (!ctx->display) {
		fprintf(stderr, "%s: XOpenDisplay err\n", __func__);
		exit(-1);
	}
	ctx->window = XCreateSimpleWindow(ctx->display, RootWindow(ctx->display, 0),
			100, 100, v_width, v_height, 0, BlackPixel(ctx->display, 0),
			WhitePixel(ctx->display, 0));
	ctx->screen = 0;
	ctx->gc = XCreateGC(ctx->display, ctx->window, 0, 0);
	
	XMapWindow(ctx->display, ctx->window);

	// current screen pix fmt
	Window root;
	unsigned int cx, cy, border, depth;
	int x, y;
	XGetGeometry(ctx->display, ctx->window, &root, &x, &y, &cx, &cy, &border, &depth);

	// visual info
	XMatchVisualInfo(ctx->display, ctx->screen, depth, DirectColor, &ctx->vinfo);

	// image
	ctx->image = XShmCreateImage(ctx->display, ctx->vinfo.visual, depth, ZPixmap, 0,
			&ctx->segment, cx, cy);
	if (!ctx->image) {
		fprintf(stderr, "%s: can't XShmCreateImage !\n", __func__);
		exit(-1);
	}
	ctx->segment.shmid = shmget(IPC_PRIVATE,
			ctx->image->bytes_per_line * ctx->image->height, 
			IPC_CREAT | 0777);
	if (ctx->segment.shmid < 0) {
		fprintf(stderr, "%s: shmget err\n", __func__);
		exit(-1);
	}

	ctx->segment.shmaddr = (char*)shmat(ctx->segment.shmid, 0, 0);
	if (ctx->segment.shmaddr == (char*)-1) {
		fprintf(stderr, "%s: shmat err\n", __func__);
		exit(-1);
	}

	ctx->image->data = ctx->segment.shmaddr;
	ctx->segment.readOnly = 0;
	XShmAttach(ctx->display, &ctx->segment);

	PixelFormat target_pix_fmt = PIX_FMT_NONE;
	switch (ctx->image->bits_per_pixel) {
		case 32:
			target_pix_fmt = PIX_FMT_RGB32;
			break;
		case 24:
			target_pix_fmt = PIX_FMT_RGB24;
			break;
		default:
			break;
	}

	if (target_pix_fmt == PIX_FMT_NONE) {
		fprintf(stderr, "%s: screen depth format err\n", __func__);
		free(ctx);
		return 0;
	}

	// sws
	ctx->target_pixfmt = target_pix_fmt;
	ctx->curr_width = cx;
	ctx->curr_height = cy;
	ctx->sws = sws_getContext(v_width, v_height, PIX_FMT_YUV420P,
			cx, cy, target_pix_fmt,
			SWS_FAST_BILINEAR, 0, 0, 0);

	avpicture_alloc(&ctx->pic_target, target_pix_fmt, cx, cy);

	XFlush(ctx->display);

	return ctx;
}

int vs_close (void *ctx)
{
	return 1;
}

int MIN(int a, int b)
{
	return a < b ? a : b;
}

int vs_show (void *ctx, const uint8_t *const data[4], int stride[4])
{
	// 首选检查 sws 是否有效, 根据当前窗口大小决定
	Ctx *c = (Ctx*)ctx;
	Window root;
	int x, y;
	unsigned int cx, cy, border, depth;
	XGetGeometry(c->display, c->window, &root, &x, &y, &cx, &cy, &border, &depth);
	if (cx != c->curr_width || cy != c->curr_height) {
		avpicture_free(&c->pic_target);
		sws_freeContext(c->sws);

		c->sws = sws_getContext(c->v_width, c->v_height, PIX_FMT_YUV420P,
				cx, cy, c->target_pixfmt, 
				SWS_FAST_BILINEAR, 0, 0, 0);
		avpicture_alloc(&c->pic_target, c->target_pixfmt, cx, cy);

		c->curr_width = cx;
		c->curr_height = cy;

		// re create image
		XShmDetach(c->display, &c->segment);
		shmdt(c->segment.shmaddr);
		shmctl(c->segment.shmid, IPC_RMID, 0);
		XDestroyImage(c->image);

		c->image = XShmCreateImage(c->display, c->vinfo.visual, depth, ZPixmap, 0,
			&c->segment, cx, cy);

		c->segment.shmid = shmget(IPC_PRIVATE,
				c->image->bytes_per_line * c->image->height,
				IPC_CREAT | 0777);
		c->segment.shmaddr = (char*)shmat(c->segment.shmid, 0, 0);
		c->image->data = c->segment.shmaddr;
		c->segment.readOnly = 0;
		XShmAttach(c->display, &c->segment);
	}

	// 
	sws_scale(c->sws, data, stride, 0, c->v_height, c->pic_target.data, c->pic_target.linesize);

	// cp to image
	unsigned char *p = c->pic_target.data[0], *q = (unsigned char*)c->image->data;
	int xx = MIN(c->image->bytes_per_line, c->pic_target.linesize[0]);
	for (int i = 0; i < c->curr_height; i++) {
		memcpy(q, p, xx);
		p += c->image->bytes_per_line;
		q += c->pic_target.linesize[0];
	}

	// 显示到 X 上
	XShmPutImage(c->display, c->window, c->gc, c->image, 0, 0, 0, 0, c->curr_width, c->curr_height, 1);

	return 1;
}

