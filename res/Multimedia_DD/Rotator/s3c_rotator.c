/*  
    Copyright (C) 2004 Samsung Electronics 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <linux/init.h>

#include <linux/moduleparam.h>
#include <linux/timer.h>
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

#define ROTATOR_SFR_SIZE        0x1000
#define ROTATOR_MINOR  		230

static int s3c_rotator_irq = NO_IRQ;
static struct resource *s3c_rotator_mem;
static void __iomem *s3c_rotator_base;
static wait_queue_head_t waitq;

int s3c_rotator_open(struct inode *inode, struct file *file)
{
	printk("s3c_rotator_open()\n"); 	
	return 0;
}

int s3c_rotator_release(struct inode *inode, struct file *file)
{
	printk("s3c_rotator_release()\n"); 
	return 0;
}

int s3c_rotator_mmap(struct file* filp, struct vm_area_struct *vma) 
{
	unsigned long pageFrameNo, size;
	
	size = vma->vm_end - vma->vm_start;

	printk("s3c_rotator_mmap()\n");

	/* page frame number of the address for a source G2D_SFR_SIZE to be stored at. */
	pageFrameNo = __phys_to_pfn(S3C6400_PA_ROTATOR);
	
	if(size > ROTATOR_SFR_SIZE) {
		printk("The size of ROTATOR_SFR_SIZE mapping is too big!\n");
		return -EINVAL;
	}
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); 
	
	if((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk("Writable ROTATOR_SFR_SIZE mapping must be shared!\n");
		return -EINVAL;
	}
	
	if(remap_pfn_range(vma, vma->vm_start, pageFrameNo, size, vma->vm_page_prot))
		return -EINVAL;
	
	return 0;
}

struct file_operations s3c_rotator_fops = {
	.owner = THIS_MODULE,
	.open = s3c_rotator_open,
	.release = s3c_rotator_release,
	.mmap = s3c_rotator_mmap,
};

static struct miscdevice s3c_rotator_dev = {
	.minor		= ROTATOR_MINOR,
	.name		= "s3c-rotator",
	.fops		= &s3c_rotator_fops,
};

int s3c_rotator_probe(struct platform_device *pdev)
{
	struct resource *res;
        int ret;

	printk(KERN_INFO "s3c_rotator_probe called\n");

	/* find the IRQs */
	s3c_rotator_irq = platform_get_irq(pdev, 0);
	if(s3c_rotator_irq <= 0) {
		printk(KERN_ERR "failed to get irq resource\n");
                return -ENOENT;
	}

        /* get the memory region */
        res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if(res == NULL) {
                printk(KERN_ERR "failed to get memory region resouce\n");
                return -ENOENT;
        }

	s3c_rotator_mem = request_mem_region(res->start, res->end - res->start + 1, pdev->name);
	if(s3c_rotator_mem == NULL) {
		printk(KERN_ERR "failed to reserved memory region\n");
                return -ENOENT;
	}

	s3c_rotator_base = ioremap(s3c_rotator_mem->start, s3c_rotator_mem->end - res->start + 1);
	if(s3c_rotator_mem == NULL) {
		printk(KERN_ERR "failed ioremap\n");
                return -ENOENT;
	}

	init_waitqueue_head(&waitq);

	ret = misc_register(&s3c_rotator_dev);
	if (ret) {
		printk (KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
			ROTATOR_MINOR, ret);
		return ret;
	}

	printk("s3c_rotator_probe success\n");

	return 0;  
}

static int s3c_rotator_remove(struct platform_device *dev)
{
	printk(KERN_INFO "s3c_rotator_remove called\n");
	return 0;
}

static int s3c_rotator_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}
static int s3c_rotator_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver s3c_rotator_driver = {
       .probe          = s3c_rotator_probe,
       .remove         = s3c_rotator_remove,
       .suspend        = s3c_rotator_suspend,
       .resume         = s3c_rotator_resume,
       .driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-rotator",
	},
};

int __init s3c_rotator_init(void)
{
 	if(platform_driver_register(&s3c_rotator_driver) != 0) {
   		printk("platform device register failed\n");
   		return -1;
  	}

	printk("s3c rotator init done\n");
       	return 0;
}

void  s3c_rotator_exit(void)
{
        platform_driver_unregister(&s3c_rotator_driver);
 	printk("s3c rotator module exit\n");
}

module_init(s3c_rotator_init);
module_exit(s3c_rotator_exit);

MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("S3C Image Rotator Device Driver");
MODULE_LICENSE("GPL");

