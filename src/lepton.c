/*
 * FLIR Lepton 3/3.5 VoSPI interface using VSYNC output
 *
 */
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>


int spiFd;                 // File descriptor for SPI device file

/**
 * Initialise the VoSPI interface. frame points to a scratch buffer for
 * use by this code to store a received frame.  Calling code must initialize
 * the Lepton to output VSYNC pulses.
 */
int vospi_init(int fd, uint32_t speed)
{
    // Set the various SPI parameters
    uint16_t mode = SPI_MODE_3;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1)
    {
        return -1;
    }

    uint8_t bits = 8;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1)
    {
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1)
    {
        return -1;
    }

    // Save the SPI file descriptor for use in the ISR
    spiFd = fd;

    return 1;
}

void send_image(uint8_t * image, uint32_t len)
{
    static uint32_t image_number = 0;
    char filename[100];
    snprintf(filename, 100, "img%03d.bin", image_number);
    image_number++;
    printf("saving %s\n", filename);

    // Save image to file
    FILE * f = fopen(filename, "wb");
    if (f != NULL)
    {
        fwrite(image, len, 1, f);
    }
    fclose(f);
}

void continuous_transfer(int num_images)
{
    // Open a file
    uint8_t packet[200];
    uint8_t temp[60][160];
    uint8_t segments[4][60][160];
    uint16_t segment_index = 0;
    uint16_t packet_number = 0;
    uint32_t img_count = 0;
    uint32_t prev_packet_number = 0;
    uint32_t failed_attempts = 0;
    uint8_t segment_mask = 0;

    while (img_count < num_images)
    {
        if (read(spiFd, &packet, 164) > 0)
        {
            if ((packet[0] & 0x0F) != 0x0F)
            {
                packet_number = packet[1];
                if ((packet_number == 0) && (prev_packet_number > 0) && (segment_index > 0))
                {
                    // Copy the temp data into the segment
                    memcpy(segments[segment_index - 1], temp, 60*160);

                    if ((segment_mask & 0x1E) == 0x1E)
                    {
                        send_image(&segments[0][0][0], 4 * 60 * 160);
                        segment_mask = 0;
                    }
                }
                else
                {
                    failed_attempts++;
                }
                //printf("%d:%d\n", segment_index, packet_number);
                if (packet_number < 60)
                    memcpy(temp[packet_number], &packet[4], 160);

                prev_packet_number = packet_number;
                if (packet_number == 20)
                {
                    segment_index = (packet[0] >> 4) & 0x7;
                    segment_mask |= 1 << segment_index;
                    //printf("%d\n", segment_index);
                    if (segment_index == 0)
                    {
                        failed_attempts++;
                    }
                }
            }
            else
            {
                failed_attempts++;
            }

            if (failed_attempts > 1000)
            {
                printf("Resync\n");
                failed_attempts = 0;
                segment_index = 0;
                segment_mask = 0;
                sleep(1);
            }
        }
        else
        {
            printf("Failed to read SPI data\n");
            break;
        }
    }
}

void big_ass_transfer(int size)
{
    // Open a file
    uint8_t * data = malloc(size);
    if (data != NULL)
    {
        printf("Reading %d bytes\n", size);
        int offset = 0;
        #define NUM_OF_XFER 10
        struct spi_ioc_transfer xfer[NUM_OF_XFER];

        for (int i = 0; i < NUM_OF_XFER; i++)
        {
            xfer[i].rx_buf = &data[i*164];
            xfer[i].tx_buf = NULL;
            xfer[i].len = 164;
            xfer[i].speed_hz = 200000;
            xfer[i].delay_usecs = 10;
            xfer[i].bits_per_word = 8;
            xfer[i].cs_change = 1;
            xfer[i].tx_nbits = 8;
            xfer[i].rx_nbits = 8;

        }

        int ret = ioctl(spiFd, SPI_IOC_MESSAGE(NUM_OF_XFER), xfer);
        printf("IOCTL return value %d\n", ret);

        // while (offset < size)
        // {
        //     //int ret = read(spiFd, &data[offset], 164);
        //     int ret = ioctl(spiFd, SPI_IOC_MESSAGE(NUM_OF_XFER), xfer);
        //     printf("IOCTL return value %d\n", ret);
        //     if (ret == 164)
        //     {
        //         offset += 164;
        //     }
        //     else
        //     {
        //         printf("Failed to read, ret: %d\n", ret);
        //     }
        // }
        printf("Read successful\n");
        // Save image to file
        FILE * f = fopen("spi.txt", "w");
        if (f != NULL)
        {
            int packet_len = 0;
            for (int i = 0; i < size; i++)
            {
                char temp[100];
                snprintf(temp, 100, "%02X ", data[i]);
                fwrite(temp, strlen(temp), 1, f);
                packet_len++;
                if (packet_len >= 164)
                {
                    fputs("\n", f);
                    packet_len = 0;
                }
            }
        }
        fclose(f);
    }
}


int main(int argc, char *argv[])
{
    int baudrate = 500000;
    if (argc == 2)
    {
        sscanf(argv[1], "%d", &baudrate);
        printf("Setting baud rate to %d\n", baudrate);
    }

    printf("Attempting to read some spi data\n");
    spiFd = 0;
    if ((spiFd = open("/dev/spidev0.0", O_RDWR)) < 0)
    {
       printf("Error opening spidev\n");
    }

    if (vospi_init(spiFd, baudrate) < 1)
    {
        printf("Failed to init SPI\n");
    }

    //continuous_transfer(60);
    big_ass_transfer(164000);

    if (spiFd > 0)
    {
        close(spiFd);
    }
}

