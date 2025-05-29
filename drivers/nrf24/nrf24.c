#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h> 
#include <linux/fs.h> 
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/gpio/consumer.h>


#define INIT_NRF24 _IO('G', 0)


struct nrf24_config
{
    u64 tx_address;
    u64 rx_address;
    u8 ce_gpio;
};

struct nrf24
{
    struct spi_device *device;
    struct miscdevice miscdev;
    struct gpio_desc *ce_gpio;
    struct nrf24_config config;
};


/* Commands */
#define R_REGISTER          0x00   /* 0b000AAAAA, where AAAAA = reg address */
#define W_REGISTER          0x20   /* 0b001AAAAA, where AAAAA = reg address */
#define R_RX_PAYLOAD        0x61
#define W_TX_PAYLOAD        0xA0
#define FLUSH_TX            0xE1
#define FLUSH_RX            0xE2
#define REUSE_TX_PL         0xE3
#define ACTIVATE            0x50
#define R_RX_PL_WID         0x60
#define W_ACK_PAYLOAD       0xA8   /* 0b10101PPP  PPP payload pipe. */
#define W_TX_PAYLOAD_NOACK  0xB0
#define NOP                 0xFF

/* Register map */
#define REG_CONFIG              0x00
#define REG_EA_AA               0x01
#define REG_EN_RXADDR           0x02
#define REG_SETUP_AW            0x03
#define REG_SETUP_RETR          0X04
#define REG_RF_CH               0x05
#define REG_RF_SETUP            0x06
#define REG_STATUS              0x07
#define REG_OBSERVE_TX          0x08
#define REG_CD                  0x09
#define REG_RX_ADDR_P0          0x0A
#define REG_RX_ADDR_P1          0x0B
#define REG_RX_ADDR_P2          0x0C
#define REG_RX_ADDR_P3          0x0D
#define REG_RX_ADDR_P4          0x0E
#define REG_RX_ADDR_P5          0x0F
#define REG_TX_ADDR             0x10
#define REG_RX_PW_P0            0x11
#define REG_RX_PW_P1            0x12
#define REG_RX_PW_P2            0x13
#define REG_RX_PW_P3            0x14
#define REG_RX_PW_P4            0x15
#define REG_RX_PW_P5            0x16
#define REG_FIFO_STATUS         0x17
#define REG_DYNPD               0x1C
#define REG_FEATURE             0x1D


/*———————————————————————————*/
/* Bit-mask helpers             */
/*———————————————————————————*/
#define CONFIG_PRIM_RX    (1<<0)
#define CONFIG_PWR_UP     (1<<1)
#define CONFIG_EN_CRC     (1<<3)

#define STATUS_RX_DR      (1<<6)
#define STATUS_TX_DS      (1<<5)
#define STATUS_MAX_RT     (1<<4)



static int nrf24_read_regs(struct nrf24 *nrf24, u8 start_reg,
                           u8 *buf, size_t len)
{
    struct spi_device *device = nrf24->device;
    u8 cmd = R_REGISTER | (start_reg & 0x1F);
    int ret;

    ret = spi_write_then_read(device, &cmd, 1, buf, len);
    if (ret)
        dev_err(&device->dev,
                "nrf24: failed to read %zu bytes @0x%02x: %d\n",
                len, start_reg, ret);
    return ret;
}

static int nrf24_write_regs(struct nrf24 *nrf24,
                            u8 start_reg,
                            const u8 *buf,
                            size_t len)
{
    struct spi_device *device = nrf24->device;
    u8 cmd = W_REGISTER | (start_reg & 0x1F);
    struct spi_transfer xfers[] = {
        {
            .tx_buf = &cmd,
            .len    = 1,
        },
        {
            .tx_buf = buf,
            .len    = len,
        },
    };
    struct spi_message msg;
    int ret;

    spi_message_init(&msg);
    spi_message_add_tail(&xfers[0], &msg);
    spi_message_add_tail(&xfers[1], &msg);

    ret = spi_sync(device, &msg);
    if (ret)
        dev_err(&device->dev,
                "nrf24: failed to write %zu bytes @0x%02x: %d\n",
                len, start_reg, ret);
    return ret;
}

/*———————————————————————————*/
/* Switch between RX/TX        */
/*———————————————————————————*/
static int nrf24_set_mode(struct nrf24 *nrf24, bool rx)
{
    struct spi_device *device = nrf24->device;

    u8 cfg;
    int ret;

    /* 1) read CONFIG */
    ret = nrf24_read_regs(nrf24, REG_CONFIG, &cfg, 1);
    if (ret) return ret;

    /* 2) flip PRIM_RX */
    if (rx)
        cfg |= CONFIG_PRIM_RX;
    else
        cfg &= ~CONFIG_PRIM_RX;

    /* 3) write it back */
    ret = nrf24_write_regs(nrf24, REG_CONFIG, &cfg, 1);
    if (ret) return ret;

    /* 4) drive CE accordingly */
    //gpio_set_value(ce_gpio, rx ? 1 : 0);
    return 0;
}





static int nrf24_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct nrf24 *nrf24 = container_of(misc, struct nrf24, miscdev);

    // Set nrf24 struct as private data, it is needed in write/read to have spi_device.
    file->private_data = nrf24;


    // Read initial value of 
    /* Read CONFIG register, default value must be 8. */
    //u8 address = CONFIG;
    u8 data;
    int ret = nrf24_read_regs(nrf24, REG_CONFIG, &data, 1);
    if (ret)
        pr_err("nrf24 - No communication with nrf24.\n");

    pr_info("Default value of CONFIG : %d\n", data);

    u8 data2 = 12;
    ret = nrf24_write_regs(nrf24, REG_CONFIG, &data2, 1);
    if (ret)
        pr_err("nrf24 - No communication with nrf24 (write).\n");

    u8 dataRead;
    ret = nrf24_read_regs(nrf24, REG_CONFIG, &dataRead, 1);
    if (ret)
        pr_err("nrf24 - No communication with nrf24.\n");

    pr_info("Aftetwritting value of CONFIG : %d\n", dataRead);

    return 0;
}

static int nrf24_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long nrf24_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    pr_info("nrf24 - ioctl cmd=0x%x, arg=0x%lx\n", cmd, arg);
    struct nrf24 *nrf24 = file->private_data;

    switch (cmd)
    {
    case INIT_NRF24:
    {
        int bytes = copy_from_user(&nrf24->config, (struct nrf24_config *)arg, sizeof(struct nrf24_config));
        if (bytes < 0)
        {
            pr_err("gpioctrl - Bad address");
            return -EFAULT; // Bad address.
        }
        pr_info("nrf24 ce : %d\n", nrf24->config.ce_gpio);

        // If it is <= 0, no ce is needed, module can be used only as rx.
        if (nrf24->config.ce_gpio > 0)
        {
            nrf24->ce_gpio = gpio_to_desc(nrf24->config.ce_gpio);
            if (!nrf24->ce_gpio)
            {
                pr_err("nrf24 - Error getting gpio descripter");
                return -EINVAL; // Invalid argument.
            }
            int ret = gpiod_direction_output(nrf24->ce_gpio, 0); // Default low.
            if (ret)
            {
                pr_err("gpioctrl - Error setting pin %d as output\n", nrf24->config.ce_gpio);
                return ret;
            }
            pr_info("nrf24 - Ce is init..\n");
        }


        break;
    }
    default:
        return 0;
    }
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
    struct nrf24 *adata  = file->private_data;
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

    adata->miscdev.minor  = MISC_DYNAMIC_MINOR;

    /* Register all found devices from dt, device name nrf24-<index from 0> */
    u32 device_index = 0;
    of_property_read_u32(device->dev.of_node, "reg", &device_index);
    adata->miscdev.name = devm_kasprintf(&device->dev, GFP_KERNEL,
                                         "nrf24-%u",
                                         device_index);
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