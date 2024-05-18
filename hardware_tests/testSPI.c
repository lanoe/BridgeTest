#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <sys/ioctl.h>

#define BMP280_CHIP_ID 0x58

enum {
    INIT,
    READY,
    CALIB_T1_REQ,
    CALIB_T1_RESP,
    CALIB_T2_REQ,
    CALIB_T2_RESP,
    CALIB_T3_REQ,
    CALIB_T3_RESP,
    ID_REQ,
    ID_RESP,
    CTRL_REQ,
    CTRL_RESP,
    MEAS_REQ,
    MEAS_RESP,
    STATUS_REQ,
    STATUS_RESP,
    TEMP_REQ,
    TEMP_RESP,
    END
};

#define BUFFER_SIZE 3
#define CTRL_MEAS 0x21

int main(int argc, char *argv[])
{
    int fd, ret=0;
    char *portname = "/dev/spidev1.0";

    if (argc > 1) portname = argv[1];

    fprintf(stderr, "With BMP280 Digital Sensor\n");
    fprintf(stderr, "Test SPI on %s ...\n", portname);

    fd = open(portname, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", portname, strerror(errno));
        return -1;
    }

    static uint8_t mode = 3;
    static uint8_t bits = 8;
    static uint32_t speed = 1000000;

    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1) {
        fprintf(stderr, "Failed to ioctl SPI_IOC_WR_MODE: %s\n", strerror(errno));
        return -1;
    }
    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1) {
        fprintf(stderr, "Failed to ioctl SPI_IOC_RD_MODE: %s\n", strerror(errno));
        return -1;
    }

    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1) {
        fprintf(stderr, "Failed to ioctl SPI_IOC_WR_BITS_PER_WORD: %s\n", strerror(errno));
        return -1;
    }
    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1) {
        fprintf(stderr, "Failed to ioctl SPI_IOC_RD_BITS_PER_WORD: %s\n", strerror(errno));
        return -1;
    }

    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1) {
        fprintf(stderr, "Failed to ioctl SPI_IOC_WR_MAX_SPEED_HZ: %s\n", strerror(errno));
        return -1;
    }
    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1) {
        fprintf(stderr, "Failed to ioctl SPI_IOC_WR_MAX_SPEED_HZ: %s\n", strerror(errno));
        return -1;
    }

    printf("spi mode: %d\n", mode);
    printf("bits per word: %d\n", bits);
    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    int status = INIT;
    unsigned int len = 0;
    unsigned char tx_rx[BUFFER_SIZE];
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_rx,
        .rx_buf = (unsigned long)tx_rx,
        .len = BUFFER_SIZE,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    uint16_t dig_T1=0;
    int16_t dig_T2=0, dig_T3=0;

    do {

	if (status != END) {
            // SEND ....
            tx_rx[0] = tx_rx[1] = tx_rx[2] = 0xFF;

            if (status == INIT) {
#ifdef DEBUG
                fprintf(stderr, "Get Status ...\n");
#endif
		tx_rx[0] = 0xF3 | 0x80;
            }
	    else if (status == READY) {
#ifdef DEBUG
                fprintf(stderr, "Get Calib T1 ...\n");
#endif
		tx_rx[0] = 0x88;
                status = CALIB_T1_REQ;
            }
	    else if (status == CALIB_T1_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Calib T2 ...\n");
#endif
		tx_rx[0] = 0x8A;
                status = CALIB_T2_REQ;
            }
	    else if (status == CALIB_T2_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Calib T3 ...\n");
#endif
		tx_rx[0] = 0x8C;
                status = CALIB_T3_REQ;
            }
            else if (status == CALIB_T3_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Identifier ...\n");
#endif
		tx_rx[0] = 0xD0 | 0x80;
                status = ID_REQ;
            }
            else if (status == ID_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Configuration ...\n");
#endif
		tx_rx[0] = 0xF5 | 0x80;
                status = CTRL_REQ;
            }
            else if (status == CTRL_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Control Measure ...\n");
#endif
                tx_rx[0] = 0xF4 & 0x7F;
                tx_rx[1] = CTRL_MEAS;
                status = MEAS_REQ;
            }
            else if (status == MEAS_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Status ...\n");
#endif
		tx_rx[0] = 0xF3 | 0x80;
                status = STATUS_REQ;
            }
            else if (status == STATUS_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Temperature ...\n");
#endif
		tx_rx[0] = 0xFA | 0x80;
                status = TEMP_REQ;
            }
            else if (status == TEMP_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Pressure ...\n");
#endif
		tx_rx[0] = 0xF7 | 0x80;
	        status = END;
            }

	    if (status != END) {
#ifdef DEBUG
		if ((tx_rx[0] & 0x80) == 0x80) // READ
                    fprintf(stderr, "Send : 0x%02X\n", tx_rx[0]);
		else // WRITE
                    fprintf(stderr, "Send : 0x%02X 0x%02X\n", tx_rx[0], tx_rx[1]);
#endif
	        len = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	        if (len < 1) 
                    fprintf(stderr, "Failed to ioctl SPI_IOC_MESSAGE: %s\n", strerror(errno));
            }
        }

        // RECEIVE ....
        if (status != END) {
            if (len > 0) {
                if (status == INIT) {
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X 0x%02X\n", tx_rx[1], tx_rx[2]);
#endif
                    if ((tx_rx[1] & 0x08) == 0x00 && (tx_rx[1] & 0x01) == 0x00)
                        status = READY;
                    else if ((tx_rx[1] == 0xFF) && (tx_rx[1] == 0xFF)) {
                        fprintf(stderr, "Init Failed !\n");
                        status = END;
                    }
                }
                if (status == CALIB_T1_REQ) {
                    dig_T1 = ((tx_rx[2] << 8) & 0xFF00) | (tx_rx[1] & 0x00FF);
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X 0x%02X\n", tx_rx[1], tx_rx[2]);
                    fprintf(stderr, "dig_T1 = %d (0x%04X)\n", dig_T1, dig_T1);
#endif
                    status = CALIB_T1_RESP;
                }
		if (status == CALIB_T2_REQ) {
                    dig_T2 = ((tx_rx[2] << 8) & 0xFF00) | (tx_rx[1] & 0x00FF);
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X 0x%02X\n", tx_rx[1], tx_rx[2]);
                    fprintf(stderr, "dig_T2 = %d (0x%04X)\n", dig_T2, dig_T2);
#endif
                    status = CALIB_T2_RESP;
                }
		if (status == CALIB_T3_REQ) {
                    dig_T3 = ((tx_rx[2] << 8) & 0xFF00) | (tx_rx[1] & 0x00FF);
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X 0x%02X\n", tx_rx[1], tx_rx[2]);
                    fprintf(stderr, "dig_T3 = %d (0x%04X)\n", dig_T3, dig_T3);
#endif
                    status = CALIB_T3_RESP;
                }
                if (status == ID_REQ) {
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X\n", tx_rx[1]);
#endif
		    if (tx_rx[1] != BMP280_CHIP_ID) {
                        printf("Failed to get chip id !\n");
			status = END;
                    }
		    else {
                        printf("BMP280 ID : 0x%02X\n", BMP280_CHIP_ID);
                        status = ID_RESP;
                    }
                }
                if (status == CTRL_REQ) {
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X\n", tx_rx[1]);
#endif
                    status = CTRL_RESP;
                }
                if (status == MEAS_REQ) {
                    status = MEAS_RESP;
                }
                if (status == STATUS_REQ) {
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X 0x%02X\n", tx_rx[1], tx_rx[2]);
#endif
		    if ((tx_rx[1] & 0x08) == 0x00) 
                        status = STATUS_RESP;
		    else 
                        status = MEAS_RESP;
		}
                if (status == TEMP_REQ) {
#ifdef DEBUG
                    fprintf(stderr, "Receive : 0x%02X 0x%02X\n", tx_rx[1], tx_rx[2]);
#endif
		    if (tx_rx[1] == 0x80 && tx_rx[2] == 0x00)
                        status = MEAS_RESP;
		    else {
                        int32_t t_fine, var1, var2;
                        int32_t adc_T = 0;
                        adc_T = ((tx_rx[1] << 16) & 0xFF0000) | ((tx_rx[2] << 8) & 0x00FF00);
#ifdef DEBUG
                        printf("adc_T = 0x%08X (%d)\n", adc_T, adc_T);
#endif
                        adc_T >>= 4;

                        var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) *
			       ((int32_t)dig_T2)) >> 11;

	                var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
                               ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
                               ((int32_t)dig_T3)) >> 14;

                        t_fine = var1 + var2;

                        float T = (t_fine * 5 + 128) >> 8;
                        fprintf(stderr, "TEMPERATURE : %0.2f C\n", T/100);
                        status = TEMP_RESP;
                    }
                }
            }
        }

    } while (status < END);

    if (fd > 0) close(fd);
    return 0;
}
