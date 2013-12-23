#ifndef _vcompress__hh
#define _vcompress__hh

#include "types.h"
/** FIXME: vc_open 将需要增加很多入口参数, 用于设置编码属性 */
void *vc_open (int width, int height, double fps);
int vc_close (void *ctx);

/** 每当 返回 > 0, 说明得到一帧数据, 此时可以调用 vc_get_last_frame_info() 获取更多信息 */
int vc_compress (void *ctx, unsigned char *data[4], int stride[4], const void **buf, int *len);
int vc_get_last_frame_info (void *ctx, int *key_frame, int64_t *pts, int64_t *dts);

/** 强制尽快输出关键帧 */
int vc_force_keyframe (void *ctx);

#endif // vcompress.h

