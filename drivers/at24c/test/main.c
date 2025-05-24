#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(void)
{
    int at24c_fd = open("/dev/at24c", O_RDWR);
    if (at24c_fd < 0) 
    {
        perror("Fail to open device file: /dev/at24c\n");
        return -1;
    } 

    const int bufferSize = 256000; // 256KB capacity of at24c256.
    char *inputData = malloc(bufferSize);
    char *outputData = malloc(bufferSize);

    for (int i = 0; i < bufferSize; i++)
        inputData[i] = (char)(i + 50);

    // Put address to 0x10.
    lseek(at24c_fd, 0x00, SEEK_SET);

    int bytesWritten = write(at24c_fd, inputData, bufferSize);
    if (bytesWritten != bufferSize)
    {
        printf("%d bytes written\n", bytesWritten);
        printf("Writting to at24c failed.\n");
        close(at24c_fd);
        return -1;
    }
    else
    {
        printf("%d bytes written.\n", bytesWritten);
    }

    // Put address again to 0x10.
    lseek(at24c_fd, 0x00, SEEK_SET);

    sleep(1);
    int readBytes = read(at24c_fd, outputData, bufferSize);
    if (readBytes != bufferSize)
    {
        printf("Reading from at24c failed.\n");
        close(at24c_fd);
        return -1;
    }
    else
    {
        printf("%d bytes read.\n", readBytes);
        outputData[readBytes] = '\0';
    }

    /* Compare with memcmp() */
    if (memcmp(inputData, outputData, bufferSize) == 0) 
    {
        printf("Success: buffers match!\n");
    } 
    else 
    {
        printf("Error: buffers differ!\n");
        for (int i = 0; i < bufferSize; i++) 
        {
            if (inputData[i] != outputData[i]) 
            {
                printf(" First mismatch at byte %d: wrote 0x%02x, read 0x%02x\n",
                       i, (unsigned char)inputData[i], (unsigned char)outputData[i]);
                break;
            }
        }
    }

    free(inputData);
    free(outputData);
    return EXIT_SUCCESS;
}