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
    REG_CTRL1_REQ,
    REG_CTRL2_REQ,
    REG_CTRL3_REQ,
    SEC_REQ,
    MIN_REQ,
    HOUR_REQ,
    DAY_REQ,
    WEEKDAY_REQ,
    MONTH_REQ,
    YEAR_REQ,
    REG_CTRL1_SET,
    REG_CTRL3_SET,
    END
};

#define PCF8523_I2C_ADDR 0x68
#define DATA_BUFFER_SIZE 3

int main(int argc, char *argv[])
{
    int fd;
    char *portname = "/dev/i2c-0";

    if (argc > 1) portname = argv[1];

    fprintf(stderr, "Test RTC (PCF8523) on %s ...\n", portname);

    fd = open(portname, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", portname, strerror(errno));
        return -1;
    }
    
    if (ioctl(fd, I2C_SLAVE_FORCE, PCF8523_I2C_ADDR) < 0) {
        fprintf(stderr, "Failed to ioctl I2C_SLAVE (0x%02x) : %s\n", PCF8523_I2C_ADDR, strerror(errno));
        return -1;
    }

    unsigned char HOUR=0, AM_PM=0, CAPA=0, BATTERY=0;
    unsigned char sec=0, min=0, hour=0, day=0, weekday=0, month=0, year=0;
    unsigned char buf_value=0;

    int status = REG_CTRL1_REQ;

    do {

	if (status != END) {
            // SEND ....
            unsigned char cmd[2];
            unsigned int len = 0;

            if (status == REG_CTRL1_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Hour Format (12_24) and Capacity Selection (CAP_SEL) ...\n");
#endif
		cmd[0] = REG_CTRL1_REQ;
		len++;
            }
            else if (status == REG_CTRL1_SET) {
#ifdef DEBUG
                fprintf(stderr, "Set Capacity to 12pF ...\n");
#endif
		cmd[0] = REG_CTRL1_REQ;
		len++;
		cmd[1] = buf_value | 0x80;
		len++;
            }
            else if (status == REG_CTRL3_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Battery Status ...\n");
#endif
		cmd[0] = REG_CTRL3_REQ;
		len++;
            }
            else if (status == REG_CTRL3_SET) {
#ifdef DEBUG
                fprintf(stderr, "Activate Battery Status ...\n");
#endif
		cmd[0] = REG_CTRL3_REQ;
		len++;
		cmd[1] = buf_value & 0x1F;
		len++;
            }
            else if (status == SEC_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Seconds ...\n");
#endif
		cmd[0] = SEC_REQ;
		len++;
            }
            else if (status == MIN_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Minutes ...\n");
#endif
		cmd[0] = MIN_REQ;
		len++;
            }
            else if (status == HOUR_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Hours ...\n");
#endif
		cmd[0] = HOUR_REQ;
		len++;
            }
            else if (status == DAY_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Days ...\n");
#endif
		cmd[0] = DAY_REQ;
		len++;
            }
            else if (status == WEEKDAY_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get weekday ...\n");
#endif
		cmd[0] = WEEKDAY_REQ;
		len++;
            }
            else if (status == MONTH_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Months ...\n");
#endif
		cmd[0] = MONTH_REQ;
		len++;
            }
            else if (status == YEAR_REQ) {
#ifdef DEBUG
                fprintf(stderr, "Get Years ...\n");
#endif
		cmd[0] = YEAR_REQ;
		len++;
            }

           if (len > 0) {
#ifdef DEBUG
                fprintf(stderr, "Send (%d) :", len);
		for(int i = 0; i < len; i++) fprintf(stderr, " %02x", cmd[i]);
	        fprintf(stderr, "\n");
#endif
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
            unsigned char buf = 0x00;
            int rdlen = 0;
            rdlen = read(fd, &buf, 1);

            if (rdlen > 0) {
#ifdef DEBUG
                fprintf(stderr, "Receive (%d) : 0x%02X\n", rdlen, buf);
#endif
                if (status == REG_CTRL1_REQ) {
                    if ((buf & 0x08) == 0x00) HOUR=1;
                    if ((buf & 0x80) != 0x00) CAPA=1;
		    if (CAPA == 0) {
			status = REG_CTRL1_SET;
		        buf_value = buf;
	            }
                    else status = REG_CTRL3_REQ;
                }
		else if (status == REG_CTRL1_SET) {
                    status = REG_CTRL3_REQ;
		}
                else if (status == REG_CTRL3_REQ) {
                    if ((buf & 0x04) == 0x00) BATTERY=1;
                    if ((buf & 0xE0) != 0x00) BATTERY=0;
		    if (BATTERY == 0 && CAPA == 0) {
                        status = REG_CTRL3_SET;
		        buf_value = buf;
		    }
                    else status = SEC_REQ;
                }
		else if (status == REG_CTRL3_SET) {
                    status = SEC_REQ;
		}
                else if (status == SEC_REQ) {
                    sec = buf & 0x7F;
                    status = MIN_REQ;
                }
                else if (status == MIN_REQ) {
                    min = buf & 0x7F;
                    status = HOUR_REQ;
                }
                else if (status == HOUR_REQ) {
                    if (HOUR == 0) {
                        if ((buf & 0x20) != 0x00) AM_PM=1; 
                        hour = buf & 0x1F;
                        if (hour < 1 || hour > 0x12) fprintf(stderr, "Failed to read HOUR(1-12) : %d\n", hour);
		    }
                    else {
                        hour = buf & 0x3F;
                        if (hour > 0x24) fprintf(stderr, "Failed to read HOUR(0-24) : %d\n", hour);
                    }
                    status = DAY_REQ;
                }
                else if (status == DAY_REQ) {
                    day = buf & 0x3F;
                    if (day < 1 || day > 0x31) fprintf(stderr, "Failed to read DAY(1-31) : %d\n", day);
                    status = WEEKDAY_REQ;
                }
                else if (status == WEEKDAY_REQ) {
                    weekday = buf & 0x07;
                    if (weekday < 1 || weekday > 6) fprintf(stderr, "Failed to read WEEKDAY(0-6) : %d\n", weekday);
                    status = MONTH_REQ;
                }
                else if (status == MONTH_REQ) {
                    month = buf & 0x1F;
                    if (month < 1 || month > 0x12) fprintf(stderr, "Failed to read MONTH(1-12) : %d\n", month);
                    status = YEAR_REQ;
                }
                else if (status == YEAR_REQ) {
                    year = buf; 
                    if (year > 0x99) fprintf(stderr, "Failed to read YEAR(0-99) : %d\n", year);
                    status = END;
                }

                if (status == END) {
                    unsigned char week[7][4] = {"Dim\0","Lun\0","Mar\0","Mer\0","Jeu\0","Ven\0","Sam\0"};
                    fprintf(stderr, "Time : %02X:%02X:%02X %s\n", hour, min, sec, HOUR == 0 ? ( AM_PM == 0 ? "(AM)" : "(PM)") : "" );
                    fprintf(stderr, "Date : %s %02X/%02X/20%02X\n", weekday<1 || weekday>6 ? "Failed" : (char*) week[weekday], day, month, year);
                    fprintf(stderr, "Battery : %s\n", BATTERY == 0 ? "Failed" : "OK");
                    fprintf(stderr, "Capa : %s\n", CAPA == 0 ? "7pF - Failed" : "12.5pF");
		    if (BATTERY == 0 && CAPA == 0) fprintf(stderr, "PLEASE RESTART THE TEST !\n");
		    if (BATTERY == 0 && CAPA == 1) fprintf(stderr, "Set the battery 'CR2032' AND RESTART THE TEST !\n");
                }
            }
        }

    } while (status < END);

    if (fd > 0) close(fd);
    return 0;
}
