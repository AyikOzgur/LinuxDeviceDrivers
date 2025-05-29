#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(void)
{
    int nrf24_fd = open("/dev/nrf24-0", O_RDWR);
    if (nrf24_fd < 0) 
    {
        perror("Fail to open device file: /dev/at24c\n");
        return -1;
    } 
    close(nrf24_fd);
    return EXIT_SUCCESS;
}