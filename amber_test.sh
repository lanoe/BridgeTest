#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests/"

if [ -f /tmp/amber_interface ]; then
  interface=$(cat /tmp/amber_interface)
fi

if [ "$interface" != "" ]; then
  $DIR/testAmber $interface
else
  $DIR/testAmber
fi
