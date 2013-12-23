#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "vcompress.h"
#include "SsbSipH264Encode.h"
#include "LogMsg.h"

//硬解码不具备类似x264的b_repeat_headers属性，无法将
//头信息放到所有i帧中(只会放到第1帧中，事实上只有头
//一帧是i帧)，这样必须先运行客户端的播放器再运行服务器，
//否则会因为解析不到头信息(SPS,PPS)而无法播放
struct Ctx
{
    int width;
    int height;
    void    *handle;
    uint8_t *frm; 
    int hdr_size;
    int strm_size;
    bool first;
    int gop;
    int fnr;
};
typedef struct Ctx Ctx;

void *vc_open (int width, int height, double fps)
{
	Ctx *c = malloc(sizeof(Ctx));

    c->gop = 30;
	c->handle = SsbSipH264EncodeInit(width, height, fps, 1000, c->gop);
	if (c->handle == NULL) {
		LOG_MSG(LOG_ERROR, "Test_Encoder", "SsbSipH264EncodeInit Failed\n");
		return NULL;
	}

	SsbSipH264EncodeExe(c->handle);
    c->width  = width;
    c->height = height;

    c->first = true;
    c->fnr = 0;

	return c;
}

int vc_close (void *ctx)
{
	Ctx *c = (Ctx*)ctx;
	SsbSipH264EncodeDeInit(c->handle);
    free(c->frm);
    free(ctx);
	return 1;
}

static bool is_i_frame(uint8_t *buf)
{
    //printf("0x%02x 0x%02x\n", buf[3], buf[4]);
    if ((buf[4] & 0x1f) == 5)
        return true;
    else 
        return false;
}

int vc_compress (void *ctx, unsigned char *data[4], int stride[4], const void **out, int *len)
{
	Ctx *c = (Ctx*)ctx;
	unsigned char *p_inbuf, *p_outbuf;
    int i, planar_size, nshift, nbufptr = 0;

	p_inbuf = SsbSipH264EncodeGetInBuf(c->handle, 0);

    /* data[4] -> yuv_buf */
    for (i = 0; i < 3; i++) {
        nshift = (i == 0 ? 0:1);
        planar_size = stride[i] * (c->height >> nshift);
        memcpy((char*)p_inbuf + nbufptr, data[i], planar_size);
        nbufptr += planar_size;
    }

	SsbSipH264EncodeExe(c->handle);

	p_outbuf = SsbSipH264EncodeGetOutBuf(c->handle, (long*)len);
    if (p_outbuf == NULL)
        return -1;

    if (c->first) {
        SsbSipH264EncodeGetConfig(c->handle, H264_ENC_GETCONF_HEADER_SIZE, (void*)&c->hdr_size);
        c->strm_size = *len;
        c->frm = malloc(c->strm_size + c->hdr_size);
        memcpy(c->frm, p_outbuf, c->hdr_size);
        *len -= c->hdr_size;
        p_outbuf += c->hdr_size;
        c->first = false;
    } 

    if (*len > c->strm_size) {
        c->strm_size = *len;
        c->frm = realloc(c->frm, c->strm_size + c->hdr_size);
    }
    memcpy(&c->frm[c->hdr_size], p_outbuf, *len);

    //printf("c->hdr_size = %d\n", c->hdr_size);
    //if (is_i_frame(&c->frm[c->hdr_size])) { 
    if (c->fnr == 0) { 
        *out = c->frm;
        *len += c->hdr_size;
        //printf("\n\nkey frame\n\n");
    } else {
        //printf("p b frame\n");
        *out = &c->frm[c->hdr_size];
    }

    c->fnr++;
    if (c->fnr == c->gop)
        c->fnr = 0;

	return 0;
}

int vc_get_last_frame_info (void *ctx, int *key_frame, int64_t *pts, int64_t *dts)
{
    return 0;
}

int vc_force_keyframe (void *ctx)
{
    return 0;
}
