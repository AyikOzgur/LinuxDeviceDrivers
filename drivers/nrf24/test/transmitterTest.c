#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#define INIT_NRF24    _IO('G', 0)
#define PAYLOAD_SIZE  32

struct nrf24_config 
{
    uint64_t type;  /* 0 - Transmitter, 1 - Receiver */
    uint64_t ce_gpio;
};

int main(void)
{
    int tx_fd = open("/dev/nrf24-1", O_RDWR);
    if (tx_fd < 0) {
        perror("open /dev/nrf24-1");
        return 1;
    }

    /* small pause so the module side can settle */
    sleep(1);

    /* configure the transmitter */
    struct nrf24_config tx_cfg = 
    {
        .type = 0,           /* 0 - Transmitter. */
        .ce_gpio    = 592,   /* e.g. GPIO20 */
    };

    if (ioctl(tx_fd, INIT_NRF24, &tx_cfg) < 0) 
    {
        perror("ioctl INIT_NRF24 tx");
        goto cleanup;
    }
    else
    {
        printf("nrf24-0 is init as tx.\n");
    }

    /* —— child: transmitter loop —— */
    char payload[PAYLOAD_SIZE];
    const char *msg = "Hello, nRF24!";
    /* prepare the 32-byte packet */
    memset(payload, 0, PAYLOAD_SIZE);
    strncpy(payload, msg, PAYLOAD_SIZE - 1);

    while (1)
    {        
        if (write(tx_fd, payload, PAYLOAD_SIZE) != PAYLOAD_SIZE) 
        {
            perror("write payload");
            break;
        }
        printf("TX: sent \"%s\"\n", payload);
        sleep(1);
    }


cleanup:
    close(tx_fd);
    return 0;
}
