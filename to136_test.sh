#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/hardware_tests/"

if [ "$#" -lt 1 ]; then
	$DIR/testTO136 $1
else
  $DIR/testTO136
fi
