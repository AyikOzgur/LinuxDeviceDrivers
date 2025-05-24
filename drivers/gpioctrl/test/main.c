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
    int myGpioDriver = open("/dev/my_gpio_driver", 0);
    if (myGpioDriver < 0) 
    {
        perror("Fail to open device file: /dev/mydev.");
        return -1;
    } 

    GPIOConfig_t gpioParams;
    // Gpio numbers of pins will be platform dependent.
    gpioParams.inputPin = 591;  // GPIO20 on raspberry pi.
    gpioParams.outputPin = 592; // GPIO21 on raspberry pi.
    
    ioctl(myGpioDriver, INIT_GPIO, &gpioParams);
    unsigned long val = 0;
    while(1)
    {
        ioctl(myGpioDriver, READ_PIN, &val);
        ioctl(myGpioDriver, WRITE_PIN, &val);
    }

    close(myGpioDriver);
    return 0;
}