
/*
 * linux/drivers/video/s3c_pp_common.c
 *
 * Revision 1.0  
 *
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    S3C PostProcessor driver 
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/errno.h> /* error codes */
#include <asm/div64.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/arch/map.h>
#include <linux/miscdevice.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
#include <linux/config.h>
#include <asm/arch/registers.h>
#else
#include <asm/arch/regs-pp.h>
#endif

#include "s3c_pp_common.h"

#define PFX "s3c_pp"

// setting the source/destination color space
void set_data_format(pp_params *pParams)
{
	// set the source color space
	switch(pParams->SrcCSpace) {
		case YC420:
			pParams->in_pixel_size	= 1;
			break;
		case YCBYCR:
			pParams->in_pixel_size	= 2;
			break;
		case YCRYCB:
			pParams->in_pixel_size	= 2;
			break;
		case CBYCRY:
			pParams->in_pixel_size	= 2;
			break;
		case CRYCBY:
			pParams->in_pixel_size	= 2;
			break;
		case RGB24:
			pParams->in_pixel_size	= 4;
			break;
		case RGB16:
			pParams->in_pixel_size	= 2;
			break;
		default:
			break;
	}

	// set the destination color space
	if(pParams->OutPath == POST_DMA) {
		switch(pParams->DstCSpace) {
			case YC420:
				pParams->out_pixel_size	= 1;
				break;
			case YCBYCR:
				pParams->out_pixel_size	= 2;
				break;
			case YCRYCB:
				pParams->out_pixel_size	= 2;
				break;
			case CBYCRY:
				pParams->out_pixel_size	= 2;
				break;
			case CRYCBY:
				pParams->out_pixel_size	= 2;
				break;
			case RGB24:
				pParams->out_pixel_size	= 4;
				break;
			case RGB16:
				pParams->out_pixel_size	= 2;
				break;
			default:
				break;
		}
	}
	else if(pParams->OutPath == POST_FIFO) 
	{
		if(pParams->DstCSpace == RGB30) 
		{
			pParams->out_pixel_size	= 4;
		} 
		else if(pParams->DstCSpace == YUV444) 
		{
			pParams->out_pixel_size	= 4;
		} 
	}

	// setting the register about src/dst data format
	set_data_format_register(pParams);
}


// setting the source/destination buffer address
void set_buf_addr(pp_params *pParams)
{
	buf_addr_t	buf_addr;


	buf_addr.offset_y			= (pParams->SrcFullWidth - pParams->SrcWidth) * pParams->in_pixel_size;
	buf_addr.start_pos_y		= (pParams->SrcFullWidth*pParams->SrcStartY+pParams->SrcStartX)*pParams->in_pixel_size;
	buf_addr.end_pos_y			= pParams->SrcWidth*pParams->SrcHeight*pParams->in_pixel_size + buf_addr.offset_y*(pParams->SrcHeight-1);
	buf_addr.src_frm_start_addr	= pParams->SrcFrmSt;
	buf_addr.src_start_y		= pParams->SrcFrmSt + buf_addr.start_pos_y;
	buf_addr.src_end_y			= buf_addr.src_start_y + buf_addr.end_pos_y;


	if(pParams->SrcCSpace == YC420) {
		buf_addr.offset_cb		= buf_addr.offset_cr = ((pParams->SrcFullWidth - pParams->SrcWidth) / 2) * pParams->in_pixel_size;
		buf_addr.start_pos_cb	= pParams->SrcFullWidth * pParams->SrcFullHeight * 1	\
								+ (pParams->SrcFullWidth * pParams->SrcStartY / 2 + pParams->SrcStartX) /2 * 1;

		buf_addr.end_pos_cb		= pParams->SrcWidth/2*pParams->SrcHeight/2*pParams->in_pixel_size \
					 			+ (pParams->SrcHeight/2 -1)*buf_addr.offset_cb;
		buf_addr.start_pos_cr	= pParams->SrcFullWidth * pParams->SrcFullHeight *1 \
					   			+ pParams->SrcFullWidth*pParams->SrcFullHeight/4 *1 \
					   			+ (pParams->SrcFullWidth*pParams->SrcStartY/2 + pParams->SrcStartX)/2*1;
		buf_addr.end_pos_cr		= pParams->SrcWidth/2*pParams->SrcHeight/2*pParams->in_pixel_size \
					 			+ (pParams->SrcHeight/2-1)*buf_addr.offset_cr;

		buf_addr.src_start_cb	= pParams->SrcFrmSt + buf_addr.start_pos_cb;
		buf_addr.src_end_cb		= buf_addr.src_start_cb + buf_addr.end_pos_cb;

		buf_addr.src_start_cr	= pParams->SrcFrmSt + buf_addr.start_pos_cr;
		buf_addr.src_end_cr		= buf_addr.src_start_cr + buf_addr.end_pos_cr;

	}

	if(pParams->OutPath == POST_DMA) {
		buf_addr.offset_rgb		= (pParams->DstFullWidth - pParams->DstWidth)*pParams->out_pixel_size;
		buf_addr.start_pos_rgb	= (pParams->DstFullWidth*pParams->DstStartY + pParams->DstStartX)*pParams->out_pixel_size;
		buf_addr.end_pos_rgb	= pParams->DstWidth*pParams->DstHeight*pParams->out_pixel_size 	\
								+ buf_addr.offset_rgb*(pParams->DstHeight - 1);
		buf_addr.dst_start_rgb 	= pParams->DstFrmSt + buf_addr.start_pos_rgb;
		buf_addr.dst_end_rgb 	= buf_addr.dst_start_rgb + buf_addr.end_pos_rgb;

		if(pParams->DstCSpace == YC420) {
			buf_addr.out_offset_cb		= buf_addr.out_offset_cr = ((pParams->DstFullWidth - pParams->DstWidth)/2)*pParams->out_pixel_size;
			buf_addr.out_start_pos_cb 	= pParams->DstFullWidth*pParams->DstFullHeight*1 \
							  	 		+ (pParams->DstFullWidth*pParams->DstStartY/2 + pParams->DstStartX)/2*1;
			buf_addr.out_end_pos_cb 	= pParams->DstWidth/2*pParams->DstHeight/2*pParams->out_pixel_size \
							 			+ (pParams->DstHeight/2 -1)*buf_addr.out_offset_cr;

			buf_addr.out_start_pos_cr 	= pParams->DstFullWidth*pParams->DstFullHeight*1 \
							   			+ (pParams->DstFullWidth*pParams->DstFullHeight/4)*1 \
							   			+ (pParams->DstFullWidth*pParams->DstStartY/2 +pParams->DstStartX)/2*1;
			buf_addr.out_end_pos_cr 	= pParams->DstWidth/2*pParams->DstHeight/2*pParams->out_pixel_size \
							 			+ (pParams->DstHeight/2 -1)*buf_addr.out_offset_cb;

			buf_addr.out_src_start_cb 	= pParams->DstFrmSt + buf_addr.out_start_pos_cb;
			buf_addr.out_src_end_cb 	= buf_addr.out_src_start_cb + buf_addr.out_end_pos_cb;
			buf_addr.out_src_start_cr 	= pParams->DstFrmSt + buf_addr.out_start_pos_cr;
			buf_addr.out_src_end_cr 	= buf_addr.out_src_start_cr + buf_addr.out_end_pos_cr;

		}
	}

	// setting the register about src/dst buffer address
	set_buf_addr_register(&buf_addr, pParams);
}


// setting the scaling information(source/destination size)
void set_scaler(pp_params *pParams)
{
	scaler_info_t	scaler_info;

	
	if (pParams->SrcWidth >= (pParams->DstWidth<<6)) {
		printk("Out of PreScalar range !!!\n");
		return;
	}
	if(pParams->SrcWidth >= (pParams->DstWidth<<5)) {
		scaler_info.pre_h_ratio = 32;
		scaler_info.h_shift = 5;		
	} else if(pParams->SrcWidth >= (pParams->DstWidth<<4)) {
		scaler_info.pre_h_ratio = 16;
		scaler_info.h_shift = 4;		
	} else if(pParams->SrcWidth >= (pParams->DstWidth<<3)) {
		scaler_info.pre_h_ratio = 8;
		scaler_info.h_shift = 3;		
	} else if(pParams->SrcWidth >= (pParams->DstWidth<<2)) {
		scaler_info.pre_h_ratio = 4;
		scaler_info.h_shift = 2;		
	} else if(pParams->SrcWidth >= (pParams->DstWidth<<1)) {
		scaler_info.pre_h_ratio = 2;
		scaler_info.h_shift = 1;		
	} else {
		scaler_info.pre_h_ratio = 1;
		scaler_info.h_shift = 0;		
	}

	scaler_info.pre_dst_width = pParams->SrcWidth / scaler_info.pre_h_ratio;
	scaler_info.dx = (pParams->SrcWidth<<8) / (pParams->DstWidth<<scaler_info.h_shift);


	if (pParams->SrcHeight >= (pParams->DstHeight<<6)) {
		printk("Out of PreScalar range !!!\n");
		return;
	}
	if(pParams->SrcHeight >= (pParams->DstHeight<<5)) {
		scaler_info.pre_v_ratio = 32;
		scaler_info.v_shift = 5;		
	} else if(pParams->SrcHeight >= (pParams->DstHeight<<4)) {
		scaler_info.pre_v_ratio = 16;
		scaler_info.v_shift = 4;		
	} else if(pParams->SrcHeight >= (pParams->DstHeight<<3)) {
		scaler_info.pre_v_ratio = 8;
		scaler_info.v_shift = 3;		
	} else if(pParams->SrcHeight >= (pParams->DstHeight<<2)) {
		scaler_info.pre_v_ratio = 4;
		scaler_info.v_shift = 2;		
	} else if(pParams->SrcHeight >= (pParams->DstHeight<<1)) {
		scaler_info.pre_v_ratio = 2;
		scaler_info.v_shift = 1;		
	} else {
		scaler_info.pre_v_ratio = 1;
		scaler_info.v_shift = 0;		
	}	

	scaler_info.pre_dst_height = pParams->SrcHeight / scaler_info.pre_v_ratio;
	scaler_info.dy = (pParams->SrcHeight<<8) / (pParams->DstHeight<<scaler_info.v_shift);
	scaler_info.sh_factor = 10 - (scaler_info.h_shift + scaler_info.v_shift);


	// setting the register about scaling information
	set_scaler_register(&scaler_info, pParams);
}

int get_out_data_size(pp_params *pParams)
{
	switch(pParams->DstCSpace) {
		case YC420:
			return (pParams->DstFullWidth * pParams->DstFullHeight * 3) >> 1;
			break;
		case YCBYCR:
			return (pParams->DstFullWidth * pParams->DstFullHeight * 2);
			break;
		case YCRYCB:
			return (pParams->DstFullWidth * pParams->DstFullHeight * 2);
			break;
		case CBYCRY:
			return (pParams->DstFullWidth * pParams->DstFullHeight * 2);
			break;
		case CRYCBY:
			return (pParams->DstFullWidth * pParams->DstFullHeight * 2);
			break;
		case RGB24:
			return (pParams->DstFullWidth * pParams->DstFullHeight * 4);
			break;
		case RGB16:
			return (pParams->DstFullWidth * pParams->DstFullHeight * 2);
			break;
		default:
			printk(KERN_ERR "Input parameter is wrong\n");
			return -EINVAL;
			break;
	}
}


