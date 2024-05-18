#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)"

rm -rf /var/lib/connman/wifi_*
rfkill unblock wifi

ssid="$1"
passphrase="$2"
hidden="$3"

if [ "$ssid" != "" ] && [ "$passphrase" != "" ]; then
  result=$(connmanctl enable wifi 2>&1)
  if [ "$result" = "Enabled wifi" ] || [ "$result" = "Error wifi: Already enabled" ] ; then
    echo "wifi enabled"
  else
    echo $result
    echo "Failed to enable wifi !"
    exit 1
  fi

  TIME=5
  x=1
  while [ $x -le $TIME ]
  do
    echo -ne "Scan Wifi : $x times over $TIME ...\\r"
    result=$(connmanctl scan wifi 2>&1)
    sleep 2
    if [ "$hidden" = "true" ]; then
      service=$(connmanctl services | cut -c 5- | sed 's/ wifi_/:wifi_/g' | grep "_hidden_managed_psk" | awk -F: '{print $2}')
    else
      service=$(connmanctl services | cut -c 5- | sed 's/ wifi_/:wifi_/g' | grep "^$ssid" | awk -F: '{print $2}')
    fi
    if [ "$service" != "" ]; then
        x=$(( $TIME + 1))
    else
        x=$(( $x + 1 ))
    fi
    done

  if [ "$service" = "" ] ; then
    echo "Failed to find the wifi AP '$ssid'"
    exit 1
  else
    if [ "$hidden" != "true" ]; then
      echo "Wifi AP '$ssid' found"
    fi
  fi

  echo "Connect to $service ..."

  sync
  sleep 1
  $DIR/connect_wifi $service "$ssid" "$passphrase" $hidden 

  sleep 1
  echo "Check ip ..."
  ip=$(ifconfig wlan0 | grep broadcast | awk '{ print $2 }')
  if [ "$ip" = "" ]; then
    echo "Failed to get ip address !"
  else
    echo "ip=$ip"	  
  fi

  echo "Disconnect ..."
  service=$(connmanctl services | cut -c 5- | sed 's/ wifi_/:wifi_/g' | grep "^$ssid" | awk -F: '{print $2}')
  if [ "$service" != "" ]; then
    result=$(connmanctl disconnect $service 2>&1)
    checkResultError=$(echo $result | grep Error)
    if [ "$checkResultError" != "" ]; then
      echo $result
      echo "Failed to disconnect !"
    fi
  else
    echo "Failed to connect !"
  fi

  sync
  sleep 1
  result=$(connmanctl disable wifi 2>&1)
  if [ "$result" = "Disabled wifi" ] || [ "$result" = "Error wifi: Already disabled" ] ; then
    echo "wifi disabled"
  else
    echo $result
    echo "Failed to disable wifi !"
    exit 1
  fi

else
  echo "Failed : need 3 paramters (ssid, password and/or hidden flag) !"
fi
