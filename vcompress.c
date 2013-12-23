#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/time.h>
#include <x264.h>
#include "vcompress.h"
#include "utils.h"

struct Ctx
{
	x264_t *x264;
	x264_picture_t picture;
	x264_param_t param;
	void *output;		// 用于保存编码后的完整帧
	int output_bufsize, output_datasize;
	int64_t pts; 		// 输入 pts
	int64_t (*get_pts)(struct Ctx *);

	int64_t info_pts, info_dts;
	int info_key_frame;
	int info_valid;

	int force_keyframe;
    tst_t t;
};
typedef struct Ctx Ctx;

// return currsec * 90000
static int64_t curr ()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	double n = tv.tv_sec + 0.000001*tv.tv_usec;
	n *= 90000.0;
	return (int64_t)n;
}

static int64_t next_pts (struct Ctx *ctx)
{
	int64_t now = curr();
	return now - ctx->pts;
}

static int64_t first_pts (struct Ctx *ctx)
{
	ctx->pts = curr();
	ctx->get_pts = next_pts;
	return 0;
}

void *vc_open (int width, int height, double fps)
{
	Ctx *ctx = malloc(sizeof(Ctx));

	ctx->force_keyframe = 0;

	// 设置编码属性
	//x264_param_default(&ctx->param);
	x264_param_default_preset(&ctx->param, "ultrafast", "zerolatency");
	//x264_param_default_preset(&ctx->param, "ultrafast", "");
	//x264_param_apply_profile(&ctx->param, "baseline");
	//x264_param_apply_profile(&ctx->param, "main");

	ctx->param.i_width = width;
	ctx->param.i_height = height;
	ctx->param.b_repeat_headers = 1;  // 重复SPS/PPS 放到关键帧前面
	ctx->param.b_cabac = 1;     //墒编码，适用于Main/High Profile
    ctx->param.i_threads = 1;
    //ctx->param.i_threads = 4;
    //ctx->param.i_lookahead_threads = 2;

	// ctx->param.b_intra_refresh = 1;

	ctx->param.i_fps_num = (int)fps;
	ctx->param.i_fps_den = 1;
	//ctx->param.b_vfr_input = 1;

	ctx->param.i_keyint_max = ctx->param.i_fps_num * 2;
	//ctx->param.i_keyint_min = 1;

	// rc
	// ctx->param.rc.i_rc_method = X264_RC_CRF;
	ctx->param.rc.i_bitrate = 50;
	//ctx->param.rc.f_rate_tolerance = 0.1;
	//ctx->param.rc.i_vbv_max_bitrate = ctx->param.rc.i_bitrate * 1.3;
	//ctx->param.rc.f_rf_constant = 600;
	//ctx->param.rc.f_rf_constant_max = ctx->param.rc.f_rf_constant * 1.3;

#ifdef DEBUG
	ctx->param.i_log_level = X264_LOG_WARNING;
#else
	ctx->param.i_log_level = X264_LOG_NONE;
#endif // release

	ctx->x264 = x264_encoder_open(&ctx->param);
	if (!ctx->x264) {
		fprintf(stderr, "%s: x264_encoder_open err\n", __func__);
		free(ctx);
		return 0;
	}

	x264_picture_init(&ctx->picture);
	ctx->picture.img.i_csp = X264_CSP_I420;
	ctx->picture.img.i_plane = 3;

	ctx->output = malloc(128*1024);
	ctx->output_bufsize = 128*1024;
	ctx->output_datasize = 0;

	ctx->get_pts = first_pts;
	ctx->info_valid = 0;

    ctx->t = tst_create();

	return ctx;
}

int vc_close (void *ctx)
{
	Ctx *c = (Ctx*)ctx;
    tst_free(c->t);
	x264_encoder_close(c->x264);
	free(c->output);
	free(c);
	return 1;
}

int vc_get_last_frame_info (void *ctx, int *key_frame, int64_t *pts, int64_t *dts)
{
	Ctx *c = (Ctx *)ctx;
	if (c->info_valid) {
		*key_frame = c->info_key_frame;
		*pts = c->info_pts;
		*dts = c->info_dts;
		return 1;
	}
	else {
		return -1;
	}
}

static int encode_nals (Ctx *c, x264_nal_t *nals, int nal_cnt)
{
	char *pout = (char*)c->output;
	c->output_datasize = 0;
	for (int i = 0; i < nal_cnt; i++) {
		if (c->output_datasize + nals[i].i_payload > c->output_bufsize) {
			// 扩展
			c->output_bufsize = (c->output_datasize+nals[i].i_payload+4095)/4096*4096;
			c->output = realloc(c->output, c->output_bufsize);
		}
		memcpy(pout+c->output_datasize, nals[i].p_payload, nals[i].i_payload);
		c->output_datasize += nals[i].i_payload;
	}

	return c->output_datasize;
}

static void dumpnal (x264_nal_t *nal)
{
	// 打印前面 10 个字节
	for (int i = 0; i < nal->i_payload && i < 20; i++) {
		fprintf(stderr, "%02x ", (unsigned char)nal->p_payload[i]);
	}
}

static void dumpnals (int type, x264_nal_t *nal, int nals)
{
	fprintf(stderr, "======= dump nals %d type=%d, ======\n", nals, type);
	for (int i = 0; i < nals; i++) {
		fprintf(stderr, "\t nal  #%d: %dbytes, nal type=%d ", i, nal[i].i_payload, nal[i].i_type);
		dumpnal(&nal[i]);
		fprintf(stderr, "\n");
	}
}

int vc_compress (void *ctx, unsigned char *data[4], int stride[4], const void **out, int *len)
{
	Ctx *c = (Ctx*)ctx;
	
	// 设置 picture 数据
	for (int i = 0; i < 4; i++) {
		c->picture.img.plane[i] = data[i];
		c->picture.img.i_stride[i] = stride[i];
	}

	// encode
	x264_nal_t *nals;
	int nal_cnt;
	x264_picture_t pic_out;
	
	c->picture.i_pts = c->get_pts(c);

	x264_picture_t *pic = &c->picture;

	if (c->force_keyframe) {
		c->picture.i_type = X264_TYPE_IDR;
		c->force_keyframe = 0;
	}
	else {
		c->picture.i_type = X264_TYPE_AUTO;
	}

	do {
		// 这里努力消耗掉 delayed frames ???
		// 实际使用 zerolatency preset 时, 效果足够好了
//        tst_start(c->t); //test
        int rc = x264_encoder_encode(c->x264, &nals, &nal_cnt, pic, &pic_out);
  //      tst_print_and_start(c->t, "x264_encoder_encode"); //test
		if (rc < 0) return -1;
		if (pic_out.b_keyframe) {
			//dumpnals(pic_out.i_type, nals, nal_cnt);
		}
		encode_nals(c, nals, nal_cnt);
	} while (0);

	*out = c->output;
	*len = c->output_datasize;

	if (nal_cnt > 0) {
		c->info_valid = 1;
		c->info_key_frame = pic_out.b_keyframe;
		c->info_pts = pic_out.i_pts;
		c->info_dts = pic_out.i_dts;
	}
	else {
		fprintf(stderr, ".");
		return 0; // 继续
	}

#ifdef DEBUG_MORE
	static int64_t _last_pts = c->info_pts;

	fprintf(stderr, "DBG: pts delta = %lld\n", c->info_pts - _last_pts);
	_last_pts = c->info_pts;
#endif //


#ifdef DEBUG_MORE
	static size_t _seq = 0;

	fprintf(stderr, "#%lu: [%c] frame type=%d, size=%d\n", _seq, 
			pic_out.b_keyframe ? '*' : '.', 
			pic_out.i_type, c->output_datasize);

	_seq++;
#endif // debug

	return 1;
}

int vc_force_keyframe (void *ctx)
{
	Ctx *c = (Ctx*)ctx;
	c->force_keyframe = 1;
	return 1;
}

