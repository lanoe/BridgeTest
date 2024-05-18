echo "Remove all USB devices before launching this test ..."

echo "Power on usb ..."
if [ ! -L /sys/class/gpio/gpio86 ] && [ ! -L /sys/class/gpio/gpio90 ]; then
  echo 86 > /sys/class/gpio/export
  echo 90 > /sys/class/gpio/export
  echo out > /sys/class/gpio/gpio86/direction
  echo out > /sys/class/gpio/gpio90/direction
fi
echo 0 > /sys/class/gpio/gpio86/value
echo 0 > /sys/class/gpio/gpio90/value
echo 1 > /sys/class/gpio/gpio86/value
echo 1 > /sys/class/gpio/gpio90/value
sleep 6

lsusb > /tmp/lsusb.txt

nb_usb1_dev=-1
nb_usb2_dev=-1

while read -r line ; do
  if [ "$line" != "Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub" ] ; then
    echo "> $line"
  fi
  nb_usb1_dev=$(expr $nb_usb1_dev + 1)
done < <(grep "Bus 001" /tmp/lsusb.txt)

while read -r line ; do
  if [ "$line" != "Bus 002 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub" ] ; then
    echo "> $line"
  fi
  nb_usb2_dev=$(expr $nb_usb2_dev + 1)
done < <(grep "Bus 002" /tmp/lsusb.txt)

if [ "$nb_usb1_dev" != "0" ] && [ "$nb_usb1_dev" != "-1" ]; then
  echo "> $nb_usb1_dev device detected on Bus 1 > Test Failed !"
fi
if [ "$nb_usb2_dev" != "0" ] && [ "$nb_usb2_dev" != "-1" ]; then
  echo "> $nb_usb2_dev device detected on Bus 2 > Test Failed !"
fi

rm -f /tmp/lsusb.txt

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
