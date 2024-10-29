#!/bin/bash

BASE_ADDR=0x4000

for n in {0..4095}
do
	ADDR=$(printf %X $((BASE_ADDR+(n*4))))
	#echo $ADDR
	RES=$(eb-read dev/ttyUSB0 0x$ADDR/4)
	if ! [ $RES = "00000000" ]
	then
		#echo "obase=2; ibase=16; $ADDR" | bc
    echo $ADDR
    echo $RES
	fi	
done

#eb-find dev/ttyUSB0 0x0000000000000651 0x434e5448
