// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/slab.h>

struct at24c_data
{
    struct i2c_client  *client;
    struct miscdevice   miscdev;
};

static int at24c_open(struct inode *inode, struct file *file)
{
    pr_info("at24c - device opened\n");
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

static const struct file_operations at24c_fops = 
{
    .owner          = THIS_MODULE,
    .open           = at24c_open,
    .release        = at24c_release,
    .unlocked_ioctl = at24c_ioctl,
};

static int at24c_probe(struct i2c_client *client)
{
    pr_info("at24c - Probe.\n");
    struct at24c_data *adata;
    int ret;

    adata = devm_kzalloc(&client->dev, sizeof(*adata), GFP_KERNEL);
    if (!adata)
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
    struct at24c_data *adata = i2c_get_clientdata(client);

    misc_deregister(&adata->miscdev);
    dev_info(&client->dev, "AT24C EEPROM removed\n");
}

static const struct of_device_id at24c_of_match[] = 
{
    { .compatible = "atmel,atc24c" },
    {}
};

MODULE_DEVICE_TABLE(of, at24c_of_match);

static struct i2c_driver at24c_driver = 
{
    .driver = 
    {
        .name           = "at24c",
        .of_match_table = at24c_of_match,
    },
    .probe    = at24c_probe,
    .remove   = at24c_remove,
};

module_i2c_driver(at24c_driver);

MODULE_AUTHOR("Ozgur Ayik");
MODULE_DESCRIPTION("Custom AT24C EEPROM misc-device driver");
MODULE_LICENSE("GPL");