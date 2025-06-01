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
    uint64_t ce_gpio; /* GPIO number of CE pin */
    uint8_t tx_address[5];
    uint8_t rx_address[5];
};

int main(void)
{
    int tx_fd = open("/dev/nrf24-0", O_RDWR);
    if (tx_fd < 0) 
    {
        perror("open /dev/nrf24-0");
        return 1;
    }

    /* small pause so the module side can settle */
    sleep(1);

    int rx_fd = open("/dev/nrf24-1", O_RDWR);
    if (rx_fd < 0) 
    {
        perror("open /dev/nrf24-1");
        close(tx_fd);
        return 1;
    }

    /* configure the transmitter */
    struct nrf24_config tx_cfg = 
    {
        .ce_gpio    = 591,   /* GPIO20 on raspberry pi */
        .tx_address = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE5 },
        .rx_address = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 }, /* Not used in this test app. */
    };
    if (ioctl(tx_fd, INIT_NRF24, &tx_cfg) < 0) 
    {
        perror("ioctl INIT_NRF24 tx");
        goto cleanup;
    }

    /* configure the receiver */
    struct nrf24_config rx_cfg = 
    {
        .ce_gpio  = 592,    /* GPIO21 on raspbbery pi */
        .tx_address = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 }, /* Not used in this test app. */
        .rx_address = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE5 },
    };
    if (ioctl(rx_fd, INIT_NRF24, &rx_cfg) < 0) 
    {
        perror("ioctl INIT_NRF24 rx");
        goto cleanup;
    }


    char payload[PAYLOAD_SIZE];
    const char *msg = "Hello, nRF24!";
    memset(payload, 0, PAYLOAD_SIZE);
    strncpy(payload, msg, PAYLOAD_SIZE - 1);

    // Reader buffer.
    char buf[PAYLOAD_SIZE];

    while (1)
    {
        if (write(tx_fd, payload, PAYLOAD_SIZE) != PAYLOAD_SIZE) 
        {
            perror("write payload");
            break;
        }
        printf("TX: sent \"%s\"\n", payload);
        sleep(1);

        ssize_t rd = read(rx_fd, buf, PAYLOAD_SIZE);
        if (rd < 0) 
        {
            perror("read payload");
            break;
        }
        buf[rd < PAYLOAD_SIZE ? rd : PAYLOAD_SIZE-1] = '\0';
        printf("RX: got %zd bytes: \"%s\"\n", rd, buf);
        sleep(1);
    }

cleanup:
    close(tx_fd);
    close(rx_fd);
    return 0;
}
