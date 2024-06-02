#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define MY_MAJOR_NUMBER 202

static struct cdev charDevice;

static int open(struct inode *inode, struct file *file)
{
   pr_info("open is called.\n");
   return 0;
}

static int close(struct inode *inode, struct file *file)
{
   pr_info("close is called.\n");
   return 0;
}

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
   pr_info("ioctl is called . cmd : %d , arg : %d \n", cmd, arg);
   return 0;
}

static const struct file_operations deviceFileOperations = 
{
   .owner = THIS_MODULE,
   .open = open,
   .release = close,
   .unlocked_ioctl = ioctl
};

static int __init init(void)
{
   // Get first device identifier
   dev_t dev = MKDEV(MY_MAJOR_NUMBER, 0);
   pr_info("init.\n");

   // Allocate number of devices
   int ret = register_chrdev_region(dev, 1, "charDevice");
   if(ret < 0)
   {
      pr_info("Unable to allocate Major number %d ", MY_MAJOR_NUMBER);
      return ret;
   }

   // initialize the cdev strycture and add it to kernel space
   cdev_init(&charDevice, &deviceFileOperations);
   ret = cdev_add(&charDevice, dev, 1);

   if(ret < 0)
   {
      unregister_chrdev_region(dev, 1);
      pr_info("Unable to add cdev.\n");
      return ret;
   }

   return 0;
}

// We can not define it's name as "exit", so customExit is ok.
static void __exit customExit(void)
{
   pr_info("exiting... \n");
   cdev_del(&charDevice);
   unregister_chrdev_region(MKDEV(MY_MAJOR_NUMBER, 0), 1);
}

module_init(init);
module_exit(customExit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ozgur Ayik");
MODULE_DESCRIPTION("Simple char device module.");