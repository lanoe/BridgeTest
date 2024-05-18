#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests/"

if [ $# -eq 1 ]; then
  $DIR/testRS485 $1
else
  $DIR/testRS485
fi
