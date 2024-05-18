#include <stdio.h>
#include <fcntl.h> 
#include <errno.h>
#include <sys/select.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>


#define DISP1_DATA12 53 
#define DISP1_DATA23 91 

#define MAX_BUFFER_LENGTH 1024
#define MAX_PACKET_LENGTH 256
#define MAX_REQ 3
#define MAX_FRAME 20

enum {
    INIT,
    FIRMWARE_REQ,
    FIRMWARE_RESP,
    SERIAL_NUMBER_REQ,
    SERIAL_NUMBER_RESP,
    GET_ALL_PARAMS_REQ,
    GET_ALL_PARAMS_RESP,
    SET_PARAM_REQ,
    SET_PARAM_RESP,
    GET_PARAM_REQ,
    GET_PARAM_RESP,
    SOFT_RESET_REQ,
    SOFT_RESET_RESP,
    END
};

enum {
    UART_CTL0,
    UART_CTL1,
    UART_BR0,
    UART_BR1,
    UART_MCTL,
    UART_CMD_OUT_ENABLE,
    UART_DIDELAY,
    APP_MAXPACKETLENGTH = 10,
    APP_AES_ENABLE,
    APP_WOR_PERIODH = 28,
    APP_WOR_PERIODL,
    APP_WOR_MULTIPLIERH,
    APP_WOR_MULTIPLIERL,
    APP_WOR_RX_TIME,
    MBUS_CODING = 39,
    MBUS_PREAMBLELENGTHH,
    MBUS_PREAMBLELENGTHL,
    MBUS_SYNCH,
    MBUS_RXTimeout,
    MBUS_FrameFormat,
    MBUS_Bl_ADD_Disable = 48,
    MBUS_Bl_Control,
    MBUS_Bl_ManID1,
    MBUS_Bl_ManID2,
    MBUS_Bl_IDNr1,
    MBUS_Bl_IDNr2,
    MBUS_Bl_IDNr3,
    MBUS_Bl_IDNr4,
    MBUS_Bl_Version,
    MBUS_Bl_DevType,
    RF_Channel = 60,
    RF_Power,
    RF_DataRate,
    RF_AutoSleep,
    RSSI_Enable = 69,
    Mode_Preselect,
    Net_Mode,
    Config_CRC_Disable,
    CFG_Flags = 80
};

static void gpioExport(int gpio);
static void gpioDirection(int gpio, int direction); // 1 for output, 0 for input
static void gpioSet(int gpio, int value);
static void gpioUnexport(int gpio);

int main(int argc, char *argv[])
{
    int fd;
    struct termios tty;
    fd_set set;
    struct timeval timeout;
    char *portname = "/dev/ttymxc0";
    char address = 0, value = 0;

    if (argc > 1) portname = argv[1];
    if (argc == 1 || argc == 2) fprintf(stderr, "Test Amber on %s ...\n", portname);
    else if (argc == 4) {
        address = (char) atoi(argv[2]);
        value = (char) atoi(argv[3]);
        fprintf(stderr, "Amber : Write value %d to address %d on %s ...\n", value, address, portname);
    }
    else {
        fprintf(stderr, "Failed : wrong parameters !\n");
        return -1;
    }

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
    tty.c_cflag &= ~PARENB;     /* no parity bit */
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

    unsigned int status = INIT;
    unsigned int wait = 0;
    
    // Reset to read Amber informations
    if (strstr(portname, "/dev/ttymxc") != NULL && argc != 4) {
        //fprintf(stderr, "Hardware factory reset of the Amber (to be checked !) ...\n");
        //gpioExport(DISP1_DATA12);
        //gpioDirection(DISP1_DATA12, 1);
        //gpioSet(DISP1_DATA12, 1);
        //sleep(2);
        //gpioSet(DISP1_DATA12, 0);
        //gpioUnexport(DISP1_DATA12);
        //sleep(2);
        fprintf(stderr, "Hardware reset the Amber, please wait ...\n");
        gpioExport(DISP1_DATA23);
        gpioDirection(DISP1_DATA23, 1);
        gpioSet(DISP1_DATA23, 0);
        sleep(2);
        gpioSet(DISP1_DATA23, 1);
        gpioUnexport(DISP1_DATA23);
        sleep(2);
    }

    unsigned int req = 0, rdlen = 0, i = 0i, nb_frame = 0;;
    unsigned char buf[MAX_BUFFER_LENGTH];
    memset(buf, 0, sizeof(buf));
    unsigned char cmd[MAX_PACKET_LENGTH];
    memset(cmd, 0, sizeof(cmd));

    do {
#ifdef DEBUG
	fprintf(stderr, "status = %d\n", status);
#endif
	////////////////////////////////////////////////
	// SEND
	////////////////////////////////////////////////
        if (status >= INIT && wait == 0) {
	    int len = 0;
	    if (status == INIT && argc == 4) {
#ifdef DEBUG
                fprintf(stderr, "Get Parameter value ...\n");
#endif
                cmd[len++] = 0xFF; // START_BYTE
                cmd[len++] = 0x0A; // CMD_GET_REQ
                cmd[len++] = 0x02; // payload size
                cmd[len++] = address; // address
                cmd[len++] = 0x01; // just get one byte
                status = GET_PARAM_REQ;
            }
	    else if (status == INIT) {
#ifdef DEBUG
                fprintf(stderr, "Get Firmware Version ...\n");
#endif
                cmd[len++] = 0xFF; // START_BYTE
                cmd[len++] = 0x0C; // CMD_FWV_REQ
                cmd[len++] = 0x00; // null size
                status = FIRMWARE_REQ;
            }
            else if (status == GET_PARAM_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Set Parameter value ...\n");
#endif
                cmd[len++] = 0xFF; // START_BYTE
                cmd[len++] = 0x09; // CMD_SET_REQ
                cmd[len++] = 0x03; // payload size
                cmd[len++] = address; // address
                cmd[len++] = 0x01; // just set one byte
                cmd[len++] = value; // value to be set
                status = SET_PARAM_REQ;
            }
	    else if (status == SET_PARAM_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Software Reset ...\n");
#endif
                cmd[len++] = 0xFF; // START_BYTE
                cmd[len++] = 0x05; // CMD_RESET_REQ
                cmd[len++] = 0x00; // null size
                status = SOFT_RESET_REQ;
            }
            else if (status == FIRMWARE_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get Serial Number ...\n");
#endif
                cmd[len++] = 0xFF; // START_BYTE
                cmd[len++] = 0x0B; // CMD_SERIALNO_REQ
                cmd[len++] = 0x00; // null size 
                status = SERIAL_NUMBER_REQ;
            }
            else if (status == SERIAL_NUMBER_RESP) {
#ifdef DEBUG
                fprintf(stderr, "Get All Parameters ...\n");
#endif
                cmd[len++] = 0xFF; // START_BYTE
                cmd[len++] = 0x0A; // CMD_GET_REQ
                cmd[len++] = 0x02; // size
                cmd[len++] = 0x00; // start address
                cmd[len++] = 0x80; // number of bytes
                status = GET_ALL_PARAMS_REQ;
            }
            else if (status == GET_ALL_PARAMS_RESP || status == SOFT_RESET_RESP) {
                status = END;
            }

            if (len > 0) {
                unsigned char crc = 0;
                for(i = 0; i < len; i++) crc ^= cmd[i];
                cmd[len++] = crc;
#ifdef DEBUG
                fprintf(stderr, "Send (%d) :", len);
                for (i = 0; i < len; i++) fprintf(stderr, " %02x", cmd[i]);
                fprintf(stderr, "\n");
#endif
                int wlen = write(fd, cmd, len);
                if (wlen != len) {
                    fprintf(stderr, "Failed to write: %d, %d\n", wlen, errno);
                }
                tcdrain(fd);    /* delay for output */
            }
        }

        ////////////////////////////////////////////////
        // RECEIVED
        ////////////////////////////////////////////////
        if (status != END)
        {
            unsigned int check_status = 0;

            FD_ZERO(&set); /* clear the set */
            FD_SET(fd, &set); /* add our file descriptor to the set */

            timeout.tv_sec = 8;
            timeout.tv_usec = 0;

            int rv = select(fd + 1, &set, NULL, NULL, &timeout);
            if (rv == -1) {
                fprintf(stderr, "Failed to select: %s\n", strerror(errno));
                status = END;
            }
            else if (rv == 0) {
                fprintf(stderr, "Timeout ... \n");
                check_status = 1;
		rdlen = 0;
		wait = 0;
            }
            else {
                rdlen += read(fd, &buf[rdlen], MAX_PACKET_LENGTH-rdlen);
            }

            int start_byte_position = -1;
            int packet_len = 0;

            if (rdlen > 0) {

                // check command frame presence
                // frame : <0xFF> <command(1)> <length(1)> <payload(length)> <crc(1)>
                for(i = 0; i < rdlen-1; i++) {
                    if(buf[i] == 0xFF && buf[i+1] == (cmd[1] | 0x80)) {
                        start_byte_position = i;
                        packet_len = buf[i+2]+4;
                        break;
                    }
                }
#ifdef DEBUG
                fprintf(stderr, "Received (%d) : ", rdlen);
                for(i = 0; i < rdlen; i++) fprintf(stderr, " %02x", buf[i]);
                fprintf(stderr, "\n");
                fprintf(stderr, "start_byte_position = %d\n", start_byte_position);
                fprintf(stderr, "packet_len = %d\n", packet_len);
#endif
                if (start_byte_position != -1 && rdlen <= MAX_PACKET_LENGTH && buf[start_byte_position] == 0xFF && rdlen >= buf[start_byte_position+2]+4) {
                    // Check CRC
                    unsigned char crc = 0;
                    for(i = 0; i < packet_len-1; i++) crc ^= buf[start_byte_position+i];

                    if (crc == buf[start_byte_position+packet_len-1]) {
                        // Valid frame received ...
                        if (buf[start_byte_position+1] == 0x8C && buf[start_byte_position+2] == 3 && status == FIRMWARE_REQ) { // CMD_FWV_REQ
                            fprintf(stderr, "FIRMWARE VERSION : v%d.%d.%d\n", buf[start_byte_position+3], buf[start_byte_position+4], buf[start_byte_position+5]);
                            status = FIRMWARE_RESP;
			    nb_frame = 0;
                            req = 0;
                        }
                        else if (buf[start_byte_position+1] == 0x8B && buf[start_byte_position+2] == 4 && status == SERIAL_NUMBER_REQ) { // CMD_SERIALNO_REQ
                            unsigned int sn = (unsigned int)(buf[start_byte_position+4] << 16) | (unsigned int)(buf[start_byte_position+5] << 8) | (unsigned int)(buf[start_byte_position+6]);
                            fprintf(stderr, "SERIAL NUMBER : 0%02d%06d \n", buf[start_byte_position+3], sn);
                            status = SERIAL_NUMBER_RESP;
			    nb_frame = 0;
                            req = 0;
                        }
                        else if (buf[start_byte_position+1] == 0x89 && status == SET_PARAM_REQ) { // CMD_SET_REQ
                            if (buf[start_byte_position+2] != 0x01) {
                                fprintf(stderr, "Failed : invalid frame ! size = %d\n", buf[start_byte_position+2]);
                               status = END;
                            }
                            else if (buf[start_byte_position+3] != 0x00) {
                                fprintf(stderr, "Failed : invalid response, status=%d\n", buf[start_byte_position+3]);
                                status = END;
                            }
			    else
                                status = SET_PARAM_RESP;
			    nb_frame = 0;
                            req = 0;
                        }
                        else if (buf[start_byte_position+1] == 0x8A && status == GET_PARAM_REQ) { // CMD_GET_REQ
                            if (buf[start_byte_position+2] != 0x03 || buf[start_byte_position+3] != (char)address || buf[start_byte_position+4] != 0x01) {
                                fprintf(stderr, "Failed : invalid frame !\n");
                                status = END;
                            }
                            else if (buf[start_byte_position+5] != value)
                                status = GET_PARAM_RESP;
                            else {
                                fprintf(stderr, "Amber : Value %d already set to address %d\n", value, address);
                                status = END;
                            }
			    nb_frame = 0;
                            req = 0;
                        }
                        else if (buf[start_byte_position+1] == 0x8A && status == GET_ALL_PARAMS_REQ) { // CMD_GET_REQ
                            if (buf[start_byte_position+3] != 0x00) fprintf(stderr, "Failed : wrong start address %d\n", buf[start_byte_position+3]);
                            if (buf[start_byte_position+2] != buf[start_byte_position+4]+2) fprintf(stderr, "Failed : wrong size %d\n", buf[start_byte_position+4]);
                            fprintf(stderr, "--------- PARAMETERS ---------\n");
                            fprintf(stderr, "%02d UART_CTL0           : %d\n", UART_CTL0, buf[start_byte_position+5+UART_CTL0]);
                            fprintf(stderr, "%02d UART_CTL1           : %d\n", UART_CTL1, buf[start_byte_position+5+UART_CTL1]);
                            fprintf(stderr, "%02d UART_BR0            : %d\n", UART_BR0, buf[start_byte_position+5+UART_BR0]);
                            fprintf(stderr, "%02d UART_BR1            : %d\n", UART_BR1, buf[start_byte_position+5+UART_BR1]);
                            fprintf(stderr, "%02d UART_MCTL           : %d\n", UART_MCTL, buf[start_byte_position+5+UART_MCTL]);
                            fprintf(stderr, "%02d UART_CMD_Out_enable : %d\n", UART_CMD_OUT_ENABLE, buf[start_byte_position+5+UART_CMD_OUT_ENABLE]);
                            unsigned int delay = (unsigned int)(buf[start_byte_position+5+UART_DIDELAY] << 8) | (unsigned int)(buf[start_byte_position+5+UART_DIDELAY+1]);
                            fprintf(stderr, "%02d UART_DIDelay        : %d\n", UART_DIDELAY, delay);
                            fprintf(stderr, "-----------------------------\n");
                            fprintf(stderr, "%02d APP_MAXPacketLength : %d\n", APP_MAXPACKETLENGTH, buf[start_byte_position+5+APP_MAXPACKETLENGTH]);
                            fprintf(stderr, "%02d APP_AES_Enable      : %d\n", APP_AES_ENABLE, buf[start_byte_position+5+APP_AES_ENABLE]);
                            fprintf(stderr, "%02d APP_WOR_PeriodH     : %d\n", APP_WOR_PERIODH, buf[start_byte_position+5+APP_WOR_PERIODH]);
                            fprintf(stderr, "%02d APP_WOR_PeriodL     : %d\n", APP_WOR_PERIODL, buf[start_byte_position+5+APP_WOR_PERIODL]);
                            fprintf(stderr, "%02d APP_WOR_MultiplierH : %d\n", APP_WOR_MULTIPLIERH, buf[start_byte_position+5+APP_WOR_MULTIPLIERH]);
                            fprintf(stderr, "%02d APP_WOR_MultiplierL : %d\n", APP_WOR_MULTIPLIERL, buf[start_byte_position+5+APP_WOR_MULTIPLIERL]);
                            fprintf(stderr, "%02d APP_WOR_RX_Time     : %d\n", APP_WOR_RX_TIME, buf[start_byte_position+5+APP_WOR_RX_TIME]);
                            fprintf(stderr, "-----------------------------\n");
                            fprintf(stderr, "%02d MBUS_Coding          : %d\n", MBUS_CODING, buf[start_byte_position+5+MBUS_CODING]);
                            fprintf(stderr, "%02d MBUS_PreambleLengthH : %d\n", MBUS_PREAMBLELENGTHH, buf[start_byte_position+5+MBUS_PREAMBLELENGTHH]);
                            fprintf(stderr, "%02d MBUS_PreambleLengthL : %d\n", MBUS_PREAMBLELENGTHL, buf[start_byte_position+5+MBUS_PREAMBLELENGTHL]);
                            fprintf(stderr, "%02d MBUS_Synch           : %d\n", MBUS_SYNCH, buf[start_byte_position+5+MBUS_SYNCH]);
                            fprintf(stderr, "%02d MBUS_RXTimeout       : %d\n", MBUS_RXTimeout, buf[start_byte_position+5+MBUS_RXTimeout]);
                            fprintf(stderr, "%02d MBUS_FrameFormat     : %d\n", MBUS_FrameFormat, buf[start_byte_position+5+MBUS_FrameFormat]);
                            fprintf(stderr, "%02d MBUS_Bl_ADD_Disable  : %d\n", MBUS_Bl_ADD_Disable, buf[start_byte_position+5+MBUS_Bl_ADD_Disable]);
                            fprintf(stderr, "%02d MBUS_Bl_Control      : %d\n", MBUS_Bl_Control, buf[start_byte_position+5+MBUS_Bl_Control]);
                            fprintf(stderr, "%02d MBUS_Bl_ManID1       : %d\n", MBUS_Bl_ManID1, buf[start_byte_position+5+MBUS_Bl_ManID1]);
                            fprintf(stderr, "%02d MBUS_Bl_ManID2       : %d\n", MBUS_Bl_ManID2, buf[start_byte_position+5+MBUS_Bl_ManID2]);
                            fprintf(stderr, "%02d MBUS_Bl_IDNr1        : %d\n", MBUS_Bl_IDNr1, buf[start_byte_position+5+MBUS_Bl_IDNr1]);
                            fprintf(stderr, "%02d MBUS_Bl_IDNr2        : %d\n", MBUS_Bl_IDNr2, buf[start_byte_position+5+MBUS_Bl_IDNr2]);
                            fprintf(stderr, "%02d MBUS_Bl_IDNr3        : %d\n", MBUS_Bl_IDNr3, buf[start_byte_position+5+MBUS_Bl_IDNr3]);
                            fprintf(stderr, "%02d MBUS_Bl_IDNr4        : %d\n", MBUS_Bl_IDNr4, buf[start_byte_position+5+MBUS_Bl_IDNr4]);
                            fprintf(stderr, "%02d MBUS_Bl_Version      : %d\n", MBUS_Bl_Version, buf[start_byte_position+5+MBUS_Bl_Version]);
                            fprintf(stderr, "%02d MBUS_Bl_DevType      : %d\n", MBUS_Bl_DevType, buf[start_byte_position+5+MBUS_Bl_DevType]);
                            fprintf(stderr, "-----------------------------\n");
                            fprintf(stderr, "%02d RF_Channel         : %d\n", RF_Channel, buf[start_byte_position+5+RF_Channel]);
                            fprintf(stderr, "%02d RF_Power           : %d\n", RF_Power, buf[start_byte_position+5+RF_Power]);
                            fprintf(stderr, "%02d RF_DataRate        : %d\n", RF_DataRate, buf[start_byte_position+5+RF_DataRate]);
                            fprintf(stderr, "%02d RF_AutoSleep       : %d\n", RF_AutoSleep, buf[start_byte_position+5+RF_AutoSleep]);
                            fprintf(stderr, "%02d RSSI_Enable        : %d\n", RSSI_Enable, buf[start_byte_position+5+RSSI_Enable]);
                            fprintf(stderr, "%02d Mode_Preselect     : %d\n", Mode_Preselect, buf[start_byte_position+5+Mode_Preselect]);
                            fprintf(stderr, "%02d Net_Mode           : %d\n", Net_Mode, buf[start_byte_position+5+Net_Mode]);
                            fprintf(stderr, "%02d Config_CRC_Disable : %d\n", Config_CRC_Disable, buf[start_byte_position+5+Config_CRC_Disable]);
                            fprintf(stderr, "%02d CFG_Flags          : %d\n", CFG_Flags, buf[start_byte_position+5+CFG_Flags]);
                            fprintf(stderr, "-----------------------------\n");
                            status = GET_ALL_PARAMS_RESP;
			    nb_frame = 0;
                            req = 0;
                        }
                        else if (buf[start_byte_position+1] == 0x85 && status == SOFT_RESET_REQ) { // CMD_SET_REQ
                            if (buf[start_byte_position+2] != 0x01)
                                fprintf(stderr, "Failed : invalid frame ! size = %d\n", buf[start_byte_position+2]);
                            else if (buf[start_byte_position+3] != 0x00)
                                fprintf(stderr, "Failed : invalid response, status=%d\n", buf[start_byte_position+3]);
                            else
                                fprintf(stderr, "Amber : Modification done !\n");
                            status = END;
			    nb_frame = 0;
                            req = 0;
			}
                        else {
                            check_status = 1;
#ifdef DEBUG
                            fprintf(stderr, "Unknown frame ...\n");
#endif
                        }
			rdlen = 0;
			wait = 0;
		    }
                    else {
			rdlen = 0;
			wait = 1;
#ifdef DEBUG
                        fprintf(stderr, "CRC Error : crc = 0x%02x and buf[%d] = 0x%02x !\n", crc, packet_len-1, buf[start_byte_position+packet_len-1]);
#endif
                    }
                }
                else {
                    if (start_byte_position == -1) {
			nb_frame++;
#ifdef DEBUG
                        fprintf(stderr, "No start byte in frame %d...\n", nb_frame);
#endif
			if (nb_frame > MAX_FRAME) {
                            check_status = 1;
			    nb_frame = 0;
                            wait = 0;
                        } else
                            wait = 1;
			rdlen = 0;

                    } else if (rdlen > MAX_PACKET_LENGTH) {
#ifdef DEBUG
                        fprintf(stderr, "Too long frame ...\n");
#endif
			rdlen = 0;
                        wait = 0;

                    } else {
#ifdef DEBUG
                        fprintf(stderr, "Incomplete frame ...\n");
#endif
                        wait = 1;
                    }
                }
            } // if rdlen > 0

            if (check_status) {
                if (req < MAX_REQ && status == FIRMWARE_REQ) {
                    fprintf(stderr, "FIRMWARE_REQ n=%d\n", req++);
                    status = INIT;
                }
                else if (req < MAX_REQ && status == SERIAL_NUMBER_REQ) {
                    fprintf(stderr, "SERIAL_NUMBER_REQ n=%d\n", req++);
                    status = FIRMWARE_RESP;
                }
                else if (req < MAX_REQ && status == GET_ALL_PARAMS_REQ) {
                    fprintf(stderr, "GET_ALL_PARAMS_REQ n=%d\n", req++);
                    status = SERIAL_NUMBER_RESP;
                }
                else if (req < MAX_REQ && status == GET_PARAM_REQ) {
                    fprintf(stderr, "GET_PARAM_REQ n=%d\n", req++);
                    status = INIT;
                }
                else if (req < MAX_REQ && status == SET_PARAM_REQ) {
                    fprintf(stderr, "SET_PARAM_REQ n=%d\n", req++);
                    status = GET_PARAM_RESP;
                }
                else if (req < MAX_REQ && status == SOFT_RESET_REQ) {
                    fprintf(stderr, "SOFT_RESET_REQ n=%d\n", req++);
                    status = SET_PARAM_RESP;
                }
                else if (status != INIT) {
                    fprintf(stderr, "Failed : invalid status %d ... !\n", status);
                    status = END;
                }
                if (req >= MAX_REQ) {
                    fprintf(stderr, "Failed to communicate with Amber ... !\n");
                    status = END;
                }
                check_status = 0;
            }
        }

    } while (status < END);

    if (fd > 0) close(fd);
    return 0;
}

static void gpioExport(int gpio)
{
    int fd;
    char buf[32];
    fd = open("/sys/class/gpio/export", O_WRONLY);
    sprintf(buf, "%d", gpio); 
    write(fd, buf, strlen(buf));
    close(fd);
}

static void gpioDirection(int gpio, int direction) // 1 for output, 0 for input
{
    int fd;
    char buf[64];
    sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);
    if (direction) write(fd, "out", 3);
    else write(fd, "in", 2);
    close(fd);
}

static void gpioSet(int gpio, int value)
{
    int fd;
    char buf[64];
    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
    fd = open(buf, O_WRONLY);
    sprintf(buf, "%d", value);
    write(fd, buf, 1);
    close(fd);
}

static void gpioUnexport(int gpio)
{
    int fd;
    char buf[32];
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    sprintf(buf, "%d", gpio); 
    write(fd, buf, strlen(buf));
    close(fd);
}

