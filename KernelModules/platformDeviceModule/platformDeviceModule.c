#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h> 



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

// Device init function is going to be called when driver is matched with device
// not when module is loaded as char device.
static int __init my_probe(struct platform_device *pdev)
{
   int ret;
   pr_info("my_prbe is called.\n");

  ret = misc_register(&my_miscdevice);
  if(ret != 0)
  {
    pr_err("Coould not register misc device.\n");
    return ret;
  }

  pr_info("mydev got minor : %i", my_miscdevice.minor);
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
static const struct of_device_id my_of_ids[] = {
{ .compatible = "my_compatible"},
{},
};

MODULE_DEVICE_TABLE(of,my_of_ids);


// create platform driver

static struct platform_driver my_platform_driver = {
    .probe = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my_compatible",
        .of_match_table = my_of_ids,
        .owner = THIS_MODULE,
    }
};


// Register your platform driver

module_platform_driver(my_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Ozgur Ayik");
MODULE_DESCRIPTION("Misc device and platform driver example");