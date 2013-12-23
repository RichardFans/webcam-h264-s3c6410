#ifndef _vshow__hh
#define _vshow__hh

/** 使用 X11 显示 YUV
 */

// 参数为需要显示的 yuv 图片的大小
void *vs_open (int v_width, int v_height);
int vs_close (void *ctx);

// 显示 PIX_FMT_YUV420P
int vs_show (void *ctx, unsigned char *data[4], int stride[4]);

#endif // vshow.h

