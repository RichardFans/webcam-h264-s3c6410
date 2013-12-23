#ifndef _S3C_PP_COMMON_H_
#define _S3C_PP_COMMON_H_

#define PP_IOCTL_MAGIC 'P'

#define PPROC_SET_PARAMS			_IO(PP_IOCTL_MAGIC, 0)
#define PPROC_START					_IO(PP_IOCTL_MAGIC, 1)
#define PPROC_STOP					_IO(PP_IOCTL_MAGIC, 2)
#define PPROC_INTERLACE_MODE		_IO(PP_IOCTL_MAGIC, 3)
#define PPROC_PROGRESSIVE_MODE		_IO(PP_IOCTL_MAGIC, 4)
#define PPROC_GET_PHY_INBUF_ADDR	_IO(PP_IOCTL_MAGIC, 5)
#define PPROC_GET_INBUF_SIZE		_IO(PP_IOCTL_MAGIC, 6)
#define PPROC_GET_BUF_SIZE			_IO(PP_IOCTL_MAGIC, 7)
#define PPROC_START_ONLY			_IO(PP_IOCTL_MAGIC, 8)
#define PPROC_GET_OUT_DATA_SIZE		_IO(PP_IOCTL_MAGIC, 9)

#define PP_MINOR  				253		// post processor is misc device driver


typedef enum {
	INTERLACE_MODE,
	PROGRESSIVE_MODE
} s3c_pp_scan_mode_t;

typedef enum {
	POST_DMA, POST_FIFO
} s3c_pp_path_t;

typedef enum {
	ONE_SHOT, FREE_RUN
} s3c_pp_run_mode_t;

typedef enum {
	PAL1, PAL2, PAL4, PAL8,
	RGB8, ARGB8, RGB16, ARGB16, RGB18, RGB24, RGB30, ARGB24,
	YC420, YC422, // Non-interleave
	CRYCBY, CBYCRY, YCRYCB, YCBYCR, YUV444 // Interleave
} cspace_t;

typedef enum
{
	HCLK = 0, PLL_EXT = 1, EXT_27MHZ = 3
} pp_clk_src_t;

typedef enum
{
	POST_IDLE = 0,
	POST_BUSY
} s3c_pp_state_t;

typedef struct{
	unsigned int SrcFullWidth;			// Source Image Full Width(Virtual screen size)
	unsigned int SrcFullHeight;			// Source Image Full Height(Virtual screen size)
	unsigned int SrcStartX; 			// Source Image Start width offset
	unsigned int SrcStartY; 			// Source Image Start height offset
	unsigned int SrcWidth;				// Source Image Width
	unsigned int SrcHeight; 			// Source Image Height
	unsigned int SrcFrmSt; 				// Base Address of the Source Image : Physical Address
	cspace_t SrcCSpace; 				// Color Space ot the Source Image

	unsigned int DstFullWidth; 			// Source Image Full Width(Virtual screen size)
	unsigned int DstFullHeight; 		// Source Image Full Height(Virtual screen size)
	unsigned int DstStartX; 			// Source Image Start width offset
	unsigned int DstStartY; 			// Source Image Start height offset
	unsigned int DstWidth; 				// Source Image Width
	unsigned int DstHeight; 			// Source Image Height
	unsigned int DstFrmSt; 				// Base Address of the Source Image : Physical Address
	cspace_t DstCSpace; 				// Color Space ot the Source Image

	unsigned int SrcFrmBufNum; 			// Frame buffer number
	s3c_pp_run_mode_t Mode; 			// POST running mode(PER_FRAME or FREE_RUN)
	s3c_pp_path_t InPath;				// Data path of the source image
	s3c_pp_path_t OutPath; 				// Data path of the desitination image

	unsigned int in_pixel_size;			// source format size per pixel
	unsigned int out_pixel_size;		// destination format size per pixel
}pp_params;

typedef struct{
	unsigned int pre_h_ratio, pre_v_ratio, h_shift, v_shift, sh_factor;
	unsigned int pre_dst_width, pre_dst_height, dx, dy;
} scaler_info_t;

typedef struct{
	unsigned int offset_y, offset_cb, offset_cr;
	unsigned int src_start_y, src_start_cb, src_start_cr;
	unsigned int src_end_y, src_end_cb, src_end_cr;
	unsigned int start_pos_y, end_pos_y;
	unsigned int start_pos_cb, end_pos_cb;
	unsigned int start_pos_cr, end_pos_cr;
	unsigned int start_pos_rgb, end_pos_rgb;
	unsigned int dst_start_rgb, dst_end_rgb;
	unsigned int src_frm_start_addr;

	unsigned int offset_rgb, out_offset_cb, out_offset_cr;
	unsigned int out_start_pos_cb, out_start_pos_cr;
	unsigned int out_end_pos_cb, out_end_pos_cr;
	unsigned int out_src_start_cb, out_src_start_cr;
	unsigned int out_src_end_cb, out_src_end_cr;
} buf_addr_t;

// This function is used for the MFC only
extern void pp_for_mfc(unsigned int uSrcFrmSt,     unsigned int uDstFrmSt,
			   unsigned int srcFrmFormat,  unsigned int dstFrmFormat,
               unsigned int srcFrmWidth,   unsigned int srcFrmHeight,
               unsigned int dstFrmWidth,   unsigned int dstFrmHeight,
               unsigned int srcXOffset,    unsigned int srcYOffset,
               unsigned int dstXOffset,    unsigned int dstYOffset,
               unsigned int srcCropWidth,  unsigned int srcCropHeight,
               unsigned int dstCropWidth,  unsigned int dstCropHeight
);

// below functions are used for Post Processor commonly
void set_data_format(pp_params *pParams);
void set_buf_addr(pp_params *pParams);
void set_scaler(pp_params *pParams);

// below functions'body is implemented in each post processor IP file(ex. s3c_pp_6400.c)
void set_scaler_register(scaler_info_t *scaler_info, pp_params *pParams);
void set_buf_addr_register(buf_addr_t *buf_addr, pp_params *pParams);
void set_data_format_register(pp_params *pParams);
int get_out_data_size(pp_params *pParams);
s3c_pp_state_t post_get_processing_state(void);

#endif // _S3C_PP_COMMON_H_

