#include<stdio.h>
#include <unistd.h>

int main(void)
{
    int my_dev = open("/dev/myGpioDevice", 0);
    if (my_dev < 0) 
    {
        perror("Fail to open device file: /dev/myGpioDevice.");
        return -1;
    }

    while(1)
    {
        ioctl(my_dev, 0, 0);
        sleep(1);
        ioctl(my_dev, 0, 1); 
        sleep(1);
    }
    return 0;
}