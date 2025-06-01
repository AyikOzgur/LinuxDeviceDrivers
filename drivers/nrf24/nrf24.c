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

#define CHECK_BIT_VALUE(u8_val, bit_pos)  ( ((u8_val) >> (bit_pos)) & 0x1U )

#define INIT_NRF24 _IO('G', 0)
#define NRF24_MAX_PAYLOAD 32


struct nrf24_config
{
    u64 ce_gpio;
    u8 tx_address[5];
    u8 rx_address[5];
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
#define REG_CONFIG          0x00
#define REG_EN_AA           0x01
#define REG_EN_RXADDR       0x02
#define REG_SETUP_AW        0x03
#define REG_SETUP_RETR      0X04
#define REG_RF_CH           0x05
#define REG_RF_SETUP        0x06
#define REG_STATUS          0x07
#define REG_OBSERVE_TX      0x08
#define REG_CD              0x09
#define REG_RX_ADDR_P0      0x0A
#define REG_RX_ADDR_P1      0x0B
#define REG_RX_ADDR_P2      0x0C
#define REG_RX_ADDR_P3      0x0D
#define REG_RX_ADDR_P4      0x0E
#define REG_RX_ADDR_P5      0x0F
#define REG_TX_ADDR         0x10
#define REG_RX_PW_P0        0x11
#define REG_RX_PW_P1        0x12
#define REG_RX_PW_P2        0x13
#define REG_RX_PW_P3        0x14
#define REG_RX_PW_P4        0x15
#define REG_RX_PW_P5        0x16
#define REG_FIFO_STATUS     0x17
#define REG_DYNPD           0x1C
#define REG_FEATURE         0x1D
#define TX_ADDR             0x10

#define CONFIG_PRIM_RX    (1<<0)
#define CONFIG_PWR_UP     (1<<1)

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
        dev_err(nrf24->miscdev.this_device,
                "Failed to read %zu bytes @0x%02x: %d\n",
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
    struct spi_transfer xfers[] = 
    {
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
        dev_err(nrf24->miscdev.this_device,
                "Failed to write %zu bytes @0x%02x: %d\n",
                len, start_reg, ret);
    return ret;
}

static int nrf24_set_mode(struct nrf24 *nrf24, bool rx)
{
    u8 cfg;
    int ret;

    ret = nrf24_read_regs(nrf24, REG_CONFIG, &cfg, 1);
    if (ret) return ret;

    if (rx)
        cfg |= CONFIG_PRIM_RX;
    else
        cfg &= ~CONFIG_PRIM_RX;

    ret = nrf24_write_regs(nrf24, REG_CONFIG, &cfg, 1);
    if (ret) return ret;

    return 0;
}

static int nrf24_send(struct nrf24 *nrf24, const u8 *data, size_t len)
{
    int ret;
    u8 status;
    struct spi_device *device = nrf24->device;
    u8 cmd;

    /* TX mode */
    ret = nrf24_set_mode(nrf24, false);
    if (ret) 
    {
        dev_err(nrf24->miscdev.this_device, "set_mode(TX) failed: %d\n", ret);
        return ret;
    }

    /* Clear any old IRQ flags (TX_DS, MAX_RT) */
    status = STATUS_TX_DS | STATUS_MAX_RT;
    ret = nrf24_write_regs(nrf24, REG_STATUS, &status, 1);
    if (ret) 
    {
        dev_err(nrf24->miscdev.this_device, "Clearing STATUS failed: %d\n", ret);
        return ret;
    }

    cmd = FLUSH_TX;
    ret = spi_write(device, &cmd, 1);
    if (ret) 
    {
        dev_err(nrf24->miscdev.this_device, "FLUSH_TX failed: %d\n", ret);
        return ret;
    }

    /* Write the payload in one go: [W_TX_PAYLOAD][data...] */
    u8 txbuf[NRF24_MAX_PAYLOAD + 1];
    txbuf[0] = W_TX_PAYLOAD;
    memcpy(&txbuf[1], data, len);
    ret = spi_write(device, txbuf, len + 1);
    if (ret) 
    {
        dev_err(nrf24->miscdev.this_device, "W_TX_PAYLOAD failed: %d\n", ret);
        return ret;
    }

    /* Drive CE accordingly */
    gpiod_set_value(nrf24->ce_gpio, 1);

    /* It takes 130us until TX mode is ready. */
    usleep_range(130, 140);

    /* Try for 4 ms. */
    int counter = 4;
    while (counter > 0)
    {
        u8 value;
        nrf24_read_regs(nrf24, REG_STATUS, &value, 1);
        if (CHECK_BIT_VALUE(value, 5)) // TX_DS pos is 5.
            break;

        counter--;
        msleep(1);
    }

    if (counter <= 0)
        dev_err(nrf24->miscdev.this_device, "Transmittion failed.\n");

    /* Device should not stay in tx mode then 4 ms.*/
    gpiod_set_value(nrf24->ce_gpio, 0); 

    nrf24_read_regs(nrf24, REG_STATUS, &status, 1);
    status &= ~STATUS_TX_DS;
    nrf24_write_regs(nrf24, REG_STATUS, &status, 1);

    return ret;
}

static int nrf24_receive(struct nrf24 *nrf24, u8 *data, size_t len)
{
    struct spi_device *device = nrf24->device;
    u8 status, cmd;
    int ret;
    
    /* TX mode  */
    ret = nrf24_set_mode(nrf24, true);
    if (ret) return ret;

    /* CE 1 */
    gpiod_set_value(nrf24->ce_gpio, 1);

    /* It takes 130us until RX mode is ready. */
    usleep_range(130, 140);

    int counter = 10;
    while (counter > 0)
    {
        nrf24_read_regs(nrf24, REG_STATUS, &status, 1);
        if (CHECK_BIT_VALUE(status, 6)) 
        {
            break;
        }
        msleep(1);
        counter--;
    }

    if (counter <= 0)
        dev_err(nrf24->miscdev.this_device, "There is no ready data in RX FIFO.\n");
    else
    {
        cmd = R_RX_PAYLOAD;
        ret = spi_write_then_read(device, &cmd, 1, data, len);
        if (ret) 
        {
            dev_err(nrf24->miscdev.this_device, "Error reading RX FIFO.\n");
        }
        else
        {
            status |= STATUS_RX_DR;
            nrf24_write_regs(nrf24, REG_STATUS, &status, 1);
        }
    }

    /* Better to uncomment this line to keep device in standby-1 mode. 
       It is commented because in test app same spi bus is used to send/receive. 
       So we need receiver always in RX mode. */
    //gpiod_set_value(nrf24->ce_gpio, 0);

    return 0;
}

static int nrf24_init_defaults(struct nrf24 *nrf24)
{
    int ret;
    u8 tmp;

    /* Disable auto ack */
    tmp = 0;
    ret = nrf24_write_regs(nrf24, REG_EN_AA, &tmp, 1);
    if (ret) return ret;

    /* Address width = 5 bytes */
    tmp = 0x03;
    ret = nrf24_write_regs(nrf24, REG_SETUP_AW, &tmp, 1);
    if (ret) return ret;

    /* Fixed payload length = 32 on P0 */
    tmp = 32;
    ret = nrf24_write_regs(nrf24, REG_RX_PW_P0, &tmp, 1);
    if (ret) return ret;

    /* CONFIG: PWR_UP, DISABLE CRC */
    tmp = 0;
    tmp |= CONFIG_PWR_UP;
    ret = nrf24_write_regs(nrf24, REG_CONFIG, &tmp, 1);
    if (ret) return ret;

    msleep(2); /* From power down to standby-1 mode, 1.5ms is required. */

    return ret;
}

static int nrf24_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct nrf24 *nrf24 = container_of(misc, struct nrf24, miscdev);
    int ret = 0; 
    // Set nrf24 struct as private data, it is needed in write/read to have spi_device.
    file->private_data = nrf24;

    /* Power down, it will be power up in init. */
    u8 tmp = 0;
    ret = nrf24_write_regs(nrf24, REG_CONFIG, &tmp, 1);
    if (ret) return ret;
    return ret;
}

static int nrf24_release(struct inode *inode, struct file *file)
{
    struct nrf24 *nrf24 = file->private_data;
    u8 tmp = 0;
    int ret = 0;

    /* CE 0 */
    gpiod_set_value(nrf24->ce_gpio, 0);

    /* Power down */
    ret = nrf24_write_regs(nrf24, REG_CONFIG, &tmp, 1);
    if (ret) return ret;
    return 0;
}

static long nrf24_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct nrf24 *nrf24 = file->private_data;
    int ret = 0;

    switch (cmd)
    {
    case INIT_NRF24:
    {
        int bytes = copy_from_user(&nrf24->config, (struct nrf24_config *)arg, sizeof(struct nrf24_config));
        if (bytes < 0)
            return -EFAULT; // Bad address.

        // If it is 0 which means module is transmitter and needs CE.
        nrf24->ce_gpio = gpio_to_desc(nrf24->config.ce_gpio);
        if (!nrf24->ce_gpio)
        {
            dev_err(nrf24->miscdev.this_device, "Error getting gpio descripter : %llu", nrf24->config.ce_gpio);
            return -EINVAL; // Invalid argument.
        }

        ret = gpiod_direction_output(nrf24->ce_gpio, 0); // Default low.
        if (ret)
        {
            dev_err(nrf24->miscdev.this_device, "Error setting pin %llu as output\n", nrf24->config.ce_gpio);
            return ret;
        }

        ret = nrf24_write_regs(nrf24, REG_RX_ADDR_P0, nrf24->config.rx_address, 5);
        ret = nrf24_write_regs(nrf24, REG_TX_ADDR, nrf24->config.tx_address, 5);
        ret = nrf24_init_defaults(nrf24);
        return ret;
    }
    default:
        return -1;
    }

    return ret;
}

static ssize_t nrf24_read(struct file *file,
                          char __user *ubuf,
                          size_t count,
                          loff_t *ppos)
{
    struct nrf24 *nrf = file->private_data;
    u8 buf[NRF24_MAX_PAYLOAD];
    size_t len;
    int ret;

    /* Limit to max payload */
    if (count > NRF24_MAX_PAYLOAD)
        return -EINVAL;
    len = count;

    ret = nrf24_receive(nrf, buf, len);
    if (ret)
        return ret;

    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    return len;
}

static ssize_t nrf24_write(struct file *file,
                           const char __user *ubuf,
                           size_t count,
                           loff_t *ppos)
{
    struct nrf24 *nrf = file->private_data;
    u8 buf[NRF24_MAX_PAYLOAD];
    size_t len;
    int ret;

    /* Limit to max payload */
    if (count > NRF24_MAX_PAYLOAD)
        return -EINVAL;
    len = count;

    if (copy_from_user(buf, ubuf, len))
        return -EFAULT;

    ret = nrf24_send(nrf, buf, len);
    if (ret)
        return ret; 

    return len;  /* Number of bytes sent */
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
    struct nrf24 *nrf24;
    int ret;

    nrf24 = devm_kzalloc(&device->dev, sizeof(*nrf24), GFP_KERNEL);
    if (!nrf24)
        return -ENOMEM;

    /* They should point to each other. */
    nrf24->device = device;
    spi_set_drvdata(device, nrf24);

    device->max_speed_hz = 1000000; // 1Mhz
    device->bits_per_word = 8;
    device->mode = SPI_MODE_0;
    spi_setup(device);

    nrf24->miscdev.minor  = MISC_DYNAMIC_MINOR;

    /* Register all found devices from dt, device name nrf24-<index from 0> */
    u32 device_index = 0;
    of_property_read_u32(device->dev.of_node, "reg", &device_index);
    nrf24->miscdev.name = devm_kasprintf(&device->dev, GFP_KERNEL,
                                         "nrf24-%u",
                                         device_index);
    nrf24->miscdev.fops   = &nrf24_fops;
    nrf24->miscdev.parent = &device->dev;
    ret = misc_register(&nrf24->miscdev);
    if (ret) 
    {
        pr_err("Failed to register nrf24 miscdevice\n");
        return ret;
    }

    return 0;
}

static void nrf24_remove(struct spi_device *device)
{
    struct nrf24 *nrf24 = spi_get_drvdata(device);
    misc_deregister(&nrf24->miscdev);
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