.PHONY: clean

BINARY = testAmber testMaster testRS485 testI2C testSPI testRTC testTO136 testModem testBle testWifi

CC = gcc
CFLAGS = -Wall

all: $(BINARY)

debug: CFLAGS += -DDEBUG
debug: $(BINARY)

testTO136: CFLAGS += -I /home/debian/dsi-storage/libto/include/
testTO136: LDFLAGS = -lTO

testRS485: CFLAGS += -DEM111

testBle: CFLAGS += -I /usr/include/dbus-1.0 -I /usr/lib/arm-linux-gnueabihf/dbus-1.0/include -ldbus-1 -Wextra

clean:
	rm -f $(BINARY) *.o
