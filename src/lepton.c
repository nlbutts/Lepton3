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

                    if (segment_index == 4)
                    {
                        send_image((uint8_t*)segments, 4 * 60 * 160);
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
                    if (segment_index == 0)
                    {
                        failed_attempts++;
                    }
                }
            }

            if (failed_attempts > 500)
            {
                printf("Resync\n");
                failed_attempts = 0;
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

int main()
{
    printf("Attempting to read some spi data\n");
    spiFd = 0;
    if ((spiFd = open("/dev/spidev0.0", O_RDWR)) < 0)
    {
       printf("Error opening spidev\n");
    }

    if (vospi_init(spiFd, 24000000) < 1)
    {
        printf("Failed to init SPI\n");
    }

    continuous_transfer(60);

    if (spiFd > 0)
    {
        close(spiFd);
    }
}

