#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

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


int main()
{
    // Open the device file
    int fd = open("/dev/softwareUART", O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open the device file");
        return errno;
    }

    // Create a UARTConfig struct
    UARTConfig uart_params;
    uart_params.txPin = 103;
    uart_params.rxPin = 114;
    uart_params.baudRate = 9600;
    uart_params.dataBits = 8;
    uart_params.stopBits = 1;
    uart_params.parity = 0;
    uart_params.isInverted = 0;

    // Write the UARTConfig struct to the device file
    if (ioctl(fd, UART_CONFIG, &uart_params) < 0)
    {
        perror("Failed to initialize UART");
        close(fd);
        return errno;
    }

    while(1)
    {
        char buffer[] = "Hello from user space!\n";
        
        write(fd, buffer, sizeof(buffer));

        sleep(1);
    }

    return 0;
}