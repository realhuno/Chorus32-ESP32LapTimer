#!/bin/bash

if [ "$#" -ne 2 ]; then
	echo "Usage: $0 <dir to search for firmware files> <output dir>"
	exit 1
fi

FILES_DIR=$1
OUTPUT_DIR=$2

FIRMWARE_PATHS=$(find $FILES_DIR -type f -name "firmware.bin")

for path in $FIRMWARE_PATHS
do
BOARD_NAME=$(echo $path | grep -Eo "BOARD_[A-Za-z_0-1]+")
	echo $BOARD_NAME
	cp $path $OUTPUT_DIR/$BOARD_NAME.bin
done
