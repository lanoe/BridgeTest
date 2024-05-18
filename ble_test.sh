#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests"

interface=hci0
device=$(rfkill list | grep $interface | awk '{ print $3 }' | tr '[:upper:]' '[:lower:]')
rfkill unblock $device

$DIR/testBle $interface $1 $2
