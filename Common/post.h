#ifndef __SAMSUNG_SYSLSI_APDEV_S3C_POST_H__
#define __SAMSUNG_SYSLSI_APDEV_S3C_POST_H__

#define PP_IOCTL_MAGIC 'P'
#define VERSION "v0.1"
#if 0
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
#endif
#if 1  //phantom
#define S3C_PP_SET_PARAMS			        _IO(PP_IOCTL_MAGIC, 0)
#define S3C_PP_START					    _IO(PP_IOCTL_MAGIC, 1)
#define S3C_PP_GET_SRC_BUF_SIZE  		    _IO(PP_IOCTL_MAGIC, 2)
#define S3C_PP_SET_SRC_BUF_ADDR_PHY         _IO(PP_IOCTL_MAGIC, 3)
#define S3C_PP_SET_SRC_BUF_NEXT_ADDR_PHY    _IO(PP_IOCTL_MAGIC, 4)
#define S3C_PP_GET_DST_BUF_SIZE  		    _IO(PP_IOCTL_MAGIC, 5)
#define S3C_PP_SET_DST_BUF_ADDR_PHY	        _IO(PP_IOCTL_MAGIC, 6)
#define S3C_PP_ALLOC_KMEM                   _IO(PP_IOCTL_MAGIC, 7)
#define S3C_PP_FREE_KMEM                    _IO(PP_IOCTL_MAGIC, 8)
#define S3C_PP_GET_RESERVED_MEM_SIZE        _IO(PP_IOCTL_MAGIC, 9)
#define S3C_PP_GET_RESERVED_MEM_ADDR_PHY    _IO(PP_IOCTL_MAGIC, 10)
#endif


#define PP_DEV_NAME		"/dev/s3c-pp"



typedef enum {
	POST_DMA, POST_FIFO
} s3c_pp_path_t;

typedef enum {
	ONE_SHOT, FREE_RUN
} s3c_pp_run_mode_t;
#if 0
typedef enum {
	INTERLACE_MODE,
	PROGRESSIVE_MODE
} s3c_pp_scan_mode_t;
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
#endif


#if 1  //phantom
typedef enum {
	DMA_ONESHOT,
    FIFO_FREERUN
} s3c_pp_out_path_t;

typedef enum {
	PAL1, PAL2, PAL4, PAL8,
	RGB8, ARGB8, RGB16, ARGB16, RGB18, RGB24, RGB30, ARGB24,
	YC420, YC422, // Non-interleave
	CRYCBY, CBYCRY, YCRYCB, YCBYCR, YUV444 // Interleave
} s3c_color_space_t;

typedef enum {
	INTERLACE_MODE,
	PROGRESSIVE_MODE
} s3c_pp_scan_mode_t;

typedef struct{
	unsigned int SrcFullWidth; 		// Source Image Full Width(Virtual screen size)
	unsigned int SrcFullHeight; 		// Source Image Full Height(Virtual screen size)
	unsigned int SrcStartX; 			// Source Image Start width offset
	unsigned int SrcStartY; 			// Source Image Start height offset
	unsigned int SrcWidth;			// Source Image Width
	unsigned int SrcHeight; 			// Source Image Height
	unsigned int SrcFrmSt; 		// Base Address of the Source Image : Physical Address
	unsigned int src_next_buf_addr_phy; // Base Address of Source Image to be displayed next time in FIFO_FREERUN Mode
	s3c_color_space_t SrcCSpace; 		// Color Space ot the Source Image

	unsigned int DstFullWidth; 		// Source Image Full Width(Virtual screen size)
	unsigned int DstFullHeight; 		// Source Image Full Height(Virtual screen size)
	unsigned int DstStartX; 			// Source Image Start width offset
	unsigned int DstStartY; 			// Source Image Start height offset
	unsigned int DstWidth; 			// Source Image Width
	unsigned int DstHeight; 			// Source Image Height
	unsigned int DstFrmSt; 			// Base Address of the Source Image : Physical Address
	s3c_color_space_t DstCSpace; 		// Color Space ot the Source Image

//	unsigned int SrcFrmBufNum; 		// Frame buffer number
//	s3c_pp_scan_mode_t Mode; 	// POST running mode(PER_FRAME or FREE_RUN)
//	s3c_pp_out_path_t InPath;
	s3c_pp_out_path_t OutPath; 	// Data path of the desitination image
	s3c_pp_scan_mode_t Mode;

//	unsigned int in_pixel_size;
//	unsigned int out_pixel_size;
}pp_params;


// Structure type for IOCTL commands S3C_PP_SET_PARAMS, S3C_PP_SET_INPUT_BUF_START_ADDR_PHY, 
//      S3C_PP_SET_INPUT_BUF_NEXT_START_ADDR_PHY, S3C_PP_SET_OUTPUT_BUF_START_ADDR_PHY.
typedef struct {
	unsigned int src_full_width;		// Source Image Full Width (Virtual screen size)
	unsigned int src_full_height;		// Source Image Full Height (Virtual screen size)
	unsigned int src_start_x; 			// Source Image Start width offset
	unsigned int src_start_y; 			// Source Image Start height offset
	unsigned int src_width;				// Source Image Width
	unsigned int src_height; 			// Source Image Height
	unsigned int src_buf_addr_phy; 		// Base Address of the Source Image : Physical Address
	unsigned int src_next_buf_addr_phy; // Base Address of Source Image to be displayed next time in FIFO_FREERUN Mode
	s3c_color_space_t src_color_space; 	// Color Space of the Source Image

	unsigned int dst_full_width; 		// Destination Image Full Width (Virtual screen size)
	unsigned int dst_full_height; 		// Destination Image Full Height (Virtual screen size)
	unsigned int dst_start_x; 			// Destination Image Start width offset
	unsigned int dst_start_y; 			// Destination Image Start height offset
	unsigned int dst_width; 			// Destination Image Width
	unsigned int dst_height; 			// Destination Image Height
	unsigned int dst_buf_addr_phy;		// Base Address of the Destination Image : Physical Address
	s3c_color_space_t dst_color_space; 	// Color Space of the Destination Image

	s3c_pp_out_path_t out_path; 	    // output and run mode (DMA_ONESHOT or FIFO_FREERUN)
    s3c_pp_scan_mode_t scan_mode;       // INTERLACE_MODE, PROGRESSIVE_MODE
} s3c_pp_params_t;
#endif

typedef struct{
	unsigned int pre_phy_addr;
	unsigned char *pre_virt_addr;

	unsigned int post_phy_addr;
	unsigned char *post_virt_addr;
} buff_addr_t;

#endif //__SAMSUNG_SYSLSI_APDEV_S3C_POST_H__
