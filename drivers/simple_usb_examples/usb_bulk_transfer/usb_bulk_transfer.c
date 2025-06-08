#include <linux/module.h>
#include <linux/usb.h>

#define DEVICE_VID 0xdead
#define DEVICE_PID 0xbeef

struct simple_usb_device 
{
    struct usb_device *udev;
    struct usb_interface *intf;
    unsigned char bulk_in_ep;
    unsigned char bulk_out_ep;
    size_t bulk_in_maxp;
    size_t bulk_out_maxp;
    struct usb_class_driver class;
};
static struct usb_driver test_device_driver;


static int device_open(struct inode *inode, struct file *file)
{
    struct usb_interface *interface;
    struct simple_usb_device *dev;
    int subminor = iminor(inode);

    /* 1) find the interface this minor belongs to */
    interface = usb_find_interface(&test_device_driver, subminor);
    if (!interface) 
    {
        pr_err("usb_bulk_transfer_test: no interface for minor %d\n", subminor);
        return -ENODEV;
    }

    /* 2) get the per-device struct we saved in probe() */
    dev = usb_get_intfdata(interface);
    if (!dev)
        return -ENODEV;

    /* 3) store our simple_usb_device for read()/write()/ioctl() */
    file->private_data = dev;
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static ssize_t device_read(struct file *file,
                          char __user *ubuf,
                          size_t count,
                          loff_t *ppos)
{
    struct simple_usb_device *dev = file->private_data;
    unsigned char *buf;
    int retval;
    int actual_len;
    size_t to_read;

    if (!dev)
        return -ENODEV;

    /* don’t exceed the endpoint’s max packet size */
    to_read = min(count, dev->bulk_in_maxp);

    buf = kmalloc(to_read, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    /* blocking bulk‐IN */
    retval = usb_bulk_msg(dev->udev,
                          usb_rcvbulkpipe(dev->udev, dev->bulk_in_ep),
                          buf,
                          to_read,
                          &actual_len,
                          10000);  /* 10s timeout */
    if (retval) 
    {
        kfree(buf);
        return retval;
    }

    if (copy_to_user(ubuf, buf, actual_len))
        retval = -EFAULT;
    else
        retval = actual_len;

    kfree(buf);
    return retval;
}

static ssize_t device_write(struct file *file,
                           const char __user *ubuf,
                           size_t count,
                           loff_t *ppos)
{
    struct simple_usb_device *dev = file->private_data;
    unsigned char *buf;
    int retval;
    int actual_len;
    size_t to_write;

    if (!dev)
        return -ENODEV;

    to_write = min(count, dev->bulk_out_maxp);

    buf = kmalloc(to_write, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, ubuf, to_write)) 
    {
        kfree(buf);
        return -EFAULT;
    }

    /* blocking bulk‐OUT */
    retval = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, dev->bulk_out_ep),
                          buf,
                          to_write,
                          &actual_len,
                          10000);  /* 10s timeout */
    kfree(buf);
    if (retval)
        return retval;
    return actual_len;
}

static loff_t device_llseek(struct file *file, loff_t offset, int whence)
{
    return generic_file_llseek(file, offset, whence);
}

static const struct file_operations device_fops = 
{
    .owner          = THIS_MODULE,
    .open           = device_open,
    .release        = device_release,
    .unlocked_ioctl = device_ioctl,
    .read           = device_read,
    .write          = device_write,
    .llseek         = device_llseek,
};

static int device_probe(struct usb_interface *intf,
                        const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(intf);
    struct usb_host_interface *alt = intf->cur_altsetting;
    struct simple_usb_device *dev;
    int retval;
    int i;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) 
    {
        retval = -ENOMEM;
        goto err_out;
    }

    dev->udev = usb_get_dev(udev);

    /* Find & save bulk endpoints */
    for (i = 0; i < alt->desc.bNumEndpoints; i++) 
    {
        struct usb_endpoint_descriptor *epd = &alt->endpoint[i].desc;

        if (usb_endpoint_is_bulk_in(epd)) 
        {
            dev->bulk_in_ep   = epd->bEndpointAddress;
            dev->bulk_in_maxp = le16_to_cpu(epd->wMaxPacketSize);
        } 
        else if (usb_endpoint_is_bulk_out(epd)) 
        {
            dev->bulk_out_ep   = epd->bEndpointAddress;
            dev->bulk_out_maxp = le16_to_cpu(epd->wMaxPacketSize);
        }
    }

    if (!dev->bulk_in_ep || !dev->bulk_out_ep) 
    {
        dev_err(&intf->dev, "No bulk endpoints\n");
        retval = -ENODEV;
        goto err_put_dev;
    }

    /* set up the per-device class-driver */
    dev->class.name = "custom_usb_device%d";
    dev->class.fops = &device_fops;
    dev->class.minor_base = 192;

    retval = usb_register_dev(intf, &dev->class);
    if (retval)
    {
        dev_err(&intf->dev, "Unable to get minor: %d\n", retval);
        goto err_put_dev;
    }

    usb_set_intfdata(intf, dev);

    dev_info(&intf->dev, "test device is connected\n");
    return 0;

err_put_dev:
    usb_put_dev(dev->udev);
    kfree(dev);
err_out:
    return retval;
}

static void device_disconnect(struct usb_interface *intf)
{
    struct simple_usb_device *dev = usb_get_intfdata(intf);

    usb_deregister_dev(intf, &dev->class);

    usb_set_intfdata(intf, NULL);
    usb_put_dev(dev->udev);
    kfree(dev);
    dev_info(&intf->dev, "test device disconnected\n");
}

static const struct usb_device_id device_id_table[] = 
{
    { USB_DEVICE(DEVICE_VID, DEVICE_PID) },
    { } /* Terminator */
};
MODULE_DEVICE_TABLE(usb, device_id_table);

static struct usb_driver test_device_driver = 
{
    .name       = "usb_bulk_transfer_test_driver",
    .id_table   = device_id_table,
    .probe      = device_probe,
    .disconnect = device_disconnect,
};

module_usb_driver(test_device_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ozgur Ayik");
MODULE_DESCRIPTION("Simple usb driver to practice bulk transfer");