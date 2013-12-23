#ifndef _types__hh
#define _types__hh

// yuv
struct Picture
{
	unsigned char *data[4];
	int stride[4];
};
typedef struct Picture Picture;

typedef enum PixelFormat PixelFormat;
typedef struct v4l2_capability v4l2_capability;
typedef struct v4l2_requestbuffers v4l2_requestbuffers;
typedef struct v4l2_buffer v4l2_buffer;
typedef struct v4l2_format v4l2_format;
typedef struct v4l2_fmtdesc v4l2_fmtdesc;

#endif // types.h

