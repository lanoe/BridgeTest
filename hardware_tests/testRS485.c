#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

enum {
    INIT,
    ADDRESS_REQ,
    ADDRESS_RESP,
    FREQUENCY_REQ,
    FREQUENCY_RESP,
    VOLTAGE_REQ,
    VOLTAGE_RESP,
    END
};

static unsigned int crc16(unsigned char *buf, int len);

int main(int argc, char *argv[])
{
    int fd;
    struct termios tty;
    fd_set set;
    struct timeval timeout;
    char *portname = "/dev/ttymxc1";

    if (argc > 1) portname = argv[1];

#ifdef EM111
    fprintf(stderr, "With EM111 (Carlo Gavazzi)\n");
#else
    fprintf(stderr, "With CONTAX 1M 100A (Ref 031277)\n");
#endif
    fprintf(stderr, "Test RS485 on %s ...\n", portname);

    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", portname, strerror(errno));
        return -1;
    }

    if (tcgetattr(fd, &tty) < 0) {
        fprintf(stderr, "Failed to tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)B9600);
    cfsetispeed(&tty, (speed_t)B9600);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
#ifdef EM111
    tty.c_cflag &= ~PARENB;     /* no parity */
#else
    tty.c_cflag |= PARENB;      /* even parity */
#endif
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "Failed to tcsetattr: %s\n", strerror(errno));
	return -1;
    }

    int status = INIT;
#ifdef EM111
    unsigned char address = 0x01;
#else
    unsigned char address = 0x00;
#endif

    do {

	if (status != END) {
            // SEND ....
            unsigned char cmd[255];
            unsigned int len = 0; 
#ifdef DEBUG
            unsigned int i = 0;
#endif
            cmd[len++] = address; /* Slave Address */
            cmd[len++] = 0x03;    /* Read Register */

            if (status == INIT) {
#ifdef DEBUG
                fprintf(stderr, "Get Address ...\n");
#endif
#ifdef EM111
                cmd[len++] = 0x20; /* Address : 0x2000 */
                cmd[len++] = 0x00;
#else
                cmd[len++] = 0x01; /* Address : 0x110 */
                cmd[len++] = 0x10;
#endif
                cmd[len++] = 0x00; /* Number of registers = 1 */
                cmd[len++] = 0x01;
                status = ADDRESS_REQ;
            }
            else if (status == ADDRESS_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Frequency ...\n");
#endif
#ifdef EM111
                cmd[len++] = 0x01; /* Frequency : 0x110 */
                cmd[len++] = 0x10;
#else
                cmd[len++] = 0x01; /* Frequency : 0x130 */
                cmd[len++] = 0x30;
#endif
                cmd[len++] = 0x00; /* Number of registers = 1 */
                cmd[len++] = 0x01;
	        status = FREQUENCY_REQ;
            }
	    else if (status == FREQUENCY_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Voltage ...\n");
#endif
#ifdef EM111
                cmd[len++] = 0x01; /* Voltage : 0x102 */
                cmd[len++] = 0x02;
#else
                cmd[len++] = 0x01; /* Voltage : 0x131 */
                cmd[len++] = 0x31;
#endif
		cmd[len++] = 0x00; /* Number of registers = 1 */
                cmd[len++] = 0x01;
	        status = VOLTAGE_REQ;
	    }
            else if (status == VOLTAGE_RESP) {
                status = END;
	    }
	
	    if (len > 0 && status != END) {

                unsigned int crc = crc16(cmd, len);
                cmd[len++] = (unsigned char) (crc & 0x00FF);
                cmd[len++] = (unsigned char) ((crc >> 8) & 0x00FF);
#ifdef DEBUG
                fprintf(stderr, "Send (%d) :", len);
                for (i = 0; i < len; i++) fprintf(stderr, " %02x", cmd[i]);
                fprintf(stderr, "\n");
#endif
                int wlen = write(fd, cmd, len);
                if (wlen != len) {
                    fprintf(stderr, "Failed to write: %d, %d\n", wlen, errno);
		    status = END;
                }
                tcdrain(fd);    /* delay for output */
            }
        }

        // RECEIVE ....
	if (status != END) {
            unsigned char buf[255];
            int rdlen = 0;
#ifdef DEBUG
            int i = 0;
	    fprintf(stderr, "status = %d\n", status);
#endif

            FD_ZERO(&set); /* clear the set */
            FD_SET(fd, &set); /* add our file descriptor to the set */

            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            int rv = select(fd + 1, &set, NULL, NULL, &timeout);
            if (rv == -1) {
                fprintf(stderr, "Failed to select: %s\n", strerror(errno));
	        status = END;
            }
            else if (rv == 0) {
                fprintf(stderr, "Failed to read: TIMEOUT !\n");
	        status = END;
            }
            else
	        rdlen = read(fd, buf, sizeof(buf) - 1);

            if (rdlen > 0) {
#ifdef DEBUG
                fprintf(stderr, "Receive (%d) :", rdlen);
                for(i = 0; i < rdlen; i++) fprintf(stderr, " %02x", buf[i]);
                fprintf(stderr, "\n");
#endif
		if (rdlen > 2) {
                    unsigned int crc = crc16(buf, rdlen-2);
                    if (buf[rdlen-2] == (unsigned char) (crc & 0x00FF) && \
		        buf[rdlen-1] == (unsigned char) ((crc >> 8) & 0x00FF)) 
		    {
	                if (buf[1] == 0x03 && buf[2] == 2 && rdlen == buf[2]+5)  { 
			    if (status == ADDRESS_REQ) { // Address
                                address = buf[4];
                                fprintf(stderr, "ADDRESS : 0x%02x (%d)\n", address, address);
                                status = ADDRESS_RESP;
                            }
			    if (status == FREQUENCY_REQ) { // Frequency
                                unsigned int freq = (unsigned int)(buf[3] << 8) | (unsigned int)(buf[4]);
#ifdef EM111
                                fprintf(stderr, "FREQUENCY : %0.01f Hz\n", freq/10.0);
#else
                                fprintf(stderr, "FREQUENCY : %0.01f Hz\n", freq/100.0);
#endif
                                status = FREQUENCY_RESP;
                            }
			    if (status == VOLTAGE_REQ) { // Voltage
                                unsigned int volt = (unsigned int)(buf[3] << 8) | (unsigned int)(buf[4]);
#ifdef EM111
                                fprintf(stderr, "VOLTAGE : %0.01f V\n", volt/10.0);
#else
                                fprintf(stderr, "VOLTAGE : %0.01f V\n", volt/100.0);
#endif
                                status = VOLTAGE_RESP;
                            }
                        }
                    }
	            else {
                        fprintf(stderr, "Failed CRC !\n");
			status = END;
		    }
                }
            }
        }
    } while (status < END);

    if (fd > 0) close(fd);
    return 0;
}

static unsigned int crc16(unsigned char *buf, int len)
{  
    unsigned int crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++)
    {
        crc ^= (unsigned int)buf[pos];    // XOR byte into least sig. byte of crc
        for (int i = 8; i != 0; i--) {    // Loop over each bit
            if ((crc & 0x0001) != 0) {    // If the LSB is set
                crc >>= 1;                // Shift right and XOR 0xA001
                crc ^= 0xA001;
            }
            else                          // Else LSB is not set
                crc >>= 1;                // Just shift right
        }
    }
    return crc;
}
