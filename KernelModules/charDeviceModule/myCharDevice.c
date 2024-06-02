#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>

// Define Major number
#define MY_MAJOR_NUMBER 202

static struct cdev my_dev;

// My open
static int my_dev_open(struct inode *inode, struct file *file)
{
   pr_info("my_dev_open is called.\n");
   return 0;
}


// My close
static int my_dev_close(struct inode *inode, struct file *file)
{
   pr_info("my_dev_close is called.\n");
   return 0;
}


// My ioctl
static long my_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
   pr_info("my_dev_ioctl is called . cmd : %d , arg : %d \n", cmd, arg);
   return 0;
}

// Declare a file_operations structure
static const struct file_operations my_dev_fops = {
   .owner = THIS_MODULE,
   .open = my_dev_open,
   .release = my_dev_close,
   .unlocked_ioctl = my_dev_ioctl
};

static int __init my_init(void)
{
   int ret;

   // Get first device identifier
   dev_t dev = MKDEV(MY_MAJOR_NUMBER, 0);
   pr_info("my init.\n");

   // Allocate number of devices

   ret = register_chrdev_region(dev,1,"my_char_device");

   if(ret<0)
   {
      pr_info("Unable to allocate Major number %d ",MY_MAJOR_NUMBER);
      return ret;
   }

   // initialize the cdev strycture and add it to kernel space
   cdev_init(&my_dev,&my_dev_fops);
   ret = cdev_add(&my_dev,dev,1);

   if(ret<0)
   {
      unregister_chrdev_region(dev,1);
      pr_info("Unable  to add cdev.\n");
      return ret;
   }

   return 0;
}


static void __exit my_exit(void)
{

   pr_info("my exit. \n");
   cdev_del(&my_dev);
   unregister_chrdev_region(MKDEV(MY_MAJOR_NUMBER,0), 1);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ozgur Ayik");
MODULE_DESCRIPTION("This is a print out from hello world module");

