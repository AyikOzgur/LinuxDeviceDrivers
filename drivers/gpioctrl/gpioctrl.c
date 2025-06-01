#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/gpio/consumer.h>

#define INIT_GPIO _IO('G', 0)
#define READ_PIN _IO('G', 1)
#define WRITE_PIN _IO('G', 2)

typedef struct 
{
   int inputPin;
   int outputPin;
} GPIOConfig_t;

static GPIOConfig_t g_gpioParams;
static struct gpio_desc *g_inputGpio = NULL;
static struct gpio_desc *g_outputGpio = NULL;

static int open(struct inode *inode, struct file *file)
{
   return 0;
}

static int close(struct inode *inode, struct file *file)
{
   if (g_inputGpio) 
   {
      gpiod_put(g_inputGpio);
      g_inputGpio = NULL;
   }

   if (g_outputGpio)
   {
      gpiod_put(g_outputGpio);
      g_outputGpio = NULL;
   }

   return 0;
}

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
   switch (cmd)
   {
   case INIT_GPIO:
   {
      int bytes = copy_from_user(&g_gpioParams, (GPIOConfig_t *)arg, sizeof(GPIOConfig_t));
      if (bytes < 0)
      {
         pr_err("gpioctrl - Bad address");
         return -EFAULT; // Bad address.
      }
      pr_info("gpioctrl - Input pin %d \n Output pin %d\n", g_gpioParams.inputPin, g_gpioParams.outputPin);

      // Check if pin numbers are logical.
      if (g_gpioParams.inputPin == g_gpioParams.outputPin ||
          g_gpioParams.inputPin < 0||
          g_gpioParams.outputPin < 0)
      {
         pr_err("gpioctrl - Invalid gpio pin numbers");
         return -EINVAL; // Invalid argument.
      }

      g_inputGpio = gpio_to_desc(g_gpioParams.inputPin);
      g_outputGpio = gpio_to_desc(g_gpioParams.outputPin);
      if (!g_inputGpio || !g_outputGpio)
      {
         pr_err("gpioctrl - Error getting gpio descripter");
         return -EINVAL; // Invalid argument.
      }

      int ret = gpiod_direction_output(g_outputGpio, 0); // Default low.
      if (ret)
      {
         pr_err("gpioctrl - Error setting pin %d as output\n", g_gpioParams.outputPin);
         return ret;
      }

      ret = gpiod_direction_input(g_inputGpio);
      if (ret)
      {
         pr_err("gpioctrl - Error setting pin %d as input\n", g_gpioParams.inputPin);
         return ret;
      }
      pr_info("gpioctrl - Driver is initialized.\n");
      break;
   }
   case WRITE_PIN:
   {
      int value;
      if (copy_from_user(&value, (void __user *)arg, sizeof(value)))
            return -EFAULT;
      if (value < 0 || value > 1)
         return -EINVAL; // Invalid argument.
   
      gpiod_set_value(g_outputGpio, value);
      break;
   }
   case READ_PIN:
   {
      int value = gpiod_get_value(g_inputGpio);
      if (copy_to_user((void __user *)arg, &value, sizeof(value)))
         return -EFAULT;
      break;
   }
   default:
   {
      return -EINVAL; // Invalid argument.
   }
   }

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
   .name = "my_gpio_driver",
   .fops = &miscDeviceFileOperations,
};

static int __init gpictrl_init(void)
{
   // Register device with kernel
   int ret = misc_register(&miscDevice);
   if (ret != 0)
   {
      pr_err("gpioctrl - Could not register misc device");
      return ret;
   }

   // Device info
   pr_info("gpioctrl - Driver is loaded with %i minor number as misc device.\n", 
            miscDevice.minor);
   return 0;
}

static void __exit gpioctrl_exit(void)
{
   pr_info("gpioctrl - is removed.\n");

   if (g_inputGpio) 
   {
      gpiod_put(g_inputGpio);
      g_inputGpio = NULL;
   }
   if (g_outputGpio)
   {
      gpiod_put(g_outputGpio);
      g_outputGpio = NULL;
   }

   // Unregister device from kernel
   misc_deregister(&miscDevice);
}

module_init(gpictrl_init);
module_exit(gpioctrl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ozgur Ayik");
MODULE_DESCRIPTION("Simple gpio driver example");