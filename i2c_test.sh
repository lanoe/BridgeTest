#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests/"

if [ $# -eq 1 ]; then
  $DIR/testI2C $1
else
  $DIR/testI2C
fi
