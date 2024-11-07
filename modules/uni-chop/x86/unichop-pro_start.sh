#!/bin/sh
# startup script for UNICHOP
#
#set -x

###########################################
# setting for production
# gateway : dev/wbm0, tr0
export TRGW=dev/wbm0         # EB device 
export SDGW=tr0              # saftlib device
export MILDEV=1              # MIL device, piggy(0), sio slot 1 (1) ...
###########################################
# setting for development
# gateway: N/A
###########################################

echo -e UNICHOP start script for UNILAC Chopper

###########################################
# clean up stuff
###########################################
echo -e UNICHOP: bring possibly resident firmware to idle state
unichop-ctl $TRGW stopop
sleep 2

unichop-ctl $TRGW idle
sleep 2

echo -e UNICHOP: destroy all unowned conditions for lm32 channel of ECA
saft-ecpu-ctl $SDGW -x

echo -e UNICHOP: disable all events from I/O inputs to ECA
saft-io-ctl $SDGW -w

echo -e UNICHOP: destroy all unowned conditions and delete all macros for wishbone channel of ECA
saft-wbm-ctl $SDGW -x

###########################################
# load firmware to lm32 and configure fw
###########################################
echo -e UNICHOP: load firmware 
eb-fwload $TRGW u 0x0 unichop.bin

echo -e UNICHOP: configure firmware for gateway $NGW
sleep 2
unichop-ctl $TRGW -w$MILDEV configure
sleep 2
unichop-ctl $TRGW startop

###########################################
# configure ECA
###########################################

# lm32 listens to timing messages
# UNICHOP_ECADO_STRAHLWEG_WRITE, requires a negative offset that is 2x the value of UNICHOP_MILMODULE_ACCESST
saft-ecpu-ctl $SDGW -c 0x1ff0fa0000000000 0xfffffff000000000 200000 0xfa0 -g -d
# UNICHOP_ECADO_STRAHLWEG_READ
saft-ecpu-ctl $SDGW -c 0x1ff0fa1000000000 0xfffffff000000000 0 0xfa1 -d
# UNICHOP_ECADO_MIL_SWRITE
saft-ecpu-ctl $SDGW -c 0x1ff0fb0000000000 0xfffffff000000000 0 0xfb0 -d
# UNICHOP_ECADO_MIL_SREAD
saft-ecpu-ctl $SDGW -c 0x1ff0fb1000000000 0xfffffff000000000 0 0xfb1 -d
# UNICHOP_ECADO_IQSTOP (QR, QL)
#saft-ecpu-ctl $SDGW -c 0x11c000a000000000 0xfffffff000000000 0 0xf0a -d
#saft-ecpu-ctl $SDGW -c 0x11c100a000000000 0xfffffff000000000 0 0xf0a -d
#UNICHOP_ECADO_HSISTOP
saft-ecpu-ctl $SDGW -c 0x11c4008000000000 0xfffffff000000000 0 0xfc3 -d
#UNICHOP ECADO_HLISTOP
saft-ecpu-ctl $SDGW -c 0x11c3008000000000 0xfffffff000000000 0 0xfc2 -d
#UNICHOP_ECADO_HSICMD
saft-ecpu-ctl $SDGW -c 0x11c40ff000000000 0xfffffff000000000 0 0xfc5 -d
#UNICHOP ECADO_HLICMD
saft-ecpu-ctl $SDGW -c 0x11c30ff000000000 0xfffffff000000000 0 0xfc4 -d


###########################################
# init RPGs (Rahmenpulsgeneratoren)
###########################################
sleep 1
saft-dm tr0 -p unichop-pro-saftdm.txt

###########################################
# reset diagnostics
###########################################
sleep 2
unichop-ctl $TRGW cleardiag
