#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    /* First, you need run "mknod /dev/mydev c 202 0" to create /dev/mydev */
    int myDevice = open("/dev/mydev", 0);
    if (myDevice < 0) 
    {
        perror("Fail to open device file: /dev/mydev.");
    } 
    else 
    {
        ioctl(myDevice, 100, 110); /* cmd = 100, arg = 110. */
        close(myDevice);
    }

    return 0;
}