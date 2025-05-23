#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h> 

static int open(struct inode *inode, struct file *file)
{
    pr_info("open() is callded.\n");
    return 0;
}

static int close(struct inode *inode, struct file *file)
{
    pr_info("close() is called.\n");
    return 0; 
}

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    pr_info("ioctl() is called. cmd : %d,  arg : %d", cmd, arg);
    return 0;
}

static const struct file_operations deviceFileOperations = 
{
    .owner = THIS_MODULE,
    .open = open,
    .release = close,
    .unlocked_ioctl = ioctl,
};

static struct miscdevice miscDevice = 
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mydev",
    .fops = &deviceFileOperations,
};

// Device init function is going to be called when driver is matched with device
// not when module is loaded as char device.
static int __init probe(struct platform_device *pdev)
{
    pr_info("probe is called.\n");

    int ret = misc_register(&miscDevice);
    if(ret != 0)
    {
        pr_err("Could not register misc device.\n");
        return ret;
    }

    pr_info("mydev got minor : %i", miscDevice.minor);
    return 0;
}


// Exit function
static int __exit remove(struct platform_device *pdev)
{
    pr_info("my_remove is called.\n");
    misc_deregister(&miscDevice);
    return 0;
}

// Declera list of devices supported by driver
static const struct of_device_id ofIDs[] = 
{
    { .compatible = "my_compatible"},
    {},
};

MODULE_DEVICE_TABLE(of, ofIDs);


// create platform driver
static struct platform_driver platformDriver = 
{
    .probe = probe,
    .remove = remove,
    .driver = 
    {
        .name = "my_compatible",
        .of_match_table = ofIDs,
        .owner = THIS_MODULE,
    }
};


// Register your platform driver
module_platform_driver(platformDriver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Ozgur Ayik");
MODULE_DESCRIPTION("Misc device and platform driver example");