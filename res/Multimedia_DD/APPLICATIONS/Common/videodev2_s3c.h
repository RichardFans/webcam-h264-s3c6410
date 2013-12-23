#include <linux/videodev2.h>

#define V4L2_INPUT_TYPE_MSDMA		3
#define V4L2_INPUT_TYPE_INTERLACE	4
/****************************************************************
* struct v4l2_control
* Control IDs defined by S3C
*****************************************************************/

/* Image Effect */
#define V4L2_CID_ORIGINAL		(V4L2_CID_PRIVATE_BASE+0)
#define V4L2_CID_ARBITRARY		(V4L2_CID_PRIVATE_BASE+1)
#define V4L2_CID_NEGATIVE 		(V4L2_CID_PRIVATE_BASE+2)
#define V4L2_CID_ART_FREEZE		(V4L2_CID_PRIVATE_BASE+3)
#define V4L2_CID_EMBOSSING		(V4L2_CID_PRIVATE_BASE+4)
#define V4L2_CID_SILHOUETTE		(V4L2_CID_PRIVATE_BASE+5)

/* Image Rotate */
#define V4L2_CID_ROTATE_90		(V4L2_CID_PRIVATE_BASE+6)
#define V4L2_CID_ROTATE_180		(V4L2_CID_PRIVATE_BASE+7)
#define V4L2_CID_ROTATE_270		(V4L2_CID_PRIVATE_BASE+8)
#define V4L2_CID_ROTATE_BYPASS		(V4L2_CID_PRIVATE_BASE+9)

/* Zoom-in, Zoom-out */
#define	V4L2_CID_ZOOMIN			(V4L2_CID_PRIVATE_BASE+10)
#define V4L2_CID_ZOOMOUT		(V4L2_CID_PRIVATE_BASE+11)





/****************************************************************
*	I O C T L   C O D E S   F O R   V I D E O   D E V I C E S
*    	 It's only for S3C
*****************************************************************/
#define VIDIOC_S_CAMERA_START 		_IO ('V', BASE_VIDIOC_PRIVATE+0)
#define VIDIOC_S_CAMERA_STOP		_IO ('V', BASE_VIDIOC_PRIVATE+1)


/*
 *	M S D M A   I N P U T   F O R M A T
 */

enum v4l2_msdma_input {
	V4L2_MSDMA_CODEC             = 1,
	V4L2_MSDMA_PREVIEW           = 2,
};


struct v4l2_msdma_format
{
	__u32         		width;		/* MSDMA INPUT : Source X size */
	__u32			height;		/* MSDMA INPUT : Source Y size */
	__u32			pixelformat;
	enum v4l2_msdma_input  	input_path;
};

#define VIDIOC_MSDMA_START		_IOW ('V', BASE_VIDIOC_PRIVATE+2, struct v4l2_msdma_format)
#define VIDIOC_MSDMA_STOP		_IOW ('V', BASE_VIDIOC_PRIVATE+3, struct v4l2_msdma_format)
#define VIDIOC_S_MSDMA     		_IOW  ('V', BASE_VIDIOC_PRIVATE+4, struct v4l2_msdma_format)

#define VIDIOC_S_INTERLACE_MODE		_IOWR ('V', BASE_VIDIOC_PRIVATE+5, int)
