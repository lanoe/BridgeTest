#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests/"

if [ -f /tmp/master_interface ]; then
  interface=$(cat /tmp/master_interface)
fi

if [ "$interface" != "" ]; then
  $DIR/testMaster $interface
else
  $DIR/testMaster
fi
