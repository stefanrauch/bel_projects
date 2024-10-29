#!/bin/bash
BASE_ADDR=16384

for n in {0..2047}
do
	RAM1_ADDR=$(($BASE_ADDR+n*4))
  RAM2_ADDR=$((RAM1_ADDR+8192))
	#echo $ADDR
	DATRAM1=$(eb-read dev/ttyUSB0 0x$(printf %X $RAM1_ADDR)/4)
  DATRAM2=$(eb-read dev/ttyUSB0 0x$(printf %X $RAM2_ADDR)/4)
  if ! [ $DATRAM1 = "00000000" ]
  then
    printf "%X\n" $RAM1_ADDR
    printf "%X\n" $RAM2_ADDR
    printf "ERROR: ADDRESS 0x%d of RAM1 and ADDRESS 0x%d of RAM2 don't hold the same data! RAM1: %s, RAM2: %s\n" $RAM1_ADDR $RAM2_ADDR $DATRAM1 $DATRAM2
  fi
done