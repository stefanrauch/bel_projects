#!/bin/sh
# startup script for WRMIL
#
#set -x

###########################################
# setting for production
# gateway : dev/wbm0, tr0
export TRGW=dev/wbm0         # EB device 
export SDGW=tr0              # saftlib device
export NGW=0                 # of GW; pzu_qr, pzu_ql ...
export SIDGW=1c0             # SID of puz_qr, pzu_ql ...
export MILDEV=1              # MIL device, piggy(0), sio slot 1 (1) ...
export MILADDR=0x420800      # address of MIL device to which MIL telegram is written
# piggy: Wishbone address of 'GSI_MIL_SCU'
# SIO  : Wishbone address of 'SCU-BUS-Master' + (slot number) * 0x20000
# finally the register 0x800 needs to be added
# example SIO slot #1: 0x420800
###########################################
# setting for development
# gateway: N/A
###########################################

echo -e WRMIL start script for PZU_QR

###########################################
# clean up stuff
###########################################
echo -e WRMIL: bring possibly resident firmware to idle state
wrmil-ctl $TRGW stopop
sleep 2

wrmil-ctl $TRGW idle
sleep 2

echo -e WRMIL: destroy all unowned conditions for lm32 channel of ECA
saft-ecpu-ctl $SDGW -x

echo -e WRMIL: disable all events from I/O inputs to ECA
saft-io-ctl $SDGW -w

echo -e WRMIL: destroy all unowned conditions and delete all macros for wishbone channel of ECA
saft-wbm-ctl $SDGW -x

###########################################
# load firmware to lm32 and configure fw
###########################################
echo -e WRMIL: load firmware 
eb-fwload $TRGW u 0x0 wrmil.bin

echo -e WRMIL: configure firmware for gateway $NGW
sleep 2
wrmil-ctl $TRGW -s$NGW -w1 -m1 -l1375 configure
sleep 2
wrmil-ctl $TRGW startop

###########################################
# configure ECA
###########################################
echo -e WRMIL: configure $SDGW for timestamping, needed for monitoring
#  configured as TLU input (Lemo cable from MIL piggy)
saft-io-ctl $SDGW -n B1 -o 0 
saft-io-ctl $SDGW -n B1 -b 0x1fe1a01000000000

# lm32 listens to TLU
saft-ecpu-ctl $SDGW -c 0x1fe1a01000000001 0xffffffffffffffff 20000 0xa1 -d

# lm32 writes to MIL device via ECA wishbone channel
echo -e WRMIL: configure $DGW wishbone channel, needed for writing telegrams to the MIL device
saft-wbm-ctl $SDGW -c 0x1ff0000000000000 0xffff000000000000 0 1 -d
saft-wbm-ctl $SDGW -r 1 0x420800 0 0x5f

# lm32 listens to timing messages for EVTNO 0x000..0x0ff
saft-ecpu-ctl $SDGW -c 0x1${SIDGW}000000000000 0xfffff00000000000 500000 0xff -g -d

