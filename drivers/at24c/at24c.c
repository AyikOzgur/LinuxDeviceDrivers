#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h> 
#include <linux/fs.h> 
#include <linux/delay.h>

#define AT24C_DEFAULT_PAGE_SIZE  64
#define AT24C_ADDR_BYTES          2

struct at24c
{
    struct i2c_client  *client;
    struct miscdevice   miscdev;
    u8                 *buf;         /* bounce buffer */
    size_t              buf_sz;      /* = AT24C_ADDR_BYTES + page_size */
    size_t              page_size; 
};

static int at24c_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct at24c *adata = container_of(misc, struct at24c, miscdev);

    // Set at24c struct as private data, it is needed in write/read to have i2c_client.
    file->private_data = adata;
    return 0;
}

static int at24c_release(struct inode *inode, struct file *file)
{
    pr_info("at24c - device closed\n");
    return 0;
}

static long at24c_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    pr_info("at24c - ioctl cmd=0x%x, arg=0x%lx\n", cmd, arg);
    return 0;
}

static ssize_t at24c_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct at24c      *adata  = file->private_data;
    struct i2c_client *client = adata->client;
    size_t remaining = count;
    size_t bytesRead = 0;
    int ret;

    while (remaining) 
    {
        loff_t offset   = *ppos;
        size_t chunk    = min(remaining, adata->page_size);

        /* Build the 2-byte address in buf[0..1]: */
        adata->buf[0] = (offset >> 8) & 0xFF;
        adata->buf[1] =  offset       & 0xFF;

        /* Tell the EEPROM what word-address we want: */
        ret = i2c_master_send(client,
                              adata->buf,
                              AT24C_ADDR_BYTES);
        if (ret < 0)
            return ret;

        /* Now read up to one page: */
        ret = i2c_master_recv(client,
                              adata->buf,
                              chunk);
        if (ret < 0)
            return ret;

        /* Copy result back to userspace: */
        if (copy_to_user(buf + bytesRead, adata->buf, ret))
            return -EFAULT;

        /* Advance counters and file-offset: */
        bytesRead += ret;
        remaining -= ret;
        *ppos += ret;
    }

    return bytesRead;
}

static ssize_t at24c_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos)
{
    struct at24c      *adata  = file->private_data;
    struct i2c_client *client = adata->client;
    size_t remaining = count;
    size_t  bytesWritten = 0;
    int ret;

    while (remaining) 
    {
        loff_t offset = *ppos;
        
        /* ensure we don’t wrap past a page: */
        size_t page_off = offset % adata->page_size;
        size_t chunk    = min(remaining,
                              adata->page_size - page_off);

        adata->buf[0] = (offset >> 8) & 0xFF;
        adata->buf[1] =  offset       & 0xFF;

        if (copy_from_user(&adata->buf[2],
                           buf + bytesWritten,
                           chunk))
            return -EFAULT;

        ret = i2c_master_send(client,
                              adata->buf,
                              AT24C_ADDR_BYTES + chunk);
        if (ret != AT24C_ADDR_BYTES + chunk || ret < 0)
            pr_warn("at24c: short write (%d/%zu)\n",
                    ret, AT24C_ADDR_BYTES + chunk);

        bytesWritten += chunk;
        remaining -= chunk;
        *ppos += chunk;

        /* EEPROMs typically need a small delay here for the write cycle */
        msleep(1);    
    }

    return bytesWritten;
}

static loff_t at24c_llseek(struct file *file, loff_t offset, int whence)
{
    return generic_file_llseek(file, offset, whence);
}

static const struct file_operations at24c_fops = 
{
    .owner          = THIS_MODULE,
    .open           = at24c_open,
    .release        = at24c_release,
    .unlocked_ioctl = at24c_ioctl,
    .read           = at24c_read,
    .write          = at24c_write,
    .llseek         = at24c_llseek,
};

static int at24c_probe(struct i2c_client *client)
{
    pr_info("at24c - Probe.\n");
    struct at24c *adata;
    int ret;

    adata = devm_kzalloc(&client->dev, sizeof(*adata), GFP_KERNEL);
    if (!adata)
        return -ENOMEM;

    adata->page_size = AT24C_DEFAULT_PAGE_SIZE;
    adata->buf_sz    = AT24C_ADDR_BYTES + adata->page_size;
    adata->buf = devm_kmalloc(&client->dev, adata->buf_sz, GFP_KERNEL);
    if (!adata->buf)
        return -ENOMEM;

    // They should point to each other.
    adata->client = client;
    i2c_set_clientdata(client, adata);

    adata->miscdev.minor  = MISC_DYNAMIC_MINOR;
    adata->miscdev.name   = "at24c";
    adata->miscdev.fops   = &at24c_fops;
    adata->miscdev.parent = &client->dev;
    ret = misc_register(&adata->miscdev);
    if (ret) 
    {
        dev_err(&client->dev, "failed to register miscdevice\n");
        return ret;
    }

    dev_info(&client->dev, "AT24C EEPROM probed at 0x%02x\n", client->addr);
    return 0;
}

static void at24c_remove(struct i2c_client *client)
{
    struct at24c *adata = i2c_get_clientdata(client);

    misc_deregister(&adata->miscdev);
    dev_info(&client->dev, "AT24C EEPROM removed\n");
}

static const struct of_device_id at24c_idtable[] = 
{
    { .compatible = "atmel,at24c" },
    {}
};

MODULE_DEVICE_TABLE(of, at24c_idtable);

static struct i2c_driver at24c_driver = 
{
    .driver = 
    {
        .name           = "at24c",
        .of_match_table = at24c_idtable,
    },
    .probe    = at24c_probe,
    .remove   = at24c_remove,
};

module_i2c_driver(at24c_driver);

MODULE_AUTHOR("Ozgur Ayik");
MODULE_DESCRIPTION("Custom AT24C EEPROM misc-device driver");
MODULE_LICENSE("GPL");