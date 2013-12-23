
/*
 * linux/drivers/video/s3c_pp_6400.c
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
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/init.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
#include <linux/config.h>
#include <asm/arch/registers.h>
#include <asm/arch-s3c64xx/reserved_mem.h>
#else
#include <asm/arch/regs-pp.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch-s3c2410/regs-s3c6400-clock.h>
#include <asm/arch-s3c2410/reserved_mem.h>
#include <linux/pm.h>
#include <asm/plat-s3c24xx/pm.h>
#endif

#include "s3c_pp_common.h"

#define PFX "s3c_pp"

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,24)
extern void s3cfb_enable_local_post(int in_yuv);
extern void s3cfb_enable_dma(void);
#define s3c_fb_enable_local_post	s3cfb_enable_local_post
#define s3c_fb_enable_dma			s3cfb_enable_dma
#endif

//#define NO_PIP_MODE	1	// Currently, Do not support PIP mode

// if you want to modify src/dst buffer size, modify below defined size 
#define SYSTEM_RAM			0x07000000	// 128mb
#define RESERVE_POST_MEM	8*1024*1024	// 8mb
#define PRE_BUFF_SIZE		4*1024*1024	//4 // 4mb
#define POST_BUFF_SIZE		( RESERVE_POST_MEM - PRE_BUFF_SIZE )
#define POST_BUFF_BASE_ADDR	POST_RESERVED_MEM_START

#define USE_DEDICATED_MEM	1

static struct resource *s3c_pp_mem;

static void __iomem *s3c_pp_base;
static int s3c_pp_irq = NO_IRQ;


static struct clk *pp_clock;
static struct clk *h_clk;

static struct mutex *h_mutex;

static wait_queue_head_t waitq;


void set_scaler_register(scaler_info_t * scaler_info, pp_params *pParams)
{
	__raw_writel((scaler_info->pre_v_ratio<<7)|(scaler_info->pre_h_ratio<<0), s3c_pp_base + S3C_VPP_PRESCALE_RATIO);
	__raw_writel((scaler_info->pre_dst_height<<12)|(scaler_info->pre_dst_width<<0), s3c_pp_base + S3C_VPP_PRESCALEIMGSIZE);
	__raw_writel(scaler_info->sh_factor, s3c_pp_base + S3C_VPP_PRESCALE_SHFACTOR);
	__raw_writel(scaler_info->dx, s3c_pp_base + S3C_VPP_MAINSCALE_H_RATIO);
	__raw_writel(scaler_info->dy, s3c_pp_base + S3C_VPP_MAINSCALE_V_RATIO);
	__raw_writel((pParams->SrcHeight<<12)|(pParams->SrcWidth), s3c_pp_base + S3C_VPP_SRCIMGSIZE);
	__raw_writel((pParams->DstHeight<<12)|(pParams->DstWidth), s3c_pp_base + S3C_VPP_DSTIMGSIZE);
}

void set_buf_addr_register(buf_addr_t *buf_addr, pp_params *pParams)
{
	__raw_writel(buf_addr->src_start_y, s3c_pp_base + S3C_VPP_ADDRSTART_Y);
	__raw_writel(buf_addr->offset_y, s3c_pp_base + S3C_VPP_OFFSET_Y);
	__raw_writel(buf_addr->src_end_y, s3c_pp_base + S3C_VPP_ADDREND_Y);

	if(pParams->SrcCSpace == YC420) {
		__raw_writel(buf_addr->src_start_cb, s3c_pp_base + S3C_VPP_ADDRSTART_CB);
		__raw_writel(buf_addr->offset_cr, s3c_pp_base + S3C_VPP_OFFSET_CB);
		__raw_writel(buf_addr->src_end_cb, s3c_pp_base + S3C_VPP_ADDREND_CB);
		__raw_writel(buf_addr->src_start_cr, s3c_pp_base + S3C_VPP_ADDRSTART_CR);
		__raw_writel(buf_addr->offset_cb, s3c_pp_base + S3C_VPP_OFFSET_CR);
		__raw_writel(buf_addr->src_end_cr, s3c_pp_base + S3C_VPP_ADDREND_CR);	
	}

	if(pParams->OutPath == POST_DMA) {
		__raw_writel(buf_addr->dst_start_rgb, s3c_pp_base + S3C_VPP_ADDRSTART_RGB);
		__raw_writel(buf_addr->offset_rgb, s3c_pp_base + S3C_VPP_OFFSET_RGB);
		__raw_writel(buf_addr->dst_end_rgb, s3c_pp_base + S3C_VPP_ADDREND_RGB);

		if(pParams->DstCSpace == YC420) {
			__raw_writel(buf_addr->out_src_start_cb, s3c_pp_base + S3C_VPP_ADDRSTART_OCB);
			__raw_writel(buf_addr->out_offset_cb, s3c_pp_base + S3C_VPP_OFFSET_OCB);
			__raw_writel(buf_addr->out_src_end_cb, s3c_pp_base + S3C_VPP_ADDREND_OCB);
			__raw_writel(buf_addr->out_src_start_cr, s3c_pp_base + S3C_VPP_ADDRSTART_OCR);
			__raw_writel(buf_addr->out_offset_cr, s3c_pp_base + S3C_VPP_OFFSET_OCR);
			__raw_writel(buf_addr->out_src_end_cr, s3c_pp_base + S3C_VPP_ADDREND_OCR);
		}
	}
}

void set_data_format_register(pp_params *pParams)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);
	tmp |= (0x1<<16);
	tmp |= (0x2<<10);

	// set the source color space
	switch(pParams->SrcCSpace) {
		case YC420:
			tmp &=~((0x1<<3)|(0x1<<2));
			tmp |= (0x1<<8)|(0x1<<1);
			break;
		case CRYCBY:
			tmp &= ~((0x1<<15)|(0x1<<8)|(0x1<<3)|(0x1<<0));
			tmp |= (0x1<<2)|(0x1<<1);
			break;
		case CBYCRY:
			tmp &= ~((0x1<<8)|(0x1<<3)|(0x1<<0));
			tmp |= (0x1<<15)|(0x1<<2)|(0x1<<1);
			break;
		case YCRYCB:
			tmp &= ~((0x1<<15)|(0x1<<8)|(0x1<<3));
			tmp |= (0x1<<2)|(0x1<<1)|(0x1<<0);
			break;
		case YCBYCR:
			tmp &= ~((0x1<<8)|(0x1<<3));
			tmp |= (0x1<<15)|(0x1<<2)|(0x1<<1)|(0x1<<0);	
			break;
		case RGB24:
			tmp &= ~(0x1<<8);
			tmp |=  (0x1<<3)|(0x1<<2)|(0x1<<1);
			break;
		case RGB16:
			tmp &= ~((0x1<<8)|(0x1<<1));
			tmp |=  (0x1<<3)|(0x1<<2);
			break;
		default:
			break;
	}

	// set the destination color space
	if(pParams->OutPath == POST_DMA) {
		switch(pParams->DstCSpace) {
			case YC420:
				tmp &= ~(0x1<<18);
				tmp |= (0x1<<17);
				break;
			case CRYCBY:
				tmp &= ~((0x1<<20)|(0x1<<19)|(0x1<<18)|(0x1<<17));
				break;
			case CBYCRY:
				tmp &= ~((0x1<<19)|(0x1<<18)|(0x1<<17));
				tmp |= (0x1<<20);
				break;
			case YCRYCB:
				tmp &= ~((0x1<<20)|(0x1<<18)|(0x1<<17));
				tmp |= (0x1<<19);
				break;
			case YCBYCR:
				tmp &= ~((0x1<<18)|(0x1<<17));
				tmp |= (0x1<<20)|(0x1<<19);	
				break;
			case RGB24:
				tmp |= (0x1<<18)|(0x1<<4);
				break;
			case RGB16:
				tmp &= ~(0x1<<4);
				tmp |= (0x1<<18);
				break;
			default:
				break;
		}
	}
	else if(pParams->OutPath == POST_FIFO) {
		if(pParams->DstCSpace == RGB30) {
			tmp |= (0x1<<18)|(0x1<<13); 

		} else if(pParams->DstCSpace == YUV444) {
			tmp |= (0x1<<13);
			tmp &= ~(0x1<<18)|(0x1<<17);
		} 	
	}

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}

static void set_clock_src(pp_clk_src_t clk_src)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);

	if(clk_src == HCLK) {
		if((unsigned int)h_clk > 66000000) {
			tmp &= ~(0x7f<<23);
			tmp |= (1<<24);
			tmp |= (1<<23);
		} else {
			tmp &=~ (0x7f<<23);
		}

	} else if(clk_src == PLL_EXT) {
	} else {
		tmp &=~(0x7f<<23);
	}

	tmp = (tmp &~ (0x3<<21)) | (clk_src<<21);

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}


static void set_data_path(pp_params *pParams)
{
	u32 tmp;

		
	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);	

	tmp &=~(0x1<<12);	// 0: progressive mode, 1: interlace mode

	tmp &=~(0x1<<31);

	if(pParams->OutPath == POST_FIFO) {
		s3c_fb_enable_local_post(0);
		tmp |= (0x1<<13);
	} else if(pParams->OutPath == POST_DMA) {
		s3c_fb_enable_dma();
		tmp &=~(0x1<<13);
	}

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}

static void set_interlace_mode(u32 on_off)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);

	if(on_off == 1) tmp |=(1<<12);
	else tmp &=~(1<<12);

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}


static void set_auto_load(pp_params *pParams)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);

	if(pParams->Mode == FREE_RUN) {
		tmp |= (1<<14);
	} else if(pParams->Mode == ONE_SHOT) {
		tmp &=~(1<<14);
	}

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}

static void post_int_enable(u32 int_type)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);

	if(int_type == 0) {		//Edge triggering
		tmp &= ~(S3C_MODE_IRQ_LEVEL);
	} else if(int_type == 1) {	//level triggering
		tmp |= S3C_MODE_IRQ_LEVEL;
	}

	tmp |= S3C_MODE_POST_INT_ENABLE;

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}

static void post_int_disable(void)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);

	tmp &=~ S3C_MODE_POST_INT_ENABLE;

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}

static void start_processing(void)
{
	__raw_writel(0x1<<31, s3c_pp_base + S3C_VPP_POSTENVID);
}

static void stop_processing_free_run(void)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_MODE);

	tmp &=~(1<<14);

	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE);
}

s3c_pp_state_t post_get_processing_state(void)
{
	s3c_pp_state_t	state;
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_POSTENVID);
	if (tmp & S3C_VPP_POSTENVID)
	{
		state = POST_BUSY;
	}
	else
	{
		state = POST_IDLE;
	}

	printk("Post processing state = %d\n", state);

	return state;
}


static void config_pp(pp_params	*pParams)
{
	u32 tmp;

	tmp = __raw_readl(s3c_pp_base + S3C_VPP_POSTENVID);
	tmp &= ~S3C_POSTENVID_ENABLE;
	__raw_writel(tmp, s3c_pp_base + S3C_VPP_POSTENVID);

	tmp = S3C_MODE2_ADDR_CHANGE_DISABLE |S3C_MODE2_CHANGE_AT_FRAME_END |S3C_MODE2_SOFTWARE_TRIGGER;
	__raw_writel(tmp, s3c_pp_base + S3C_VPP_MODE_2);

#ifdef NO_PIP_MODE
	pParams->SrcStartX	= pParams->SrcStartY = 0;
	pParams->DstStartX	= pParams->DstStartY = 0;
	pParams->SrcWidth	= pParams->SrcFullWidth;
	pParams->SrcHeight	= pParams->SrcFullHeight;
	pParams->DstWidth	= pParams->DstFullWidth;
	pParams->DstHeight	= pParams->DstFullHeight;
#endif

	set_clock_src(HCLK);

	// setting the output data path (DMA or FIFO)
	set_data_path(pParams);

	// setting the src/dst color space
	set_data_format(pParams);

	// setting the src/dst size 
	set_scaler(pParams);

	// setting the src/dst buffer address
	set_buf_addr(pParams);
	
	set_auto_load(pParams);

}


irqreturn_t s3c_pp_isr(int irq, void *dev_id,
		struct pt_regs *regs)
{
	u32 mode;
	mode = __raw_readl(s3c_pp_base + S3C_VPP_MODE);
	mode &= ~(1 << 6);			/* Clear Source in POST Processor */
	__raw_writel(mode, s3c_pp_base + S3C_VPP_MODE);
	wake_up_interruptible(&waitq);
	return IRQ_HANDLED;
}


int s3c_pp_open(struct inode *inode, struct file *file) 
{ 	
	pp_params	*pParams;

	// allocating the post processor instance
	pParams	= (pp_params *)kmalloc(sizeof(pp_params), GFP_KERNEL);
	if (pParams == NULL) {
		printk(KERN_ERR "Instance memory allocation was failed\n");
	}

	memset(pParams, 0, sizeof(pp_params));

	file->private_data	= (pp_params *)pParams;
	
	return 0;
}

int s3c_pp_read(struct file *file, char *buf, size_t count,
		loff_t *f_pos)
{
	return 0;
}

int s3c_pp_write(struct file *file, const char *buf, size_t 
		count, loff_t *f_pos)
{
	return 0;
}

int s3c_pp_mmap(struct file *file, struct vm_area_struct *vma)
{
	u32 size = vma->vm_end - vma->vm_start;
	u32 max_size;
	u32 page_frame_no;

	page_frame_no = __phys_to_pfn(POST_BUFF_BASE_ADDR);

	max_size = RESERVE_POST_MEM + PAGE_SIZE - (RESERVE_POST_MEM % PAGE_SIZE);

	if(size > max_size) {
		return -EINVAL;
	}

	vma->vm_flags |= VM_RESERVED;

	if( remap_pfn_range(vma, vma->vm_start, page_frame_no, size, vma->vm_page_prot)) {
		printk(KERN_ERR "%s: mmap_error\n", __FUNCTION__);
		return -EAGAIN;

	}

	return 0;
}


int s3c_pp_release(struct inode *inode, struct file *file) 
{
	pp_params	*pParams;

	pParams	= (pp_params *)file->private_data;
	if (pParams == NULL) {
		return -1;
	}

	kfree(pParams);
	
	return 0;
}

void pp_for_mfc(unsigned int uSrcFrmSt,     unsigned int uDstFrmSt,
		unsigned int srcFrmFormat,  unsigned int dstFrmFormat,
		unsigned int srcFrmWidth,   unsigned int srcFrmHeight,
		unsigned int dstFrmWidth,   unsigned int dstFrmHeight,
		unsigned int srcXOffset,    unsigned int srcYOffset,
		unsigned int dstXOffset,    unsigned int dstYOffset,
		unsigned int srcCropWidth,  unsigned int srcCropHeight,
		unsigned int dstCropWidth,  unsigned int dstCropHeight
		)
{
	pp_params	*pParams;

	pParams	= (pp_params *)kmalloc(sizeof(pp_params), GFP_KERNEL);
	if (pParams == NULL) {
		printk(KERN_ERR "Instance memory allocation was failed\n");
	}
	
	pParams->SrcFullWidth	= srcFrmWidth;
	pParams->SrcFullHeight	= srcFrmHeight;
	pParams->SrcStartX		= srcXOffset;
	pParams->SrcStartY		= srcYOffset;
	pParams->SrcWidth		= srcCropWidth;
	pParams->SrcHeight		= srcCropHeight;
	pParams->SrcFrmSt		= uSrcFrmSt;
	pParams->SrcCSpace		= srcFrmFormat;

	pParams->DstFullWidth	= dstFrmWidth;
	pParams->DstFullHeight	= dstFrmHeight;
	pParams->DstStartX		= dstXOffset;
	pParams->DstStartY		= dstYOffset;
	pParams->DstWidth		= dstCropWidth;
	pParams->DstHeight		= dstCropHeight;
	pParams->DstFrmSt		= uDstFrmSt;
	pParams->DstCSpace		= dstFrmFormat;

	pParams->Mode			= ONE_SHOT;
	pParams->OutPath		= POST_DMA;

	config_pp(pParams);

	post_int_enable(1);
	start_processing();		
	if (interruptible_sleep_on_timeout(&waitq, 500) == 0) {
		printk(KERN_ERR "\n%s: Waiting for interrupt is timeout\n", __FUNCTION__);					
	}
	post_int_disable();
}

EXPORT_SYMBOL(pp_for_mfc);

static int s3c_pp_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	pp_params	*pParams;
	pp_params	*parg;

	pParams	= (pp_params *)file->private_data;
	parg	= (pp_params *)arg;

	mutex_lock(h_mutex);
	
	switch(cmd) {
		case PPROC_SET_PARAMS:		
			get_user(pParams->SrcFullWidth, &parg->SrcFullWidth);
			get_user(pParams->SrcFullHeight, &parg->SrcFullHeight);
			get_user(pParams->SrcStartX, &parg->SrcStartX);
			get_user(pParams->SrcStartY, &parg->SrcStartY);
			get_user(pParams->SrcWidth, &parg->SrcWidth);
			get_user(pParams->SrcHeight, &parg->SrcHeight);
			get_user(pParams->SrcFrmSt, &parg->SrcFrmSt);
			get_user(pParams->SrcCSpace, &parg->SrcCSpace);
			get_user(pParams->DstFullWidth, &parg->DstFullWidth);
			get_user(pParams->DstFullHeight, &parg->DstFullHeight);
			get_user(pParams->DstStartX, &parg->DstStartX);
			get_user(pParams->DstStartY, &parg->DstStartY);
			get_user(pParams->DstWidth, &parg->DstWidth);
			get_user(pParams->DstHeight, &parg->DstHeight);
			get_user(pParams->DstFrmSt, &parg->DstFrmSt);
			get_user(pParams->DstCSpace, &parg->DstCSpace);
			get_user(pParams->SrcFrmBufNum, &parg->SrcFrmBufNum);
			get_user(pParams->DstFrmSt, &parg->DstFrmSt);
			get_user(pParams->Mode, &parg->Mode);	
			get_user(pParams->OutPath, &parg->OutPath);
			if( (pParams->DstFullWidth > 2048) || (pParams->DstFullHeight > 2048) )
			{
				printk(KERN_ERR "\n%s: maximum width and height size are 2048\n", __FUNCTION__);
				mutex_unlock(h_mutex);
				return -EINVAL;
			}

			config_pp(pParams);
			break;
		case PPROC_START:
			if(pParams->Mode == ONE_SHOT) {
				post_int_enable(1);
				start_processing();		
				if (interruptible_sleep_on_timeout(&waitq, 500) == 0) {
					printk(KERN_ERR "\n%s: Waiting for interrupt is timeout\n", __FUNCTION__);					
				}
				post_int_disable();
			} else if(pParams->Mode == FREE_RUN) {
				post_int_disable();
				start_processing();
			}		
			break;
		case PPROC_STOP:
			if(pParams->Mode == FREE_RUN) {
				stop_processing_free_run();
			}
			break;
		case PPROC_INTERLACE_MODE:
			set_interlace_mode(1);	//interlace mode
			break;
		case PPROC_PROGRESSIVE_MODE:
			set_interlace_mode(0);
			break;
		case PPROC_GET_PHY_INBUF_ADDR:
			mutex_unlock(h_mutex);
			return POST_BUFF_BASE_ADDR;
			break;
		case PPROC_GET_INBUF_SIZE:
			mutex_unlock(h_mutex);
			return PRE_BUFF_SIZE;
			break;
		case PPROC_GET_BUF_SIZE:
			mutex_unlock(h_mutex);
			return RESERVE_POST_MEM;
			break;
		case PPROC_START_ONLY:
			if(pParams->Mode == ONE_SHOT) {
				post_int_enable(1);
				start_processing();		
			}
			break;
		case PPROC_GET_OUT_DATA_SIZE:
			mutex_unlock(h_mutex);
			return get_out_data_size(pParams);
			break;
		default:
			mutex_unlock(h_mutex);
			return -EINVAL;
	}

	mutex_unlock(h_mutex);
	
	return 0;
}

static unsigned int s3c_pp_poll(struct file *file, poll_table *wait)
{
	unsigned int	mask = 0;

	poll_wait(file, &waitq, wait);
	post_int_disable();
	mask |= POLLIN | POLLRDNORM;

	return mask;
}

static struct file_operations s3c_pp_fops = {
	.owner 	= THIS_MODULE,
	.ioctl 	= s3c_pp_ioctl,
	.open 	= s3c_pp_open,
	.read 	= s3c_pp_read,
	.write 	= s3c_pp_write,
	.mmap	= s3c_pp_mmap,
	.poll	= s3c_pp_poll,
	.release 	= s3c_pp_release
};


static struct miscdevice s3c_pp_dev = {
	.minor		= PP_MINOR,
	.name		= "s3c-pp",
	.fops		= &s3c_pp_fops,
};

static int s3c_pp_remove(struct platform_device *dev)
{
	clk_disable(pp_clock);

	free_irq(s3c_pp_irq, NULL);
	if (s3c_pp_mem != NULL) {
		pr_debug("s3c_post: releasing s3c_post_mem\n");
		iounmap(s3c_pp_base);
		release_resource(s3c_pp_mem);
		kfree(s3c_pp_mem);
	}
	misc_deregister(&s3c_pp_dev);
	return 0;
}



int s3c_pp_probe(struct platform_device *pdev)
{

	struct resource *res;

	int ret;

	int tmp;

	// Use DOUTmpll source clock as a scaler clock
	tmp = __raw_readl(S3C_CLK_SRC);

	tmp &=~(0x3<<28);
	tmp |= (0x1<<28);
	__raw_writel(tmp, S3C_CLK_SRC);

	/* find the IRQs */
	s3c_pp_irq = platform_get_irq(pdev, 0);


	if(s3c_pp_irq <= 0) {
		printk(KERN_ERR PFX "failed to get irq resouce\n");
		return -ENOENT;
	}

	/* get the memory region for the post processor driver */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(res == NULL) {
		printk(KERN_ERR PFX "failed to get memory region resouce\n");
		return -ENOENT;
	}

	s3c_pp_mem = request_mem_region(res->start, res->end-res->start+1, pdev->name);
	if(s3c_pp_mem == NULL) {
		printk(KERN_ERR PFX "failed to reserve memory region\n");
		return -ENOENT;
	}


	s3c_pp_base = ioremap(s3c_pp_mem->start, s3c_pp_mem->end - res->start + 1);
	if(s3c_pp_base == NULL) {
		printk(KERN_ERR PFX "failed ioremap\n");
		return -ENOENT;
	}

	pp_clock = clk_get(&pdev->dev, "post");
	if(pp_clock == NULL) {
		printk(KERN_ERR PFX "failed to find post clock source\n");
		return -ENOENT;
	}

	clk_enable(pp_clock);

	h_clk = clk_get(&pdev->dev, "hclk");
	if(h_clk == NULL) {
		printk(KERN_ERR PFX "failed to find h_clk clock source\n");
		return -ENOENT;
	}

	init_waitqueue_head(&waitq);

	ret = misc_register(&s3c_pp_dev);
	if (ret) {
		printk (KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
				PP_MINOR, ret);
		return ret;
	}
	ret = request_irq(s3c_pp_irq, s3c_pp_isr, SA_INTERRUPT,
			pdev->name, NULL);
	if (ret) {
		printk("request_irq(PP) failed.\n");
		return ret;
	}

	h_mutex = (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (h_mutex == NULL)
		return -1;
	
	mutex_init(h_mutex);

	/* check to see if everything is setup correctly */	
	return 0;
}

static int s3c_pp_suspend(struct platform_device *dev, pm_message_t state)
{
	int	post_state;


	post_state = post_get_processing_state();
	while (post_state == POST_BUSY)
		msleep(1);

	clk_disable(pp_clock);
	return 0;
}
static int s3c_pp_resume(struct platform_device *pdev)
{
	clk_enable(pp_clock);
	return 0;
}
static struct platform_driver s3c_pp_driver = {
	.probe          = s3c_pp_probe,
	.remove         = s3c_pp_remove,
	.suspend        = s3c_pp_suspend,
	.resume         = s3c_pp_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-vpp",
	},
};

static char banner[] __initdata = KERN_INFO "S3C PostProcessor Driver, (c) 2007 Samsung Electronics\n";

int __init  s3c_pp_init(void)
{

	printk(banner);
	if(platform_driver_register(&s3c_pp_driver)!=0)
	{
		printk("platform device register Failed \n");
		return -1;
	}

	return 0;
}

void  s3c_pp_exit(void)
{
	platform_driver_unregister(&s3c_pp_driver);
	mutex_destroy(h_mutex);
	printk("S3C PostProcessor module exit\n");
}
module_init(s3c_pp_init);
module_exit(s3c_pp_exit);


MODULE_AUTHOR("jiun.yu@samsung.com");
MODULE_DESCRIPTION("S3C PostProcessor Device Driver");
MODULE_LICENSE("GPL");


