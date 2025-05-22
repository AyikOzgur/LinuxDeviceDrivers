#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#define INIT_GPIO _IO('G', 0)
#define READ_PIN _IO('G', 1)
#define WRITE_PIN _IO('G', 2)

typedef struct 
{
   int inputPin;
   int outputPin;
} GPIOConfig_t;



int main(void)
{
    /* First, you need run "mknod /dev/mydev c 202 0" to create /dev/mydev */
    int myDevice = open("/dev/my_gpio_driver", 0);
    if (myDevice < 0) 
    {
        perror("Fail to open device file: /dev/mydev.");
        return -1;
    } 

    GPIOConfig_t gpioParams;
    gpioParams.inputPin = 21;
    gpioParams.outputPin = 22;
    
    ioctl(myDevice, INIT_GPIO, &gpioParams); /* cmd = 100, arg = 110. */
    return 0;

    while(1)
    {
        ioctl(myDevice, 100, 110); /* cmd = 100, arg = 110. */
    }
    ioctl(myDevice, 100, 110); /* cmd = 100, arg = 110. */
    close(myDevice);


    return 0;
}