#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

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


static const struct file_operations miscDeviceFileOperations = 
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
    .fops = &miscDeviceFileOperations,
};

static int __init init(void)
{    
    // Register device with kernel
    int ret = misc_register(&miscDevice);
    if(ret != 0)
    {
        pr_err("Could not register misc device : mydev");
        return ret;
    }

    // Device info
    pr_info("mydev got minor %i\n", miscDevice.minor);
    return 0;
}

static void __exit customExit(void)
{
    pr_info("mydev is removed.\n");

    // Unregister device from kernel
    misc_deregister(&miscDevice);
}

module_init(init);
module_exit(customExit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Ozgur Ayik");
MODULE_DESCRIPTION("Misc device module example");