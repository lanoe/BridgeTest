#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests/"

if [ $# -eq 1 ]; then
  $DIR/testSPI $1
else
  $DIR/testSPI
fi
