#include<stdio.h>
#include <unistd.h>

int main(void)
{
    int myDev = open("/dev/myGpioDevice", 0);
    if (myDev < 0) 
    {
        perror("Fail to open device file: /dev/myGpioDevice.");
        return -1;
    }

    // Blink the led.
    while(1)
    {
        ioctl(myDev, 0, 0);
        sleep(1);
        ioctl(myDev, 0, 1); 
        sleep(1);
    }
    return 0;
}