#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h> 
#include <linux/types.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#define SEL_INDEX       18
#define INDEX       (26%32)

// Physical addresses
#define BCM2710_PERI_BASE   0x3F000000
#define GPIO_BASE           (BCM2710_PERI_BASE+0x200000)
#define GPFSEL2             (GPIO_BASE+0x08)
#define GPSET0              (GPIO_BASE+0x1C)
#define GPCLR0              (GPIO_BASE+0x28)

// Define GPIO peripheral virtual addresses.
static void __iomem *GPFSEL2_V;
static void __iomem *GPSET0_V;
static void __iomem *GPCLR0_V;

// device open syscall
static int myOpen(struct inode *inode, struct file *file)
{
    pr_info("myOpen() is callded.\n");
    return 0;
}



// device close syscall
static int myClose(struct inode *inode, struct file *file)
{
    pr_info("myClose() is called.\n");
    return 0; 
}



// device ioctl syscall
static long myIoctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    u32 temp;
    pr_info("myIoctl() is called. cmd : %d,  arg : %d", cmd, arg);

    // Clear pin
    if(arg == 0)
    {
        temp = ioread32(GPCLR0_V);
        temp |= (1<<INDEX);
        iowrite32(temp,GPCLR0_V);
    }
    // Set pin
    else if(arg == 1)
    {
        temp = ioread32(GPSET0_V);
        temp |= (1<<INDEX);
        iowrite32(temp,GPSET0_V);
    }
    return 0;
}



// file operations struct
static const struct file_operations my_fops = 
{
    .owner = THIS_MODULE,
    .open = myOpen,
    .release = myClose,
    .unlocked_ioctl = myIoctl,
};



// Decleare and initialize struct miscdevice
static struct miscdevice my_miscdevice = 
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "myGpioDevice",
    .fops = &my_fops,
};



// Device init function is going to be called when driver is matched with device
// not when module is loaded as char device.
static int __init my_probe(struct platform_device *pdev)
{
    int ret;

    ret = misc_register(&my_miscdevice);
    if(ret != 0)
    {
    pr_err("Could not register gpio device.\n");
    return ret;
    }

    pr_info("mydev got minor : %i", my_miscdevice.minor);

    // Get corresponding virtual addresses for hardware
    GPFSEL2_V = ioremap (GPFSEL2, sizeof(u32));
    GPSET0_V = ioremap (GPSET0, sizeof(u32));
    GPCLR0_V = ioremap (GPCLR0, sizeof(u32));

    // Set pin.
    u32 selRead;

    selRead = ioread32(GPFSEL2_V);

    selRead |= (1<<SEL_INDEX); // Set 26 as output.

    iowrite32(selRead,GPFSEL2_V);

    u32 clrRead;
    
    clrRead = ioread32(GPCLR0_V);

    pr_info("My probe is done.\n");
    return 0;
}



// Exit function
static int __exit my_remove(struct platform_device *pdev)
{
    pr_info("my_remove is called.\n");
    misc_deregister(&my_miscdevice);
    return 0;
}



// Declera list of devices supported by driver
static const struct of_device_id my_of_ids[] = 
{
    { .compatible = "my_compatible"},
    {},
};



MODULE_DEVICE_TABLE(of,my_of_ids);



// create platform driver
static struct platform_driver my_platform_driver = 
{
    .probe = my_probe,
    .remove = my_remove,
    .driver = 
    {
        .name = "myGpioDriver",
        .of_match_table = my_of_ids,
        .owner = THIS_MODULE,
    }
};



// Register your platform driver
module_platform_driver(my_platform_driver);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Ozgur Ayik");
MODULE_DESCRIPTION("basic gpio test driver for raspberry pi zero two.");