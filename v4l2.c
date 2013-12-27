#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <linux/types.h>
#include <libv4l2.h>

#include "utils.h"
#include "v4l2.h"

#if defined(DBG_V4L)
#define pr_debug(fmt, ...) \
    printf("[%s][%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) \
    do {} while(0)
#endif

struct buf {
	void    *start;
	int     len;
};

struct v4l2_frms_fmt {
    struct v4l2_fmtdesc     fmt;
    __u32   frms_nr;
    struct v4l2_frmsizeenum *frms;
};

struct v4l2_dev {
	int                     fd;         /* 设备文件描述符 */      	
	__u8                    name[32];   /* 设备名称 */
	__u8                    drv[16];    /* driver名称 */

	struct v4l2_uctls       *uctls;     /* 用户控制项 */

	struct v4l2_frms_fmt    *ffmts;     
    __u32                    ffmts_nr;   /* 设备支持的像素格式数 */
    __u32                    cur_fmt;    /* 当前像素格式下标 */
    __u32                    cur_frm;    /* 当前帧大小下标 */

	struct buf              *buf;       /* 缓冲区 */
	__u32                   buf_nr;     /* 缓冲区个数 */

	v4l2_img_proc_t         proc;
    void*                   arg;

    tst_t                   t;
};

/*
 * 初始化用户控制项
 */
static int v4l2_uctl_setup(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
	struct v4l2_queryctrl qctl;
	struct v4l2_control ctl;
    struct v4l2_uctl uctls[V4L2_CID_LASTP1 - V4L2_CID_BASE];
	int uctls_nr;

	/* enumerate */
	uctls_nr = 0;
	memset(&qctl, 0, sizeof(qctl));
	for (qctl.id = V4L2_CID_BASE; 
         qctl.id < V4L2_CID_LASTP1; qctl.id++) {
		if (-1  == v4l2_ioctl(v->fd, VIDIOC_QUERYCTRL, &qctl)) {
            if (errno == EINVAL)
                continue;
            perror("VIDIOC_QUERYCTRL");
            return -1;
        }

        if (qctl.flags & V4L2_CTRL_FLAG_DISABLED)
            continue;

        if (qctl.type == V4L2_CTRL_TYPE_MENU) {
            pr_debug("this device has a V4L2_CTRL_TYPE_MENU, "
                     "but now we do not support.\n");
            continue;
        }

		memset(&ctl, 0, sizeof(ctl));
		ctl.id = qctl.id;
        if (-1 == v4l2_ioctl(v->fd, VIDIOC_G_CTRL, &ctl)) {
            perror("VIDIOC_G_CTRL");
            return -1;
        }

		uctls[uctls_nr].id   = qctl.id;
		uctls[uctls_nr].type = qctl.type;
	    uctls[uctls_nr].val  = ctl.value;
		uctls[uctls_nr].def_val = qctl.default_value;
		uctls[uctls_nr].min  = qctl.minimum;
		uctls[uctls_nr].max  = qctl.maximum;
		strcpy((char*)uctls[uctls_nr].name, (char*)qctl.name);
#ifdef DBG_V4L
		pr_debug("uctls: id = 0x%x, type = %d, val = %d,\n"
                "def_val = %d, min = %d, max = %d, name = %s, idx = %d\n", 
                uctls[uctls_nr].id, uctls[uctls_nr].type,
                uctls[uctls_nr].val, uctls[uctls_nr].def_val,
                uctls[uctls_nr].min, uctls[uctls_nr].max,
                uctls[uctls_nr].name, uctls_nr);
#endif
        uctls_nr++;
	}

    v->uctls = malloc(sizeof(struct v4l2_uctls) + uctls_nr * sizeof(struct v4l2_uctl));
    if (v->uctls == NULL) {
        perror("malloc for uctls");
        return -1;
    }
    v->uctls->nr = uctls_nr;
    memcpy(v->uctls->list, uctls, uctls_nr * sizeof(struct v4l2_uctl));
    return 0;
}

static inline void v4l2_uctl_free(v4l2_dev_t vd) {
	struct v4l2_dev *v = vd;
    free(v->uctls);
}

int v4l2_set_uctl(v4l2_dev_t vd, __u32 id, __s32 val)
{
	struct v4l2_dev *v = vd;
	struct v4l2_control ctl;

	ctl.id    = id;
	ctl.value = val;
	if (-1 == v4l2_ioctl (v->fd, VIDIOC_S_CTRL, &ctl)) {
        fprintf(stderr, "VIDIOC_S_CTRL: %s: id = 0x%x, val = 0x%x\n", strerror(errno), id, val);
        return -1;
    }

	return 0;
}

int v4l2_set_uctls2def(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    __u32 nr = v->uctls->nr;
    __u32 id;
    __s32 def_val;
    int i;
    for (i = 0; i < nr; i++) {
        id      = v->uctls->list[i].id;
        def_val = v->uctls->list[i].def_val;
        if (v4l2_set_uctl(v, id, def_val) < 0)
            return -1;
    }
    return 0;
}

__s32 v4l2_get_uctl(v4l2_dev_t vd, __u32 id, bool *ok)
{
	struct v4l2_dev *v = vd;
	struct v4l2_control ctl;

    *ok = false;
	ctl.id = id;
	if (-1 == v4l2_ioctl (v->fd, VIDIOC_G_CTRL, &ctl)) {
        pr_debug("id = %x", id);
        //perror("VIDIOC_G_CTRL");
        return -1;
    } 
    *ok = true;
    return ctl.value;
}

void v4l2_get_uctls(v4l2_dev_t vd, struct v4l2_uctl *uctls)
{
	struct v4l2_dev *v = vd;
    struct v4l2_uctl *pctl; 
    __s32 val;
    bool ok;
    int i;

    for (i = 0; i < v->uctls->nr; i++) {
        pctl = &v->uctls->list[i];
        val  = v4l2_get_uctl(v, pctl->id, &ok);
        if (ok)
            pctl->val = val;
    }
    memcpy(uctls, v->uctls->list, v->uctls->nr * sizeof(struct v4l2_uctl));
}

__u32 v4l2_get_uctls_nr(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    return v->uctls->nr;
}

static int v4l2_mmap_setup(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
	struct v4l2_requestbuffers req;
	int i; 

    memset(&req, 0, sizeof(req));
	req.count  = NR_REQBUF;    		/* 缓存队列长度 */
	req.type   = v->ffmts[v->cur_fmt].fmt.type;
	req.memory = V4L2_MEMORY_MMAP;
	
	if (-1 == xioctl(v->fd, VIDIOC_REQBUFS, &req)) {
        perror("VIDIOC_REQBUFS");
        return -1;
    }

	if (req.count < 2) {
		pr_debug("Insufficient buffer memory\n");
		return -1;		
	}

	v->buf = calloc(req.count, sizeof(struct buf));
	if (!v->buf) {
		pr_debug("Out of memory\n");
		return -1;	
	}
	
	for (i = 0; i < req.count; i++) {
		struct v4l2_buffer buf;
		bzero(&buf, sizeof(buf));

		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;

		if (-1 == xioctl(v->fd, VIDIOC_QUERYBUF, &buf)) {
            perror("VIDIOC_QUERYBUF");
            goto err_calloc;
        }

		v->buf[i].len   = buf.length;
		v->buf[i].start = v4l2_mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, v->fd, buf.m.offset);

		if (MAP_FAILED == v->buf[i].start) {
            perror("mmap");
            goto err_calloc;
        }
	}	
	v->buf_nr = req.count;
	return 0;

err_calloc:
    free(v->buf);
    return -1;
}

static int v4l2_mmap_free(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    int i;
	for (i = 0; i < v->buf_nr; i++) {
		if (-1 == v4l2_munmap(v->buf[i].start, v->buf[i].len)) {
            perror("munmap");
            return -1;
        }
	}
    free(v->buf);
    return 0;
}

static int v4l2_fmt_setup(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    struct v4l2_fmtdesc fmt, *pfmt;
    struct v4l2_fmtdesc fmts[MAX_FMT_TYPE_NR];
    struct v4l2_frmsizeenum frm, *pfrm;
    struct v4l2_frmsizeenum frms[MAX_FMT_TYPE_NR][MAX_FRM_SIZ_NR];
    int frms_nrs[MAX_FMT_TYPE_NR];
    int ret, siz, i;

    memset(&fmt, 0, sizeof(fmt));
    memset(fmts, 0, sizeof(fmts));
    memset(frms, 0, sizeof(frms));

    pfmt = &fmt;
    pfrm = &frm;
    pfmt->index = 0;
    pfmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (;;) {
        ret = v4l2_ioctl(v->fd, VIDIOC_ENUM_FMT, pfmt);
        if (ret < 0) {
            if (errno == EINVAL)
                break;
            perror("VIDIOC_ENUM_FMT");
            return -1;
        }
        memcpy(&fmts[pfmt->index], pfmt, sizeof(*pfmt));
        bzero(pfrm, sizeof(*pfrm));
        pfrm->index = 0;
        pfrm->type = V4L2_FRMSIZE_TYPE_DISCRETE;
        pfrm->pixel_format = pfmt->pixelformat;
        for (;;) {
            ret = v4l2_ioctl(v->fd, VIDIOC_ENUM_FRAMESIZES, pfrm);
            if (ret < 0) {
                if (errno == EINVAL)
                    break;
                perror("VIDIOC_ENUM_FRAMESIZES");
                return -1;
            }

            memcpy(&frms[pfmt->index][pfrm->index], pfrm, sizeof(*pfrm));
            pfrm->index++;
        }
        frms_nrs[pfmt->index] = pfrm->index;  
        pfmt->index++;
    }

    v->ffmts_nr = pfmt->index;
    v->ffmts = malloc(sizeof(struct v4l2_frms_fmt) * v->ffmts_nr);
    if (v->ffmts == NULL) {
        perror("malloc ffmts");
        return -1;
    }

    for (i = 0; i < v->ffmts_nr; i++) {
        v->ffmts[i].fmt = fmts[i];
        v->ffmts[i].frms_nr = frms_nrs[i];
        siz = sizeof(struct v4l2_frmsizeenum) * (v->ffmts[i].frms_nr);
        v->ffmts[i].frms = malloc(siz);
        if (v->ffmts[i].frms == NULL) {
            perror("malloc ffmts.frms");
            goto err_mem;
        }
        memcpy(v->ffmts[i].frms, frms[i], siz);
    }

#ifdef DBG_V4L
    int j;
    struct v4l2_frms_fmt *pffmt;
    pr_debug("support %d pixmap formats:\n", v->ffmts_nr);
    for (i = 0; i < v->ffmts_nr; i++) {
        pffmt = &v->ffmts[i];
        pr_debug("  format %d<%s>0x%x, support %d dimensions:\n", 
                 pffmt->fmt.index, pffmt->fmt.description, pffmt->fmt.pixelformat, pffmt->frms_nr);

        for (j = 0; j < pffmt->frms_nr; j++) {
            pfrm = &pffmt->frms[j];
            pr_debug("   dimension %d: %d x %d\n", pfrm->index, 
                     pfrm->discrete.width, pfrm->discrete.height);
        }
    }
#endif
	return 0;

err_mem:
    free(v->ffmts);
    for (i = i - 1; i > -1; i--) 
        free(v->ffmts[i].frms);
    return -1;
}

static inline void v4l2_fmt_free(v4l2_dev_t vd) {
	struct v4l2_dev *v = vd;
    int i;
    free(v->ffmts);
    for (i = 0; i < v->ffmts_nr; i++) 
        free(v->ffmts[i].frms);
}

int v4l2_set_fmt(v4l2_dev_t vd, __u32 fmt_nr, __u32 frm_nr)
{
	struct v4l2_dev *v = vd;
 	struct v4l2_format fmt;
    unsigned int min;

    if (fmt_nr >= v->ffmts_nr || frm_nr >= v->ffmts[fmt_nr].frms_nr) {
        pr_debug("invalid arguments: fmt_nr = %u(max: %u), frm_nr = %u(max: %u)\n",
                  fmt_nr, v->ffmts_nr, frm_nr, v->ffmts[fmt_nr].frms_nr);
        return -1;
    }

    bzero(&fmt, sizeof(fmt));
	fmt.type                = v->ffmts[fmt_nr].fmt.type;
	fmt.fmt.pix.pixelformat = v->ffmts[fmt_nr].fmt.pixelformat;
	fmt.fmt.pix.width       = v->ffmts[fmt_nr].frms[frm_nr].discrete.width; 
	fmt.fmt.pix.height      = v->ffmts[fmt_nr].frms[frm_nr].discrete.height;
	
	if (-1 == xioctl (v->fd, VIDIOC_S_FMT, &fmt)) {
        perror("VIDIOC_S_FMT");
        return -1;
    }
	v->cur_fmt = fmt_nr;
	v->cur_frm = frm_nr;

  	if (-1 == xioctl (v->fd, VIDIOC_G_FMT, &fmt)) {
        perror("VIDIOC_G_FMT");
        return -1;
    }  

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    return 0; 
}

__u32 v4l2_get_fmts_nr(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    return v->ffmts_nr;
}

__u32 v4l2_get_fmt_frms_nr(v4l2_dev_t vd, __u32 fmt_nr)
{
	struct v4l2_dev *v = vd;
    if (fmt_nr >= v->ffmts_nr) {
        pr_debug("invalid arguments: fmt_nr = %u(max: %u))\n",
                  fmt_nr, v->ffmts_nr);
        return -1;
    }
    return v->ffmts[fmt_nr].frms_nr;
}

int v4l2_get_fmt(v4l2_dev_t vd, __u32 fmt_nr, struct v4l2_fmtdesc *fmt)
{
	struct v4l2_dev *v = vd;
    if (fmt_nr >= v->ffmts_nr) {
        pr_debug("invalid arguments: fmt_nr = %u(max: %u))\n",
                  fmt_nr, v->ffmts_nr);
        return -1;
    }
    memcpy(fmt, &v->ffmts[fmt_nr].fmt, sizeof(struct v4l2_fmtdesc));
    return 0;
}

int v4l2_get_format(v4l2_dev_t vd, struct v4l2_format *fmt)
{
	struct v4l2_dev *v = vd;
  	if (-1 == xioctl (v->fd, VIDIOC_G_FMT, fmt)) {
        perror("VIDIOC_G_FMT");
        return -1;
    }  
    return 0;
}

int v4l2_set_format(v4l2_dev_t vd, struct v4l2_format *fmt)
{
	struct v4l2_dev *v = vd;
  	if (-1 == xioctl (v->fd, VIDIOC_S_FMT, fmt)) {
        perror("VIDIOC_S_FMT");
        return -1;
    }  
    return 0;
}

int v4l2_get_frmsize(v4l2_dev_t vd, 
                     __u32 fmt_nr, __u32 frm_nr, 
                     struct v4l2_frmsizeenum *frm)
{
	struct v4l2_dev *v = vd;
    if (fmt_nr >= v->ffmts_nr || frm_nr >= v->ffmts[fmt_nr].frms_nr) {
        pr_debug("invalid arguments: fmt_nr = %u(max: %u), frm_nr = %u(max: %u)\n",
                  fmt_nr, v->ffmts_nr, frm_nr, v->ffmts[fmt_nr].frms_nr);
        return -1;
    }
    memcpy(frm, &v->ffmts[fmt_nr].frms[frm_nr], sizeof(struct v4l2_frmsizeenum));
    return 0;
}

__u32 v4l2_get_cur_fmt_nr(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    return v->cur_fmt;
}

__u32 v4l2_get_cur_frm_nr(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    return v->cur_frm;
}

static int v4l2_init(v4l2_dev_t vd, __u32 fmt_nr, __u32 frm_nr) 
{
	struct v4l2_dev *v = vd;
 	struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;


	if (-1 == v4l2_ioctl(v->fd, VIDIOC_QUERYCAP, &cap)) {
        perror("VIDIOC_QUERYCAP");
        return -1;
    }

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		pr_debug("%s is no video capture device\n", v->name);
		return -1;
	}	

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        pr_debug("%s does not support streaming i/o\n", v->name);
        return -1;
    }
	strcpy((char*)v->name, (char*)cap.card);
	strcpy((char*)v->drv, (char*)cap.driver);
    pr_debug("name: %s, drv: %s\n", v->name, v->drv);

    bzero(&cropcap, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == xioctl(v->fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        if (-1 == xioctl(v->fd, VIDIOC_S_CROP, &crop)) {
            perror("VIDIOC_S_CROP");
            //return -1; ???
        } else {
            pr_debug("set crop to cropcap.defrect:\n"
                     "left(%d), top(%d), width(%d), height(%d)\n",
                     crop.c.left, crop.c.top, crop.c.width, crop.c.height);
        }
    } else {
        pr_debug("%s does not support cropcap\n", v->name);
        /* Errors ignored. */
    }

	if (-1 == v4l2_uctl_setup(v)) 
		return -1;

	if (-1 == v4l2_fmt_setup(v)) 
		goto err_uctl;	

	if (-1 == v4l2_set_fmt(v, fmt_nr, frm_nr)) 
		goto err_fmt;	
	
	if (-1 == v4l2_mmap_setup(v)) 
		goto err_fmt;	

    return 0;

err_fmt:
    v4l2_fmt_free(v);
err_uctl:
    v4l2_uctl_free(v);
    return -1;
}

static int v4l2_init2(v4l2_dev_t vd, struct v4l2_format *fmt) 
{
	struct v4l2_dev *v = vd;
 	struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

	if (-1 == v4l2_ioctl(v->fd, VIDIOC_QUERYCAP, &cap)) {
        perror("VIDIOC_QUERYCAP");
        return -1;
    }

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		pr_debug("%s is no video capture device\n", v->name);
		return -1;
	}	

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        pr_debug("%s does not support streaming i/o\n", v->name);
        return -1;
    }
	strcpy((char*)v->name, (char*)cap.card);
	strcpy((char*)v->drv, (char*)cap.driver);
    pr_debug("name: %s, drv: %s\n", v->name, v->drv);

    bzero(&cropcap, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == xioctl(v->fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        if (-1 == xioctl(v->fd, VIDIOC_S_CROP, &crop)) {
            perror("VIDIOC_S_CROP");
            //return -1; ???
        } else {
            pr_debug("set crop to cropcap.defrect:\n"
                     "left(%d), top(%d), width(%d), height(%d)\n",
                     crop.c.left, crop.c.top, crop.c.width, crop.c.height);
        }
    } else {
        pr_debug("%s does not support cropcap\n", v->name);
        /* Errors ignored. */
    }

	if (-1 == v4l2_uctl_setup(v)) 
		return -1;

	if (-1 == v4l2_fmt_setup(v)) 
		goto err_uctl;	

	if (-1 == v4l2_set_format(v, fmt)) 
		goto err_fmt;	
	
	if (-1 == v4l2_mmap_setup(v)) 
		goto err_fmt;	

    return 0;

err_fmt:
    v4l2_fmt_free(v);
err_uctl:
    v4l2_uctl_free(v);
    return -1;
}

/*
 * 反初始化设备
 * */
static inline int v4l2_uninit(v4l2_dev_t vd) {	
    v4l2_fmt_free(vd);
    v4l2_uctl_free(vd);
    return v4l2_mmap_free(vd);
}

void v4l2_process(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
	struct v4l2_buffer buf;
    int r;

    //tst_print_and_start(v->t, "v4l2_app_handler start");
    buf.type   = v->ffmts[v->cur_fmt].fmt.type;
    buf.memory = V4L2_MEMORY_MMAP;	
    /* 从队列中取出一个buf */
	do {
		r = xioctl (v->fd, VIDIOC_DQBUF, &buf);
	} while (-1 == r && EAGAIN == errno);

    if (-1 == r) {
        perror("VIDIOC_DQBUF");
        exit(EXIT_FAILURE);
    }	

    /* 执行回调函数 */
    if (v->proc)
        v->proc(v->buf[buf.index].start, buf.bytesused, v->arg);

    /* 送回队列 */
	do {
		r = xioctl (v->fd, VIDIOC_QBUF, &buf);
	} while (-1 == r && EAGAIN == errno);

    if (-1 == r) {
        perror("VIDIOC_QBUF");
        exit(EXIT_FAILURE);
    }	

    //tst_print_and_start(v->t, "v4l2_app_handler end");
}

/**
 * @fmt 只能是:
 * V4L2_PIX_FMT_RGB24
 * V4L2_PIX_FMT_BGR24
 * V4L2_PIX_FMT_YUV420
 * V4L2_PIX_FMT_YVU420
 */
v4l2_dev_t v4l2_create2(const char *dev, struct v4l2_format *fmt) 
{
    struct v4l2_dev *v = calloc(1, sizeof(struct v4l2_dev));
    if (!v) {
		perror("v4l2_create");
		return NULL;
	}

	if (NULL == dev)
		dev = DEF_V4L_DEV;

    if (-1 == (v->fd = v4l2_open(dev, O_RDWR | O_NONBLOCK))) {
        perror(dev);
        goto err_mem;
    }

    if (-1 == (v4l2_init2(v, fmt))) 
        goto err_open;

    //v->t = tst_create();

	return v;
//err_init:
  //  v4l2_uninit(v);
err_open:
    v4l2_close(v->fd);
err_mem:
    free(v);
    return NULL;
}

v4l2_dev_t v4l2_create(const char *dev, __u32 fmt_nr, __u32 frm_nr) 
{
    struct v4l2_dev *v = calloc(1, sizeof(struct v4l2_dev));
    if (!v) {
		perror("v4l2_create");
		return NULL;
	}

	if (NULL == dev)
		dev = DEF_V4L_DEV;

    if (-1 == (v->fd = v4l2_open(dev, O_RDWR))) {
        perror(dev);
        goto err_mem;
    }

    if (-1 == (v4l2_init(v, fmt_nr, frm_nr))) 
        goto err_open;

	return v;
err_open:
    v4l2_close(v->fd);
err_mem:
    free(v);
    return NULL;
}

void v4l2_free(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    v4l2_uninit(v);
    v4l2_close(v->fd);
    free(v);
}

v4l2_img_proc_t 
v4l2_set_img_proc(v4l2_dev_t vd, v4l2_img_proc_t proc, void *arg)
{
	struct v4l2_dev *v = vd;
    v4l2_img_proc_t old;
    old = v->proc;
    v->proc = proc;
    v->arg = arg;
    return old;
}

/*
 * 开启捕获图形，完成以下工作：
 * 1.将缓冲区加入队列
 * 2.启动捕获
 */
int v4l2_start_capture(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
    enum v4l2_buf_type type;
    struct v4l2_buffer buf;
	int i;

	for (i = 0; i < v->buf_nr; i++) {

		memset(&buf, 0, sizeof(buf));
		buf.type   = v->ffmts[v->cur_fmt].fmt.type;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;

		if (-1 == xioctl(v->fd, VIDIOC_QBUF, &buf)) {
            perror("VIDIOC_QBUF");
            return -1;
        }
    }
	
	type = buf.type;
	if (-1 == xioctl (v->fd, VIDIOC_STREAMON, &type)) {
        perror("VIDIOC_STREAMON");
        return -1;
    }	
    return 0;
}

int v4l2_stop_capture(v4l2_dev_t vd)
{
	struct v4l2_dev *v = vd;
	enum v4l2_buf_type type;
	type = v->ffmts[v->cur_fmt].fmt.type;
	if (-1 == xioctl(v->fd, VIDIOC_STREAMOFF, &type)) {
        perror("VIDIOC_STREAMOFF");
        return -1;
    }	
	return 0;
}

