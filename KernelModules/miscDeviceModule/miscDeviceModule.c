#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>



// device open syscall
static int my_open(struct inode *inode, struct file *file)
{
    pr_info("my_open() is callded.\n");
    return 0;
}

// device close syscall
static int my_close(struct inode *inode, struct file *file)
{
    pr_info("my_close() is called.\n");
    return 0; 
}

// device ioctl syscall
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    pr_info("my_ioctl() is called. cmd : %d,  arg : %d", cmd, arg);
    return 0;
}


// file operations struct
static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
    .unlocked_ioctl = my_ioctl,
};

// Decleare and initialize struct miscdevice
static struct miscdevice my_miscdevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mydev",
    .fops = &my_fops,
};

// Device init function is going to be called when module is loaded.
static int __init my_init(void)
{
    int ret;
    pr_info("my device init.\n");

    // Register device with kernel
    ret = misc_register(&my_miscdevice);
    if(ret != 0)
    {
        pr_err("Could not register misc device : mydev");
        return ret;
    }

    // Device info
    pr_info("mydev got minor %i\n", my_miscdevice.minor);
    return 0;
}

// Device exit funtion is going to be called when module is removed from kernel.
static void __exit my_exit(void)
{
    pr_info("mydev is removed.\n");

    // Unregister device from kernel
    misc_deregister(&my_miscdevice);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Ozgur Ayik");
MODULE_DESCRIPTION("Misc device module example");
