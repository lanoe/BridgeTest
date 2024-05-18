#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

enum {
    INIT,
    TEMPERATURE_REQ,
    TEMPERATURE_RESP,
    HUMIDITY_REQ,
    HUMIDITY_RESP,
    END
};

#define SHT21_I2C_ADDR 0x40
#define DATA_BUFFER_SIZE 3

static unsigned char crc8(unsigned char *buf, int len);

int main(int argc, char *argv[])
{
    int fd;
    char *portname = "/dev/i2c-2";

    if (argc > 1) portname = argv[1];

    fprintf(stderr, "With SHT21 on PCB008 v3\n");
    fprintf(stderr, "Test I2C on %s ...\n", portname);

    fd = open(portname, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", portname, strerror(errno));
        return -1;
    }
    
    if (ioctl(fd, I2C_SLAVE, SHT21_I2C_ADDR) < 0) {
        fprintf(stderr, "Failed to ioctl I2C_SLAVE (0x%02x) : %s\n", SHT21_I2C_ADDR, strerror(errno));
        return -1;
    }

    int status = INIT;

    do {

	if (status != END) {
            // SEND ....
            unsigned char cmd[255];
            unsigned int len = 0;

            if (status == INIT) {
#ifdef DEBUG
                fprintf(stderr, "Get Temperature ...\n");
#endif
		cmd[len++] = 0xF3;
                status = TEMPERATURE_REQ;
            }
            else if (status == TEMPERATURE_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Humidity ...\n");
#endif
		cmd[len++] = 0xF5;
	        status = HUMIDITY_REQ;
            }
            else if (status == HUMIDITY_RESP) {
                status = END;
	    }
	
	    if (len > 0) {
                int wlen = write(fd, cmd, len);
                if (wlen != len) {
                    fprintf(stderr, "Failed to write: %d, %s\n", wlen, strerror(errno));
		    status = END;
                }
                tcdrain(fd);    /* delay for output */
            }
        }

        // RECEIVE ....
	if (status != END) {
            unsigned char buf[DATA_BUFFER_SIZE];
            int rdlen = 0;
#ifdef DEBUG
            int i = 0;
#endif

            rdlen = read(fd, buf, sizeof(buf));

            if (rdlen > 0) {
#ifdef DEBUG
                fprintf(stderr, "Receive (%d) :", rdlen);
                for(i = 0; i < rdlen; i++) fprintf(stderr, " %02x", buf[i]);
                fprintf(stderr, "\n");
#endif
                unsigned char crc = crc8(buf, DATA_BUFFER_SIZE-1);
                if (crc == buf[2]) 
                {
                    if (status == TEMPERATURE_REQ) {
                        float temperature = (((buf[0] * 256 + buf[1]) * 175.72) / 65536.0) - 46.85;
                        fprintf(stderr, "TEMPERATURE : %.2f C\n", temperature);
                        status = TEMPERATURE_RESP;
                    }
                    if (status == HUMIDITY_REQ) {
                        float humidity = (((buf[0] * 256 + buf[1]) * 125.0) / 65536.0) - 6;
                        fprintf(stderr, "HUMIDITY : %.2f %%\n", humidity);
                        status = HUMIDITY_RESP;
                    }
                }
		else {
                    fprintf(stderr, "Failed CRC !");
                    status = END;
                }
            }
        }
    } while (status < END);

    if (fd > 0) close(fd);
    return 0;
}

static unsigned char crc8(unsigned char *buf, int len)
{
    // 8 bits checksum with P(x)=x^8+x^5+x^4+1 => 100110001
    unsigned char crc = 0x00;
    for (int pos = 0; pos < len; pos++)
    {
        crc ^= buf[pos];
        for (int bit = 8; bit > 0; --bit)
        {
            if (crc & 0x80) crc = (crc << 1) ^ 0x131;
            else crc = (crc << 1);
        }
    }
    return crc;
}
