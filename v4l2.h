#ifndef	__V4L2_H__
	#define __V4L2_H__

#define DEF_V4L_DEV  		"/dev/video0"

typedef struct v4l2_dev *v4l2_dev_t;
typedef void (*v4l2_img_proc_t)(const void *p, int size, void *arg);

//采集缓冲队列长度
#define NR_REQBUF 4 

#define MAX_FMT_TYPE_NR     128
#define MAX_FRM_SIZ_NR      32

#include <stdbool.h>
#include <linux/types.h>
#include <linux/videodev2.h>
/* v4l2 用户控制项结构 */
struct v4l2_uctl {
	__u32                id;            /* 用户控制项id */
	enum v4l2_ctrl_type	 type;          /* 控制项类型：整数、布尔等 */
	__s32                val;           /* 控制项当前值 */
	__s32                def_val;       /* 默认值 */
	__s32                min;           /* 最大值 */
	__s32                max;           /* 最小值 */
	__u8                 name[32];      /* 名控制项称 */
};

struct v4l2_uctls {
    __u32 nr;
    struct v4l2_uctl list[];
};

int v4l2_set_uctl(v4l2_dev_t vd, __u32 id, __s32 val);
int v4l2_set_uctls2def(v4l2_dev_t vd);

__s32 v4l2_get_uctl(v4l2_dev_t vd, __u32 id, bool *ok);
void v4l2_get_uctls(v4l2_dev_t vd, struct v4l2_uctl *uctls);
__u32 v4l2_get_uctls_nr(v4l2_dev_t vd);

int v4l2_set_fmt(v4l2_dev_t vd, __u32 fmt_nr, __u32 frm_nr);
__u32 v4l2_get_fmts_nr(v4l2_dev_t vd);
__u32 v4l2_get_fmt_frms_nr(v4l2_dev_t vd, __u32 fmt_nr);
int v4l2_get_fmt(v4l2_dev_t vd, __u32 fmt_nr, struct v4l2_fmtdesc *fmt);
int v4l2_get_format(v4l2_dev_t vd, struct v4l2_format *fmt);
int v4l2_set_format(v4l2_dev_t vd, struct v4l2_format *fmt);
int v4l2_get_frmsize(v4l2_dev_t vd, 
                     __u32 fmt_nr, __u32 frm_nr, 
                     struct v4l2_frmsizeenum *frm);
__u32 v4l2_get_cur_fmt_nr(v4l2_dev_t vd);
__u32 v4l2_get_cur_frm_nr(v4l2_dev_t vd);

v4l2_img_proc_t v4l2_set_img_proc(v4l2_dev_t vd, v4l2_img_proc_t proc, void *arg);
void v4l2_process(v4l2_dev_t vd);

int v4l2_start_capture(v4l2_dev_t vd);
int v4l2_stop_capture(v4l2_dev_t vd);

v4l2_dev_t v4l2_create(const char *dev, __u32 fmt_nr, __u32 frm_nr);
v4l2_dev_t v4l2_create2(const char *dev, struct v4l2_format *fmt);
void v4l2_free(v4l2_dev_t vd);

#endif	//__V4L2_H__

