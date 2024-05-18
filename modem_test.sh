#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests/"

power_on_usb () {
  echo "Power on usb ..."
  if [ ! -L /sys/class/gpio/gpio86 ] && [ ! -L /sys/class/gpio/gpio90 ]; then
    echo 86 > /sys/class/gpio/export
    echo 90 > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio86/direction
    echo out > /sys/class/gpio/gpio90/direction
  fi
  echo 0 > /sys/class/gpio/gpio86/value
  echo 0 > /sys/class/gpio/gpio90/value
  sleep 1
  echo 1 > /sys/class/gpio/gpio86/value
  echo 1 > /sys/class/gpio/gpio90/value
  sleep 2
}

power_off_usb () {
  echo "Power off usb ..."
  if [ ! -L /sys/class/gpio/gpio86 ] && [ ! -L /sys/class/gpio/gpio90 ]; then
    echo 86 > /sys/class/gpio/export
    echo 90 > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio86/direction
    echo out > /sys/class/gpio/gpio90/direction
  fi
  echo 1 > /sys/class/gpio/gpio86/value
  echo 1 > /sys/class/gpio/gpio90/value
  echo 0 > /sys/class/gpio/gpio86/value
  echo 0 > /sys/class/gpio/gpio90/value
  sleep 1
}

TIME=20

MODEM="ID 12d1:1506 Huawei Technologies Co., Ltd. Modem/Networkcard"

power_on_usb

x=1
while [ $x -le $TIME ]
do
  echo -ne "Check Modem : $x times over $TIME ...\\r"
  lsusb > /tmp/lsusb.txt
  if grep -qs "$MODEM" /tmp/lsusb.txt; then
    x=$(( $TIME + 1))
  else
    x=$(( $x + 1 ))
    sleep 1
  fi
done

sleep 2

if [ "$#" -lt 1 ]; then
  $DIR/testModem $1
else
  $DIR/testModem
fi

power_off_usb
