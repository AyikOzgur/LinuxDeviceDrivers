#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>



int main(void)
{
    int usb_device_fd = open("/dev/custom_usb_device0", O_RDWR);
    if (usb_device_fd < 0) 
    {
        perror("/dev/custom_usb_device0 : ");
        return 1;
    }

    char txBuffer[50];
    char rxBuffer[50];

    int counter = 0;
    while (1)
    {
        int tx_size = sprintf(txBuffer, "Message : %d", counter);
        if (write(usb_device_fd, txBuffer, tx_size) != tx_size) 
        {
            perror("write payload");
            break;
        }
        printf("TX: \"%s\"\n", txBuffer);
        sleep(1);

        ssize_t rd = read(usb_device_fd, rxBuffer, tx_size);
        if (rd < 0) 
        {
            perror("read payload");
            break;
        }
        rxBuffer[rd] = '\0';
        printf("RX: \"%s\"\n", rxBuffer);
        sleep(1);
        counter++;
    }

    close(usb_device_fd);
    return 0;
}
