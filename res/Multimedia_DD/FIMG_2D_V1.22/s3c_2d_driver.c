/*  
    Copyright (C) 2004 Samsung Electronics 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <linux/init.h>

#include <linux/moduleparam.h>
//#include <linux/config.h>
#include <linux/timer.h>
//#include <asm/arch/registers.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <asm/uaccess.h>
#include <linux/errno.h> /* error codes */
#include <asm/div64.h>
#include <linux/tty.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/arch/map.h>
#include <linux/miscdevice.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/semaphore.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>

#include "s3c_2d_driver.h"

static struct clk *s3c_2d_clock;
static struct clk *h_clk;
static int s3c_2d_irq = NO_IRQ;
static struct resource *s3c_2d_mem;
static void __iomem *s3c_2d_base;
static wait_queue_head_t waitq;


 int s3c_2d_open(struct inode *inode, struct file *file)
{
	printk("s3c_2d_open() \n"); 	
	return 0;
}


int s3c_2d_release(struct inode *inode, struct file *file)
{
	printk("s3c_2d_release() \n"); 
	return 0;
}


int s3c_2d_mmap(struct file* filp, struct vm_area_struct *vma) 
{
	printk("s3c_2d_mmap() \n"); 

	unsigned long pageFrameNo, size;
	
	size = vma->vm_end - vma->vm_start;

	// page frame number of the address for a source G2D_SFR_SIZE to be stored at. 
	pageFrameNo = __phys_to_pfn(S3C6400_PA_2D);
	
	if(size > G2D_SFR_SIZE) {
		printk("The size of G2D_SFR_SIZE mapping is too big!\n");
		return -EINVAL;
	}
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); 
	
	if((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk("Writable G2D_SFR_SIZE mapping must be shared !\n");
		return -EINVAL;
	}
	
	if(remap_pfn_range(vma, vma->vm_start, pageFrameNo, size, vma->vm_page_prot))
		return -EINVAL;
	
	return 0;
}


 struct file_operations s3c_2d_fops = {
	.owner = THIS_MODULE,
	.open = s3c_2d_open,
	.release = s3c_2d_release,
	.mmap = s3c_2d_mmap,
};


static struct miscdevice s3c_2d_dev = {
	.minor		= G2D_MINOR,
	.name		= "s3c-2d",
	.fops		= &s3c_2d_fops,
};

int s3c_2d_probe(struct platform_device *pdev)
{

	printk(KERN_INFO "s3c_2d_probe called !\n");
	struct resource *res;
        int ret;

	/* find the IRQs */
	s3c_2d_irq = platform_get_irq(pdev, 0);
	if(s3c_2d_irq <= 0) {
		printk(KERN_ERR "failed to get irq resouce\n");
                return -ENOENT;
	}

        /* get the memory region for the tv scaler driver */

        res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if(res == NULL) {
                printk(KERN_ERR "failed to get memory region resouce\n");
                return -ENOENT;
        }

	s3c_2d_mem = request_mem_region(res->start, res->end-res->start+1, pdev->name);
	if(s3c_2d_mem == NULL) {
		printk(KERN_ERR "failed to reserve memory region\n");
                return -ENOENT;
	}


	s3c_2d_base = ioremap(s3c_2d_mem->start, s3c_2d_mem->end - res->start + 1);
	if(s3c_2d_mem == NULL) {
		printk(KERN_ERR "failed ioremap\n");
                return -ENOENT;
	}

	s3c_2d_clock = clk_get(&pdev->dev, "graphics_2d");
        if(s3c_2d_clock == NULL) {
                printk(KERN_ERR "failed to find graphics_2d clock source\n");
                return -ENOENT;
        }

        clk_enable(s3c_2d_clock);

	h_clk = clk_get(&pdev->dev, "hclk");
        if(h_clk == NULL) {
                printk(KERN_ERR "failed to find h_clk clock source\n");
                return -ENOENT;
        }

	init_waitqueue_head(&waitq);

	ret = misc_register(&s3c_2d_dev);
	if (ret) {
		printk (KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
			G2D_MINOR, ret);
		return ret;
	}

	printk(" s3c_2d_probe Success\n");

	return 0;  
}

static int s3c_2d_remove(struct platform_device *dev)
{
	printk(KERN_INFO "s3c_2d_remove called !\n");
	return 0;
}

static int s3c_2d_suspend(struct platform_device *dev, pm_message_t state)
{
        clk_disable(s3c_2d_clock);
	return 0;
}
static int s3c_2d_resume(struct platform_device *pdev)
{
        clk_enable(s3c_2d_clock);
	return 0;
}


static struct platform_driver s3c_2d_driver = {
       .probe          = s3c_2d_probe,
       .remove         = s3c_2d_remove,
       .suspend        = s3c_2d_suspend,
       .resume         = s3c_2d_resume,
       .driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-2d",
	},
};

int __init  s3c_2d_init(void)
{
 	if(platform_driver_register(&s3c_2d_driver)!=0)
  	{
   		printk("platform device register Failed \n");
   		return -1;
  	}
	printk(" S3C 2D  Init : Done\n");
       	return 0;
}

void  s3c_2d_exit(void)
{
        platform_driver_unregister(&s3c_2d_driver);
 	printk("S3C: 2D module exit\n");
}
module_init(s3c_2d_init);
module_exit(s3c_2d_exit);


MODULE_AUTHOR("");
MODULE_DESCRIPTION("S3C Graphics 2D Device Driver");
MODULE_LICENSE("GPL");




