
delta=5
max_diff=1

if [ "$1" != "-v" ]; then
  if [ "$1" != "" ]; then
    delta=$1
  fi
  if [ "$2" != "" ]; then
    max_diff=$2
  fi
fi

echo "Check date/time coherence ..."
echo "------------------------------"
echo "Max Diff = $max_diff s"
echo "Delta = $delta s"
echo "--------------------------------"

echo "Please wait $delta s ..."
timedate_1=$(timedatectl 2>&1)
if [ "$timedate_1" = "Failed to query server: Invalid argument" ]; then
  echo "Update RTC time with the SYSTEM time ..."
  hwclock --systohc
  sleep 1
  timedate_1=$(timedatectl)
fi
sleep $delta
timedate_2=$(timedatectl)

echo "$timedate_2"
echo "--------------------------------"

ntp=$(echo "$timedate_2" | grep "NTP synchronized: yes")
if [ "$ntp" = "" ] ; then
  echo "Test Failed : date/time not yet synchronized with NTP server"
  echo "WAIT FEW SECONDS AND LAUNCH AGAIN THE SAME TEST PLEASE !"
  exit 1
fi

system_1=$(echo "$timedate_1" | grep "Universal time:" | sed 's/Universal time://g' | awk '{ print $2" "$3 }')
system1=$(date --date="$system_1" +%s)

rtc_1=$(echo "$timedate_1" | grep "RTC time:"  | sed 's/RTC time://g' | awk '{ print $2" "$3 }')
rtc1=`date --date="$rtc_1" +%s`

let "tDiff=$system1-$rtc1"
if [ "$tDiff" -lt 0 ]; then
  let "tDiff=$rtc1-$system1"
fi
echo "Diff measurement = $tDiff s"

rtc_2=$(echo "$timedate_2" | grep "RTC time:"  | sed 's/RTC time://g' | awk '{ print $2" "$3 }')
rtc2=$(date --date="$rtc_2" +%s)

let "tDelta=$rtc2-$rtc1"
if [ "$tDelta" -lt 0 ]; then
  let "tDelta=$rtc1-$rtc2"
fi
echo "Delta measurement = $tDelta s"
echo "--------------------------------"

let "delta=$delta+1"
if [ "$tDelta" -gt "$delta" ]; then
  echo "Test Failed : the time is not well incremented by the RTC (Diff=$tDiff(s)) !"
  exit 1
fi

let "max_diff=$max_diff+1"
if [ "$tDiff" -gt "$max_diff" ]; then
  echo "Test Failed : the RTC time is not the same as the SYSTEM time ! (Delta=$tDelta(s))"
  echo "Update RTC time with the SYSTEM time ..."
  hwclock --systohc
  echo "LAUNCH AGAIN THE SAME TEST PLEASE !"
  exit 1
fi

echo "Test OK"
