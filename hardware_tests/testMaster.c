#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define START 0x68
#define STOP 0x16

#define DISP1_DATA20 95

enum {
    INIT, 
    FIRMWARE_REQ, 
    FIRMWARE_RESP,
    ISM_MASTER_CONFIG_REQ,
    ISM_MASTER_CONFIG_RESP,
    SET_MASTER_CONFIG_REQ,
    SET_MASTER_CONFIG_RESP,
    SET_MASTER_OP_MODE_REQ,
    SET_MASTER_OP_MODE_RESP,
    SET_AES_KEY_REQ,
    SET_AES_KEY_RESP,
    SET_RADIO_CONFIG_REQ,
    SET_RADIO_CONFIG_RESP,
    SET_REMOVE_OBJECT_REQ,
    SET_REMOVE_OBJECT_RESP,
    START_REQ,
    START_RESP,
    GET_RADIO_CONFIG_REQ,
    GET_RADIO_CONFIG_RESP,
    GET_RADIO_INFO_REQ,
    GET_RADIO_INFO_RESP,
    END
};

static void gpioExport(int gpio);
static void gpioDirection(int gpio, int direction); // 1 for output, 0 for input
static void gpioSet(int gpio, int value);
static void gpioUnexport(int gpio);

int main(int argc, char *argv[])
{
    int ret = -99;
    int fd;
    struct termios tty;
    fd_set set;
    struct timeval timeout;
    char *portname = "/dev/ttymxc2";

    if (argc > 1) portname = argv[1];

    fprintf(stderr, "Test Master on %s ...\n", portname);

    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", portname, strerror(errno));
        ret = -1;
	goto err;
    }

    if (tcgetattr(fd, &tty) < 0) {
        fprintf(stderr, "Failed to tcgetattr: %s\n", strerror(errno));
        ret = -2;
	goto err;
    }

    cfsetospeed(&tty, (speed_t)B115200);
    cfsetispeed(&tty, (speed_t)B115200);

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
        ret = -3;
	goto err;
    }

    unsigned int status = INIT;

    if (argc > 1)
        fprintf(stderr, "Please, reset the Master with PushButton ...\n");
    else {
        fprintf(stderr, "Reset the Master, please wait ...\n");
        gpioExport(DISP1_DATA20);
        gpioDirection(DISP1_DATA20, 1);
        gpioSet(DISP1_DATA20, 0);
        sleep(1);
        gpioSet(DISP1_DATA20, 1);
        gpioUnexport(DISP1_DATA20);
    }

    unsigned char command_number = 0;

    do {
        unsigned char buf[255];
        int rlen=0;
#ifdef DEBUG
	int i=0;
	fprintf(stderr, "status = %d\n", status);
#endif

        FD_ZERO(&set); /* clear the set */
        FD_SET(fd, &set); /* add our file descriptor to the set */

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

        int rv = select(fd + 1, &set, NULL, NULL, &timeout);
        if (rv == -1) {
            fprintf(stderr, "Failed to select: %s\n", strerror(errno));
            ret = -3;
	    goto err;
        }
        else if (rv == 0) {
            fprintf(stderr, "Failed to read: TIMEOUT !\n");
            ret = -4;
	    goto err;
        }

        rlen = read(fd, buf, sizeof(buf) - 1);

        if (rlen > 0) {
#ifdef DEBUG
            fprintf(stderr, "Read (%d) :", rlen);
            for (i=0; i < rlen; i++) fprintf(stderr, " %02x", buf[i]);
            fprintf(stderr, "\n");
#endif
	    if (buf[0] != START || buf[rlen-1] != STOP) {
#ifdef DEBUG
                fprintf(stderr, "Failed : Message Structure Error !\n");
#endif
            }
            else if (rlen != 3+buf[1]) {
                fprintf(stderr, "Failed : Lenght Error !\n");
                status = END;
            }
            else if (buf[3] != command_number) {
                fprintf(stderr, "Failed : Command number Error ! %d\n", buf[0]);
                status = END;
            }
            else if (status == INIT && buf[2] == 0xCA) { // NotifyReset(202)
                fprintf(stderr, "NOTIFY RESET received\n");
                status = FIRMWARE_REQ;
	        command_number++;
            }
            else if (status == FIRMWARE_RESP && buf[2] == 0x33) { // GetFirmwareVerResp(51) 
                unsigned int prot = (unsigned int)(buf[7] << 8) | (unsigned int)(buf[8]);
                fprintf(stderr, "MASTER FIRMWARE VERSION : v%d.%d.%d - protocol v%d\n", buf[4], buf[5], buf[6], prot);
                status = START_REQ;
                command_number++;
            }
            else if (status == ISM_MASTER_CONFIG_RESP && buf[2] == 0xE5) { // IsmMasterConfigResp(229)
#ifdef DEBUG
                fprintf(stderr, "Ism master configuration info ...\n");
#endif
                status = SET_MASTER_CONFIG_REQ;
                command_number++;
            }
            else if (status == SET_MASTER_CONFIG_RESP && buf[2] == 0x0B) { // SetMasterConfigResp(11)
#ifdef DEBUG
                fprintf(stderr, "Set master configuration\n");
#endif
		if (buf[4] != 0x00) fprintf(stderr, "Failed to set Master Configuration - status = 0x%02X\n", buf[4]);
                status = SET_MASTER_OP_MODE_REQ;
                command_number++;
            }
            else if (status == SET_MASTER_OP_MODE_RESP && buf[2] == 0x11) { // SetMasterOperationModeResp(17)
#ifdef DEBUG
                fprintf(stderr, "Set master opration mode\n");
#endif
		if (buf[4] != 0x00) fprintf(stderr, "Failed to set Master Operation Mode - status = 0x%02X\n", buf[4]);
                status = SET_AES_KEY_REQ;
                command_number++;
            }
            else if (status == SET_AES_KEY_RESP && buf[2] == 0xE3) { // SetAesKeyResp(227)
#ifdef DEBUG
                fprintf(stderr, "Set AES key\n");
#endif
		if (buf[4] != 0x00) fprintf(stderr, "Failed to set AES key - status = 0x%02X\n", buf[4]);
                status = SET_RADIO_CONFIG_REQ;
                command_number++;
            }
            else if (status == SET_RADIO_CONFIG_RESP && buf[2] == 0x07) { // SetRadioConfigResp(7)
#ifdef DEBUG
                fprintf(stderr, "SET RADIO CONFIG\n");
#endif
		if (buf[4] != 0x00) fprintf(stderr, "Failed to set Radio Config - status = 0x%02X\n", buf[4]);
                status = START_REQ;
                command_number++;
            }
            else if (status == START_RESP && buf[2] == 0x13) { // StartResp(19)
#ifdef DEBUG
                fprintf(stderr, "START\n");
#endif
		if (buf[4] != 0x00) fprintf(stderr, "Failed to Start - status = 0x%02X\n", buf[4]);
                status = GET_RADIO_INFO_REQ;
                command_number++;
            }
            else if (status == GET_RADIO_CONFIG_RESP && buf[2] == 0x0D) { // GetRadioConfigResp(13)
#ifdef DEBUG
                fprintf(stderr, "RADIO CONFIG\n");
#endif
                status = END;
                command_number++;
            }
            else if (status == GET_RADIO_INFO_RESP && buf[2] == 0x0F) { // GetRadioInfoResp(15)
#ifdef DEBUG
                fprintf(stderr, "RADIO INFO\n");
#endif
		if (buf[4] != 0x00) fprintf(stderr, "Failed to get Radio Info - status 0x%02X\n", buf[4]);
                unsigned int build = (unsigned int)(buf[17] << 8) | (unsigned int)(buf[16]);
                fprintf(stderr, "RADIO FIRMWARE VERSION : v%d.%d - build %d\n", buf[14], buf[15], build);
                unsigned long int device_id = ((unsigned long int)buf[13] << 31) | ((unsigned long int)buf[12] << 16) \
                                            | ((unsigned long int)buf[11] << 8) | ((unsigned long int)buf[10]);
                fprintf(stderr, "RADIO DEVICE IDENTIFIER : %ld\n", device_id);
                status = END;
                command_number++;
            }
#ifdef DEBUG
            else {
                fprintf(stderr, "Failed : Invalid Message Error !\n");
                status = END;
            }
#endif
        } 

        if (status < END ) {
            unsigned char cmd[255];
            unsigned int len = 0;
            
            cmd[len++] = START; // START_BYTE
            cmd[len++] = 0x00;  // PAYLOAD_LENGTH

            if (status == FIRMWARE_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Master Firmware Version ...\n");
#endif
                cmd[len++] = 0x32;  // GetFirmwareVerReq(50)
                cmd[len++] = command_number;
                status = FIRMWARE_RESP;
            }
            else if (status == START_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Start Request ...\n");
#endif
                cmd[len++] = 0x12; // StartReq(18)
                cmd[len++] = command_number;
                status = START_RESP;
            }
            else if (status == GET_RADIO_INFO_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Radio Info Request ...\n");
#endif
                cmd[len++] = 0x0E; // GetRadioInfoReq(14)
                cmd[len++] = command_number;
                status = GET_RADIO_INFO_RESP;
            }
            else if (status == GET_RADIO_CONFIG_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Radio Config Request ...\n");
#endif
                cmd[len++] = 0x0C; // GetRadioConfigReq(12)
                cmd[len++] = command_number;
                status = GET_RADIO_CONFIG_RESP;
            }
            else if (status == ISM_MASTER_CONFIG_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Ism Master Configuration ...\n");
#endif
                cmd[len++] = 0xE4; // IsmMasterConfigReq(228)
                cmd[len++] = command_number;
                status = ISM_MASTER_CONFIG_RESP;
            }
            else if (status == SET_MASTER_CONFIG_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Set Master Configuration ...\n");
#endif
    		cmd[len++] = 0x0A; // SetMasterConfigReq(10)
                cmd[len++] = command_number;
                cmd[len++] = 0xFE;  // master_group_addr = 0xFE
                cmd[len++] = 0xAD;  // master_device_addr = 0xDEAD
                cmd[len++] = 0xDE;
                status = SET_MASTER_CONFIG_RESP;
            }
            else if (status == SET_MASTER_OP_MODE_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Set Master Operation Mode ...\n");
#endif
    		cmd[len++] = 0x10; // SetMasterOperationModeReq(16)
                cmd[len++] = command_number;
                cmd[len++] = 0x05;
                cmd[len++] = 0xE0;
                cmd[len++] = 0x33;
                cmd[len++] = 0x66;
                cmd[len++] = 0x4E;
                status = SET_MASTER_OP_MODE_RESP;
            }
            else if (status == SET_AES_KEY_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Set AES key ...\n");
#endif
    		cmd[len++] = 0xE2; // SetAesKeyReq(226)
                cmd[len++] = command_number;
                cmd[len++] = 0x01;
		for (unsigned char c=0; c<0x10; c++) cmd[len++] = c;
                status = SET_AES_KEY_RESP;
            }
            else if (status == SET_RADIO_CONFIG_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Set Radio Config ...\n");
#endif
    		cmd[len++] = 0x06; // SetRadioConfigReq(6)
                cmd[len++] = command_number;
                cmd[len++] = 0x00;
                cmd[len++] = 0xFE;
                cmd[len++] = 0x10;
                cmd[len++] = 0xAD;
                cmd[len++] = 0xDE;
                cmd[len++] = 0x34;
                cmd[len++] = 0x12;
                cmd[len++] = 0x00;
                cmd[len++] = 0x00;
                cmd[len++] = 0x80;
                cmd[len++] = 0xD8;
                cmd[len++] = 0x00;
                cmd[len++] = 0x09;
                cmd[len++] = 0x02;
                cmd[len++] = 0x05;
                cmd[len++] = 0x00;
                cmd[len++] = 0x01;
                cmd[len++] = 0xB8;
                cmd[len++] = 0x0B;
                cmd[len++] = 0x07;
                cmd[len++] = 0x07;
                cmd[len++] = 0x00;
                cmd[len++] = 0x00;
                cmd[len++] = 0xCE;
                cmd[len++] = 0xFF;
                status = SET_RADIO_CONFIG_RESP;
            }

            cmd[1] = (unsigned char) (len-2);  // PAYLOAD_LENGTH
            cmd[len++] = STOP;                 // STOP_BYTE

	    if (len > 3 && status != END) {
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
        
    } while (status < END);

    ret = 0;
err:
    if (fd > 0) close(fd);
    return ret;
}

static void gpioExport(int gpio)
{
    int fd;
    char buf[255];
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
    char buf[64];
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    sprintf(buf, "%d", gpio); 
    write(fd, buf, strlen(buf));
    close(fd);
}
