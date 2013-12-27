#ifndef _capture__hh
#define _capture__hh

#include "types.h"

// 打开 usb webcam 设备， 设置输出大小， 输出类型
void *capture_open (const char *dev_name, int v_width, int v_height, PixelFormat fmt);
int capture_get_picture (void *id, Picture *pic);
int capture_close (void *id);

int capture_get_output_ptr (void *id, unsigned char ***ptr, int **stride);

#endif // capture.h
