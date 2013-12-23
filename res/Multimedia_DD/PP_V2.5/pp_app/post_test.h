#ifndef __S3CTVSCALER_H_
#define __S3CTVSCALER_H_

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

#define QVGA_XSIZE      320
#define QVGA_YSIZE      240

#define LCD_XSIZE       320
#define LCD_YSIZE       240

#define PP_MINOR  253                 // Just some number


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

typedef struct{
	unsigned int SrcFullWidth; 		// Source Image Full Width(Virtual screen size)
	unsigned int SrcFullHeight; 		// Source Image Full Height(Virtual screen size)
	unsigned int SrcStartX; 			// Source Image Start width offset
	unsigned int SrcStartY; 			// Source Image Start height offset
	unsigned int SrcWidth;			// Source Image Width
	unsigned int SrcHeight; 			// Source Image Height
	unsigned int SrcFrmSt; 			// Base Address of the Source Image : Physical Address
	cspace_t SrcCSpace; 		// Color Space ot the Source Image

	unsigned int DstFullWidth; 		// Source Image Full Width(Virtual screen size)
	unsigned int DstFullHeight; 		// Source Image Full Height(Virtual screen size)
	unsigned int DstStartX; 			// Source Image Start width offset
	unsigned int DstStartY; 			// Source Image Start height offset
	unsigned int DstWidth; 			// Source Image Width
	unsigned int DstHeight; 			// Source Image Height
	unsigned int DstFrmSt; 			// Base Address of the Source Image : Physical Address
	cspace_t DstCSpace; 		// Color Space ot the Source Image

	unsigned int SrcFrmBufNum; 		// Frame buffer number
	s3c_pp_run_mode_t Mode; 	// POST running mode(PER_FRAME or FREE_RUN)
	s3c_pp_path_t InPath;
	s3c_pp_path_t OutPath; 	// Data path of the desitination image

	unsigned int in_pixel_size;
	unsigned int out_pixel_size;
}pp_params;


typedef struct{
	unsigned int pre_phy_addr;
	unsigned char *pre_virt_addr;

	unsigned int post_phy_addr;
	unsigned char *post_virt_addr;
} buff_addr_t;

#endif //__S3CTVSCALER_H_
