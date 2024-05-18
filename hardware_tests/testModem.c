#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFSIZE 1024
#define TIME_CHECK_REGISTRATION 20

enum {
    INIT,
    ERROR,
    TURN_ON_REQ,
    REMOVE_TRACE_REQ,
    ACTIVATE_CELL_TRACE,
    REMOVE_CELL_TRACE,
    INFO_MANUFACTURER_REQ,
    INFO_MODEL_REQ,
    INFO_REVISION_REQ,
    INFO_IMEI_REQ,
    RSSI_REQ,
    IMSI_REQ,
    OPERATOR_REQ,
    OPERATOR_GET,
    OPERATOR_NUM_REQ,
    OPERATOR_NUM_GET,
    REGISTRATION_REQ,
    REGISTRATION_GET,
    END
};

int main(int argc, char *argv[])
{
    int fd;
    struct termios tty;
    fd_set set;
    struct timeval timeout;
    char *portname = "/dev/dsi-modem";

    if (argc > 1) portname = argv[1];

    fprintf(stderr, "Test MODEM on %s ...\n", portname);

    fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", portname, strerror(errno));
        return -1;
    }

    if (tcgetattr(fd, &tty) < 0) {
        fprintf(stderr, "Failed to tcgetattr: %s\n", strerror(errno));
        if (fd > 0) close(fd);
        return -1;
    }

    cfsetospeed(&tty, (speed_t)B9600);
    cfsetispeed(&tty, (speed_t)B9600);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity */
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
        if (fd > 0) close(fd);
	return -1;
    }

    int status = INIT;
    bool remove_echo_req = false;
    int check_registration = 0;

    do {

       char cmd[BUFSIZE];
       unsigned int len = 0;
       memset(cmd, 0, sizeof(cmd));

	if (status != END) {
            // SEND ....

            if (remove_echo_req) {
#ifdef DEBUG
                fprintf(stderr, "Remove Echo ...\n");
#endif
                snprintf(cmd, BUFSIZE, "ATE0\r");
            }
	    else if (status == INIT) {
#ifdef DEBUG
                fprintf(stderr, "Check Modem ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT\r");
            }
            else if (status == TURN_ON_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Turn On Antenna ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CFUN=1\r");
            }
            else if (status == REMOVE_TRACE_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Remove Traces ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT^CURC=0\r");
            }
            else if (status == INFO_MANUFACTURER_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Manufacturer ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CGMI\r");
            }
            else if (status == INFO_MODEL_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Model ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CGMM\r");
            }
            else if (status == INFO_REVISION_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Revision ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CGMR\r");
            }
            else if (status == INFO_IMEI_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get IMEI ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CGSN\r");
            }
            else if (status == RSSI_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get RSSI ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CSQ\r");
            }
            else if (status == IMSI_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get IMSI ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CIMI\r");
            }
            else if (status == OPERATOR_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Operator Req ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+COPS=3,0\r");
            }
            else if (status == OPERATOR_NUM_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Numeric Operator Req ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+COPS=3,2\r");
            }
            else if (status == OPERATOR_GET || status == OPERATOR_NUM_GET) {
#ifdef DEBUG
                fprintf(stderr, "Get Operator ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+COPS?\r");
            }
            else if (status == REGISTRATION_REQ || status == REGISTRATION_GET) {
#ifdef DEBUG
                fprintf(stderr, "Get Network ...\n");
#endif
		if (status == REGISTRATION_REQ) check_registration++;
                snprintf(cmd, BUFSIZE, "AT+CREG?\r");
            }
	    else if (status == ACTIVATE_CELL_TRACE) {
#ifdef DEBUG
                fprintf(stderr, "Activate Cell Traces ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CREG=2\r");
            }
            else if (status == REMOVE_CELL_TRACE) {
#ifdef DEBUG
                fprintf(stderr, "Remove Cell Traces ...\n");
#endif
                snprintf(cmd, BUFSIZE, "AT+CREG=0\r");
            }

	    len = strlen(cmd);
	    cmd[len] = '\0';

	    if (len > 0 && status != END) {
#ifdef DEBUG
                fprintf(stderr, "Send (%d) : %s\n", len, cmd);
                for (int i = 0; i < len; i++) fprintf(stderr, " %02x", cmd[i]);
                fprintf(stderr, "\n");
#endif
                int wlen = write(fd, cmd, len);
                if (wlen != len) {
                    fprintf(stderr, "Failed to write: %s\n", strerror(errno));
		    status = END;
                }
                tcdrain(fd);    /* delay for output */
                usleep(200000);  // wait 200 ms
            }
        }

        // RECEIVE ....
	if (status != END) {

            char buf[BUFSIZE];
            int rdlen = 0;
            memset(buf, 0, sizeof(buf));
#ifdef DEBUG
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
                for (int i = 0; i < rdlen; i++) fprintf(stderr, " %02x", buf[i]);
                fprintf(stderr, "\n%s", buf);
#endif
                if (strstr(buf, cmd) != NULL && strstr(buf, "OK") != NULL) {
                    remove_echo_req = true;
                }
                else if (remove_echo_req && strstr(buf, "OK") != NULL) {
                    remove_echo_req = false;
                }
                else if (status == INIT && strstr(buf, "OK") != NULL) {
                    status = TURN_ON_REQ;
                }
                else if (status == TURN_ON_REQ && strstr(buf, "OK") != NULL) {
                    status = REMOVE_TRACE_REQ;
                }
                else if (status == REMOVE_TRACE_REQ && strstr(buf, "OK") != NULL) {
                    status = INFO_MANUFACTURER_REQ;
                }
                else if (status == INFO_MANUFACTURER_REQ && strstr(buf, "OK") != NULL) {
                    buf[rdlen-6] = '\0'; // remove '\r\nOK\r\n'
                    fprintf(stderr, "Manufacturer : %s", &buf[2]);
                    status = INFO_MODEL_REQ;
                }
                else if (status == INFO_MODEL_REQ && strstr(buf, "OK") != NULL) {
                    buf[rdlen-6] = '\0'; // remove '\r\nOK\r\n'
                    fprintf(stderr, "Model : %s", &buf[2]);
                    status = INFO_REVISION_REQ;
                }
                else if (status == INFO_REVISION_REQ && strstr(buf, "OK") != NULL) {
                    buf[rdlen-6] = '\0'; // remove '\r\nOK\r\n'
                    fprintf(stderr, "Revision : %s", &buf[2]);
                    status = INFO_IMEI_REQ;
                }
                else if (status == INFO_IMEI_REQ && strstr(buf, "OK") != NULL) {
                    buf[rdlen-6] = '\0'; // remove '\r\nOK\r\n'
                    fprintf(stderr, "IMEI : %s", &buf[2]);
                    status = IMSI_REQ;
                }
                else if (status == IMSI_REQ && strstr(buf, "OK") != NULL) {
                    buf[rdlen-6] = '\0'; // remove '\r\nOK\r\n'
                    fprintf(stderr, "IMSI : %s", &buf[2]);
                    status = ACTIVATE_CELL_TRACE;
                }
                else if (status == ACTIVATE_CELL_TRACE && strstr(buf, "OK") != NULL) {
                    status = OPERATOR_REQ;
                }
                else if (status == OPERATOR_REQ && strstr(buf, "OK") != NULL) {
                    status = OPERATOR_GET;
                }
                else if (status == OPERATOR_GET && strstr(buf, "OK") != NULL) {
                    buf[rdlen-4] = '\0'; // remove '\r\nOK\r\n'
                    char operator[64];
                    int mode, format;
		    if (check_registration == 0) {
                        sscanf(buf, "\r\n+COPS: %d", &mode);
                        fprintf(stderr, "OPERATOR SELECTION MODE : ");
                        switch (mode) {
                            case 0 : fprintf(stderr, "Automatic\n"); break;
                            case 1 : fprintf(stderr, "Manual\n"); break;
                            case 2 : fprintf(stderr, "Deregister from network (Failed)\n"); break;
                            case 3 : fprintf(stderr, "Not applicable (Failed)\n"); break;
                            case 4 : fprintf(stderr, "Manual/Automatic\n"); break;
                            default : fprintf(stderr, "Failed (value=%d)\n", mode); break;
                        }
			status = REGISTRATION_REQ;
                    }
                    else {
                        sscanf(buf, "\r\n+COPS: %d,%d,\"%[^\"]s\",", &mode, &format, operator);
                        fprintf(stderr, "OPERATOR : %s\n", operator);
                        status = OPERATOR_NUM_REQ;
                    }
                }
                else if (status == OPERATOR_NUM_REQ && strstr(buf, "OK") != NULL) {
                    status = OPERATOR_NUM_GET;
                }
                else if (status == OPERATOR_NUM_GET && strstr(buf, "OK") != NULL) {
                    buf[rdlen-4] = '\0'; // remove '\r\nOK\r\n'
                    char operator[64];
                    int mode, format;
                    sscanf(buf, "\r\n+COPS: %d,%d,\"%[^\"]s\",", &mode, &format, operator);
                    fprintf(stderr, "MCC : %c%c%c\n", operator[0], operator[1], operator[2]);
                    fprintf(stderr, "MNC : %s\n", &operator[3]);
                    status = REGISTRATION_GET;
                }
                else if ((status == REGISTRATION_REQ  || status == REGISTRATION_GET) && strstr(buf, "OK") != NULL) {
                    buf[rdlen-4] = '\0'; // remove '\r\nOK\r\n'
                    int n, stat, lac, cellid;
                    if (status == REGISTRATION_REQ) {
                        sscanf(buf, "\r\n+CREG: %d,%d", &n, &stat);
                        fprintf(stderr, "NETWORK : ");
                        switch (stat) {
                            case 0 : fprintf(stderr, "Not registered, Not searching"); break;
                            case 1 : fprintf(stderr, "Registered, home network     "); break;
                            case 2 : fprintf(stderr, "Not registered, Searching ..."); break;
                            case 3 : fprintf(stderr, "Registration denied"); break;
                            case 4 : fprintf(stderr, "Unknown"); break;
                            case 5 : fprintf(stderr, "Registered, roaming          "); break;
                            default : fprintf(stderr, "Failed (value=%d)", stat); break;
                        }
                        status = OPERATOR_REQ;
                    } else { // REGISTRATION_GET
                        sscanf(buf, "\r\n+CREG: %d,%d,\"%X\",\"%X\"", &n, &stat, &lac, &cellid);
		        if (stat == 1 || stat == 5) {
                            fprintf(stderr, "LAC : %d (0x%X)\n", lac, lac);
                            fprintf(stderr, "CELLID : %d (0x%X)", cellid, cellid);
                        }
			status = REMOVE_CELL_TRACE;
                    }

                    if (stat == 2) { // Searching ...
                        if (check_registration < TIME_CHECK_REGISTRATION) {
                            status = REGISTRATION_REQ;
                            usleep(1000000);  // wait 1 s
                            fprintf(stderr, " %d\r", check_registration);
                        }
                        else {
                            fprintf(stderr, "Failed : Not registered !");
                            status = REMOVE_CELL_TRACE;
                        }
                    } else
                        fprintf(stderr, "   \n");
                }
                else if (status == REMOVE_CELL_TRACE && strstr(buf, "OK") != NULL) {
                    status = RSSI_REQ;
                }
                else if (status == RSSI_REQ && strstr(buf, "OK") != NULL) {
                    buf[rdlen-4] = '\0'; // remove 'OK\r\n'
                    int rssi = 0, ber =0;
                    sscanf(buf, "\r\n+CSQ: %d,%d\r\n", &rssi, &ber);
                    fprintf(stderr, "RSSI = %d/ ", rssi);
                    if (rssi >= 0 && rssi <= 31)
                        fprintf(stderr, "%d dBm\n", (rssi*2)-113);
                    else
                        fprintf(stderr, "Failed !\n");
                    fprintf(stderr, "BER = %d/ ", ber);
                    switch (ber) {
                        case 0 : fprintf(stderr, "less than 0.2 percent\n"); break;
                        case 1 : fprintf(stderr, "0.2-0.4 percent\n"); break;
                        case 2 : fprintf(stderr, "0.4-0.8 percent\n"); break;
                        case 3 : fprintf(stderr, "0.8-1.6 percent\n"); break;
                        case 4 : fprintf(stderr, "1.6-3.2 percent\n"); break;
                        case 5 : fprintf(stderr, "3.2-6.4 percent\n"); break;
                        case 6 : fprintf(stderr, "6.4-12.8 percent\n"); break;
                        case 7 : fprintf(stderr, "more than percent\n"); break;
                        case 99 : fprintf(stderr, "Unknown\n"); break;
                        default : fprintf(stderr, "Failed !\n"); break;
                    }
                    status = END;
                }
                else {
                    fprintf(stderr, "Failed to get modem informations !\n");
                    fprintf(stderr, "%s\n", buf);
                    status = END;
                }
            }
        }
    } while (status < END);

    if (fd > 0) close(fd);
    return 0;
}
