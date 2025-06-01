#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/printk.h>

#define UART_CONFIG _IOW('U', 1, UARTConfig)

/** @brief Configuration parameters for UART
 */
typedef struct
{
    int txPin;
    int rxPin;
    int baudRate;

    // These are gonna be implemented coming version. 
    // Current version params: 8N1
    int dataBits;
    int stopBits;
    int parity;
    char isInverted;
} UARTConfig;

UARTConfig uart_params;
static struct hrtimer tx_hrtimer;
static struct hrtimer rx_hrtimer;
static char tx_buffer[256]; // Test size
static int tx_buffer_pos = 0;
static int tx_buffer_len = 0;
static int tx_in_progress = 0;
static int tx_bit_pos = 0;


static int rx_gpio_irq;
static char rx_buffer[256]; // Test size
static int rx_buffer_pos = 0;
static int bit_duration = 0;
static int rx_bit_pos = 0;

static enum hrtimer_restart tx_hrtimer_handler(struct hrtimer *timer);
static enum hrtimer_restart rx_hrtimer_handler(struct hrtimer *timer);
static irqreturn_t rx_irq_handler(int irq, void *dev_id);

static int isInit = 0;

int initPeripherals(UARTConfig *uart_params) 
{
    int ret;
    // Initialize GPIOs based on the received parameters
    ret = gpio_request(uart_params->txPin, "GPIO_TX");
    if (ret) 
    {
        pr_err("Failed to request GPIO_TX (pin %d), error: %d\n", uart_params->txPin, ret);
        return ret;
    }
    gpio_direction_output(uart_params->txPin, 1);

    ret = gpio_request(uart_params->rxPin, "GPIO_RX");
    if (ret) 
    {
        pr_err("Failed to request GPIO_RX (pin %d), error: %d\n", uart_params->rxPin, ret);
        gpio_free(uart_params->txPin);
        return ret;
    }

    gpio_direction_input(uart_params->rxPin);

    pr_info("Requested GPIOs - TX: %d, RX: %d\n", uart_params->txPin, uart_params->rxPin);

    // Request IRQ for RX
    rx_gpio_irq = gpio_to_irq(uart_params->rxPin);
    if (rx_gpio_irq < 0) 
    {
        pr_err("Failed to map GPIO %d to IRQ: %d\n", uart_params->rxPin, rx_gpio_irq);
        gpio_free(uart_params->txPin);
        gpio_free(uart_params->rxPin);
        return ret;
    }

    pr_info("Mapped GPIO %d to IRQ %d\n", uart_params->rxPin, rx_gpio_irq);

    ret = request_irq(rx_gpio_irq, rx_irq_handler, IRQF_TRIGGER_FALLING, "soft_uart_rx", NULL);
    if (ret) 
    {
        pr_err("Failed to request IRQ %d for RX: %d\n", rx_gpio_irq, ret);
        gpio_free(uart_params->txPin);
        gpio_free(uart_params->rxPin);
        return ret;
    } 
    else 
    {
        pr_info("Successfully requested IRQ %d for RX\n", rx_gpio_irq);
    }

    // Calculate bit duration for rx timer interrupt for proper sampling.
    bit_duration = 1000000 / uart_params->baudRate;
    isInit = 1;

    // Initialize high-resolution timers
    hrtimer_init(&tx_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    tx_hrtimer.function = tx_hrtimer_handler;
    
    hrtimer_init(&rx_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    rx_hrtimer.function = rx_hrtimer_handler;

    pr_info("UART initialized successfully\n");
    return 0;
}

static enum hrtimer_restart tx_hrtimer_handler(struct hrtimer *timer)
{
    static char current_byte;

    if (tx_buffer_pos >= tx_buffer_len)
    {
        // No more data to send
        tx_in_progress = 0;
        return HRTIMER_NORESTART;
    }

    if (tx_bit_pos == 0)
    {
        // Start bit
        gpio_set_value(uart_params.txPin, 0);
        current_byte = tx_buffer[tx_buffer_pos];
    }
    else if (tx_bit_pos <= 8)
    {
        // Data bit not inverted.
        gpio_set_value(uart_params.txPin, (current_byte >> (tx_bit_pos - 1)) & 0x01);
    }
    else if (tx_bit_pos == 9)
    {
        gpio_set_value(uart_params.txPin, 1); // Stop bit
        tx_bit_pos = -1; // Prepare for the next byte
        tx_buffer_pos++;
        if (tx_buffer_pos >= tx_buffer_len) 
        {
            tx_in_progress = 0;
            return HRTIMER_NORESTART;
        }
    }

    tx_bit_pos++;
    hrtimer_forward_now(&tx_hrtimer, ktime_set(0, bit_duration * 1000)); // bit_duration in microseconds to nanoseconds
    return HRTIMER_RESTART;
}

static void start_tx(void)
{
    if (tx_in_progress) 
        return;
    tx_in_progress = 1;
    tx_buffer_pos = 0;
    tx_bit_pos = 0;
    // Ensure GPIO is in idle state (stop bit = 1)
    gpio_set_value(uart_params.txPin, 1);

    if (tx_buffer_len > 0) 
    {
        hrtimer_start(&tx_hrtimer, ktime_set(0, bit_duration * 1000), HRTIMER_MODE_REL);
    }
}

static irqreturn_t rx_irq_handler(int irq, void *dev_id)
{
    disable_irq_nosync(rx_gpio_irq); // Disable gpio interrupt to avoid multiple triggers
    // Start bit detected.
    hrtimer_start(&rx_hrtimer, ktime_set(0, bit_duration * 1000), HRTIMER_MODE_REL);
    return IRQ_HANDLED;
}

static enum hrtimer_restart rx_hrtimer_handler(struct hrtimer *timer)
{
    static char current_byte = 0;

    // Read data bits
    current_byte >>= 1;
    if (gpio_get_value(uart_params.rxPin))
    {
        current_byte |= 0x80;
    }

    rx_bit_pos++;

    if (rx_bit_pos < 8)
    {
        // Read next bit
        hrtimer_forward_now(&rx_hrtimer, ktime_set(0, bit_duration * 1000));
        return HRTIMER_RESTART;
    }
    else 
    {
        rx_buffer[rx_buffer_pos++] = current_byte;
        current_byte = 0;
        rx_bit_pos = 0; // Reset bit position
        enable_irq(rx_gpio_irq); // Re-enable GPIO interrupt for the next byte
        return HRTIMER_NORESTART;
    }
}

static int open(struct inode *inode, struct file *file)
{
    pr_info("softwareUART device file opened.\n");
    return 0; // No need to any operation for now.
}

static int close(struct inode *inode, struct file *file)
{
    pr_info("softwareUART device file closed.\n");
    if (isInit)
    {
        free_irq(rx_gpio_irq, NULL);
        gpio_free(uart_params.txPin);
        gpio_free(uart_params.rxPin);
        hrtimer_cancel(&tx_hrtimer);
        hrtimer_cancel(&rx_hrtimer);
        isInit = 0;
    }

    return 0; // No need to any operation for now.
}

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int bytes = 0;
    switch (cmd)
    {
    case UART_CONFIG:
    {
        bytes = copy_from_user(&uart_params, (UARTConfig *)arg, sizeof(UARTConfig));
        if (bytes < 0)
        {
            return -EFAULT;
        }
        pr_info("TX Pin: %d\n", uart_params.txPin);
        pr_info("RX Pin: %d\n", uart_params.rxPin);
        pr_info("Baud Rate: %d\n", uart_params.baudRate);
        pr_info("(Not implemented) Data Bits: %d\n", uart_params.dataBits);
        pr_info("(Not implemented) Stop Bits: %d\n", uart_params.stopBits);
        pr_info("(Not implemented) Parity: %d\n", uart_params.parity);
        pr_info("(Not implemented) Inverted: %c\n", uart_params.isInverted);

        return (initPeripherals(&uart_params));
    }  
    }

    return -1;
}

static ssize_t read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int bytes_to_read = 0;

    // Check if there is data to read
    if (rx_buffer_pos == 0)
        return 0; // No data available

    // Determine the number of bytes to read
    bytes_to_read = min(count, (size_t)rx_buffer_pos);

    // Copy data to userspace
    if (copy_to_user(buf, rx_buffer, bytes_to_read))
        return -1;

    // Shift remaining data in the buffer
    memmove(rx_buffer, rx_buffer + bytes_to_read, rx_buffer_pos - bytes_to_read);
    rx_buffer_pos -= bytes_to_read;

    return bytes_to_read;
}

static ssize_t write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    // Check if there is enough space in the buffer
    if (count > sizeof(tx_buffer))
        return -1; // Not enough space

    // Copy data from userspace
    if (copy_from_user(tx_buffer, buf, count))
        return -1;

    // Set the buffer length and start transmission
    tx_buffer_len = count;
    start_tx();

    return count;
}

static const struct file_operations miscDeviceFileOperations =
{
    .owner = THIS_MODULE,
    .open = open,
    .release = close,
    .unlocked_ioctl = ioctl,
    .read = read,
    .write = write,
};

static struct miscdevice miscDevice =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "softwareUART",
    .fops = &miscDeviceFileOperations,
};

static int __init init(void)
{
    // Register device with kernel.
    int ret = misc_register(&miscDevice);
    if (ret != 0)
    {
        pr_err("Could not register misc device : %s\n", miscDevice.name);
        return ret;
    }

    // Device info.
    pr_info("%s device got minor %d\n", miscDevice.name, miscDevice.minor);

    return 0;
}

static void __exit customExit(void)
{
    pr_info("softwareUART device removed\n");
    
    // Unregister device from kernel
    misc_deregister(&miscDevice);

    if (isInit)
    {
        free_irq(rx_gpio_irq, NULL);
        gpio_free(uart_params.txPin);
        gpio_free(uart_params.rxPin);
        hrtimer_cancel(&tx_hrtimer);
        hrtimer_cancel(&rx_hrtimer);
        isInit = 0;
    }
}

module_init(init);
module_exit(customExit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Ozgur Ayik");
MODULE_DESCRIPTION("Software UART Device Module");