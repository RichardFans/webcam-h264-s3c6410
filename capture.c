#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include "capture.h"
#include "v4l2.h"
#include "app.h"
#include "utils.h"

struct Ctx
{
    v4l2_dev_t v;
	int width, height;	// 输出图像大小
	struct SwsContext *sws;	// 用于转换
	int rows;	// 用于 sws_scale()
	int bytesperrow; // 用于cp到 pic_src
	AVPicture pic_src, pic_target;	// 用于 sws_scale
    PixelFormat fmt;
};
typedef struct Ctx Ctx;

void capture_handler(const void *p, int size, void *arg)
{
    Ctx *ctx = (Ctx*)arg;
    
    tst_t t = tst_create();
    tst_start(t);

    avpicture_fill(&ctx->pic_src, p, PIX_FMT_YUV420P,
            ctx->width, ctx->height);
	// sws_scale
	sws_scale(ctx->sws, ctx->pic_src.data, ctx->pic_src.linesize,
            0, ctx->rows, ctx->pic_target.data, ctx->pic_target.linesize);

    //tst_print(t, "sws_scale");
}

void *capture_open (app_t a, const char *dev_name, int t_width, int t_height, PixelFormat tarfmt)
{
	Ctx *ctx = malloc(sizeof(Ctx));
    struct v4l2_format fmt;

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	fmt.fmt.pix.width       = t_width;
	fmt.fmt.pix.height      = t_height;
    ctx->v = v4l2_create2(a, dev_name, &fmt);
    PixelFormat srcfmt = PIX_FMT_YUV420P;

	// 构造转换器
	ctx->width = t_width;
	ctx->height = t_height;
	ctx->sws = sws_getContext(fmt.fmt.pix.width, fmt.fmt.pix.height, srcfmt,
                        ctx->width, ctx->height, tarfmt, 	// PIX_FMT_YUV420P 对应 X264_CSP_I420
                        SWS_FAST_BILINEAR, 0, 0, 0);

	ctx->rows = fmt.fmt.pix.height;
	ctx->bytesperrow = fmt.fmt.pix.bytesperline;

    avpicture_alloc(&ctx->pic_target, tarfmt, ctx->width, ctx->height);

    v4l2_set_img_proc(ctx->v, capture_handler, ctx);
    v4l2_start_capture(ctx->v);

    ctx->fmt = tarfmt;

	return ctx;
}

int capture_get_picture (void *id, Picture *pic)
{
	Ctx *ctx = (Ctx*)id;
	for (int i = 0; i < 4; i++) {
		pic->data[i] = ctx->pic_target.data[i];
		pic->stride[i] = ctx->pic_target.linesize[i];
	}

	return 1;
}

int capture_close (void *id)
{
	Ctx *ctx = (Ctx*)id;

    v4l2_stop_capture(ctx->v);
    v4l2_free(ctx->v);

	avpicture_free(&ctx->pic_target);
	sws_freeContext(ctx->sws);
	free(ctx);

	return 1;
}

