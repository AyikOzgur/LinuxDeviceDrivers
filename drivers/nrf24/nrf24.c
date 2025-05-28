#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h> 
#include <linux/fs.h> 
#include <linux/delay.h>
#include <linux/mutex.h>



#define R_REGISTER    0x00   /* 0b000AAAAA, where AAAAA = reg address */
#define W_REGISTER    0x01   /* 0b001AAAAA, where AAAAA = reg address */
#define R_RX_PAYLOAD  0x61
#define W_TX_PAYLOAD  0xA0
#define FLUSH_TX 
#define FLUSH_RX 

static int nrf24_read_regs(struct spi_device *spi, u8 start_reg,
                           u8 *buf, size_t len)
{
    u8 cmd = R_REGISTER | (start_reg & 0x1F);
    int ret;

    ret = spi_write_then_read(spi, &cmd, 1, buf, len);
    if (ret)
        dev_err(&spi->dev,
                "nrf24: failed to read %zu bytes @0x%02x: %d\n",
                len, start_reg, ret);
    return ret;
}



struct nrf24
{
    struct spi_device *device;
    struct miscdevice miscdev;
};

static int nrf24_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct nrf24 *adata = container_of(misc, struct nrf24, miscdev);

    // Set nrf24 struct as private data, it is needed in write/read to have i2c_client.
    file->private_data = adata;
    return 0;
}

static int nrf24_release(struct inode *inode, struct file *file)
{
    pr_info("nrf24 - device closed\n");
    return 0;
}

static long nrf24_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    pr_info("nrf24 - ioctl cmd=0x%x, arg=0x%lx\n", cmd, arg);
    return 0;
}

static ssize_t nrf24_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct nrf24 *adata = file->private_data;
    return 0;
}

static ssize_t nrf24_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos)
{
    struct at24c *adata  = file->private_data;
    return 0;
}

static loff_t nrf24_llseek(struct file *file, loff_t offset, int whence)
{
    return generic_file_llseek(file, offset, whence);
}

static const struct file_operations nrf24_fops = 
{
    .owner          = THIS_MODULE,
    .open           = nrf24_open,
    .release        = nrf24_release,
    .unlocked_ioctl = nrf24_ioctl,
    .read           = nrf24_read,
    .write          = nrf24_write,
    .llseek         = nrf24_llseek,
};

static int nrf24_probe(struct spi_device *device)
{
    pr_info("nrf24 - Probe.\n");
    struct nrf24 *adata;
    int ret;

    adata = devm_kzalloc(&device->dev, sizeof(*adata), GFP_KERNEL);
    if (!adata)
        return -ENOMEM;

    // They should point to each other.
    adata->device = device;
    spi_set_drvdata(device, adata);

    // Start init of spi
    device->max_speed_hz = 1000000; // 1Mhz
    device->bits_per_word = 8;
    device->mode = SPI_MODE_0;
    ret = spi_setup(device);
    if (ret)
        pr_err("nrf24 - spi_setup error.\n");

    /* Read CONFIG register, default value must be 8. */
    u8 address = 0x00;  /* read register 0x00 */
    u8 data;
    ret = nrf24_read_regs(device, address, &data, 1);
    if (ret)
        pr_err("nrf24 - No communication with nrf24.\n");

    if (data != 8)
        pr_err("nrf24 - Default value of CONFIG register did not match with expected value.\n");

    adata->miscdev.minor  = MISC_DYNAMIC_MINOR;
    adata->miscdev.name   = "nrf24";
    adata->miscdev.fops   = &nrf24_fops;
    adata->miscdev.parent = &device->dev;
    ret = misc_register(&adata->miscdev);
    if (ret) 
    {
        dev_err(&device->dev, "failed to register miscdevice\n");
        return ret;
    }

    dev_info(&device->dev, "nrf24 - Probed probably.\n");
    return 0;
}

static void nrf24_remove(struct spi_device *device)
{
    struct nrf24 *adata = spi_get_drvdata(device);

    misc_deregister(&adata->miscdev);
    dev_info(&device->dev, "nrf removed\n");
}

static const struct of_device_id nrf24_idtable[] = 
{
    { .compatible = "nordic,nrf24" },
    {}
};

/* Bind mechanism based on DT. */
MODULE_DEVICE_TABLE(of, nrf24_idtable);

static struct spi_driver nrf24_driver = 
{
    .driver = 
    {
        .name           = "nrf24",
        .of_match_table = nrf24_idtable,
    },
    .probe    = nrf24_probe,
    .remove   = nrf24_remove,
};

module_spi_driver(nrf24_driver);

MODULE_AUTHOR("Ozgur Ayik");
MODULE_DESCRIPTION("Custom nrf24 misc-device driver");
MODULE_LICENSE("GPL");