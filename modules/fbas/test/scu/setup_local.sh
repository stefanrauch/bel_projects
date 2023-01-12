#!/bin/sh

# Set up MPS nodes (run it locally on SCU)

export DEV_TX="dev/wbm0"
export DEV_RX="dev/wbm0"

export addr_set_node_type="0x20140820"
export addr_get_node_type="0x20140830"
export addr_cmd="0x20140508"     # shared memory location for command buffer
export addr_cnt1="0x20140934"    # shared memory location for received frames counter
export addr_msr1="0x20140968"    # shared memory location for measurement results
export addr_eca_vld="0x20140990" # shared memory location of counter for valid actions
export addr_eca_ovf="0x20140994" # shared memory location of counter for overflow actions
export addr_senderid="0x20140998" # shared memory location of sender ID

export FW_TX="fbas.scucontrol.bin"
export FW_RX="fbas.scucontrol.bin"

export instr_fsm_configure=0x01 # FSM CONFIGURE state
export instr_fsm_opready=0x02   # FSM OPREADY state

export instr_set_nodetype=0x15  # set node type
export instr_set_io_oe=0x16     # set IO output enable
export instr_get_io_oe=0x17     # get IO output enable
export instr_toggle_io=0x18     # toggle IO output
export instr_load_senderid=0x19 # load sender ID

export instr_probe_sb_diob=0x20 # probe DIOB slave card on SCU bus
export instr_probe_sb_user=0x21 # probe a given slave (sys and group IDs are expected in shared mem @FBAS_SHARED_SET_SBSLAVES)

export instr_en_mps=0x30        # enable MPS signalling
export instr_dis_mps=0x31       # disable MPS signalling
export instr_st_tx_dly=0x32     # store the transmission delay measurement results to shared memory
export instr_st_ow_dly=0x33     # store the one-way delay measurement results to shared memory
export instr_st_sg_lty=0x34     # store the signalling latency measurement results to shared memory
export instr_st_ttl_ival=0x35   # store the TTL interval measurement results to shared memory

export      mac_tx_node="0x00267b0006d7"      # MAC address TX node
export     mac_any_node="0xffffffffffff"      # MAC address of any node
export evt_mps_flag_any="0xffffeeee00000000"  # generator event for MPS flags
export  evt_mps_flag_ok="0xffffeeee00000001"  # event to generate the MPS OK flag
export evt_mps_flag_nok="0xffffeeee00000002"  # event to generate the MPS NOK flag
export evt_mps_flag_tst="0xffffeeee00000003"  # event to generate the MPS TEST flag
export evt_mps_prot_std="0x1fcbfcb000000000"  # event with MPS protocol (regular)
export evt_mps_prot_chg="0x1fccfcc000000000"  # event with MPS protocol (change in flag)
export          evt_tlu="0xffff100000000000"  # TLU event (used to catch the signal change at IO port)
export    evt_new_cycle="0xffffdddd00000000"  # event for new cycle
export      evt_id_mask="0xffffffff00000000"  # event mask

user_approval() {
    echo -en "\nCONITNUE (Y/n)? "
    read -r answer

    if [ "$answer" != "y" ] && [ "$answer" != "Y" ] && [ "$answer" != "" ]; then
        exit 1
    fi
}

wait_seconds() {
    #echo "wait $1 seconds ..."
    sleep $1
}

wait_print_seconds() {
    # $1 - wait period in seconds

    if [ "$1" == "" ]; then
        return
    fi

    for i in $(seq 1 $1); do
        #echo -ne "time left (seconds): $[ $1 - $i ]\r"
        v=$[ $1 - $i ]
        v=$(printf "%*d\r" "8" $v)          # print numbers in 8 digits leading with spaces
        echo -ne "time left (seconds): $v"  # overwrite previous output
        wait_seconds 1
    done

}

check_mpstx() {
    if [ -z "$DEV_TX" ]; then
        echo "DEV_TX is not set"
        exit 1
    fi
}

check_mpsrx() {
    if [ -z "$DEV_RX" ]; then
        echo "DEV_RX is not set"
        exit 1
    fi
}

check_dev_fw() {

    echo "verify if a firmware runs on LM32"
    eb-info -w $1
}

list_dev_ram() {
    echo "get LM32 RAM address (host perspective)"
    eb-ls $1 | grep LM32

    # 12.14          0000000000000651:10041000           4040000  LM32-CB-Cluster
    # 12.14.5        0000000000000651:54111351           4060000  LM32-RAM-User
}

read_shared_mem() {
    # $1 - device
    # $2 - memory address

    eb-read $1 $2
}

write_shared_mem() {
    # $1 - device
    # $2 - memory address
    # $3 - value

    eb-write $1 $2 $3
}

start_saftd() {

    check_mpstx
    check_mpsrx

    echo "terminate SAFT daemon if it's running"
    sudo killall saftd

    if [ $? -eq 0 ]; then
        echo "wait until SAFT daemon terminates"
        for i in $(seq 1 10); do
            echo -ne "time left (seconds): $[ 10 - $i ]\r"
            wait_seconds 1
        done
    fi

    echo "attach mpstx:$DEV_TX and mpsrx:$DEV_RX'"
    sudo saftd mpstx:$DEV_TX mpsrx:$DEV_RX
}

check_node() {
    # $1 - node type (DEV_RX or DEV_TX)

    if [ "$1" != "DEV_RX" ] && [ "$1" != "DEV_TX" ]; then
        echo "unknown node type: $1 -> Exit!"
        exit 1
    fi
}

load_fw() {
    # $1 - node type (DEV_RX or DEV_TX)
    # $2 - firmware filename

    check_node "$1"

    unset device fw_filename

    if [ "$1" == "DEV_RX" ]; then
        check_mpsrx
        fw_filename=$FW_RX
    else
        check_mpstx
        fw_filename=$FW_TX
    fi

    if [ ! -z "$2" ]; then
        fw_filename="$2"
        if [ ! -f "$fw_filename" ]; then
            echo "'$fw_filename' not found. Exit"
            return 1
        fi
    fi

    device=$(eval echo "\$$1") # expand variable

    echo "$1: load the LM32 firmware '$fw_filename'"
    eb-fwload $device u 0x0 $fw_filename
    if [ $? -ne 0 ]; then
        echo "Error: failed to load LM32 FW '$fw_filename'. Exit!"
        exit 1
    fi
    wait_seconds 1
}

configure_node() {
    # $1 - node type (DEV_RX or DEV_TX)
    # $2 - sender node groups (SENDER_TX or SENDER_ANY or SENDER_ALL)

    check_node "$1"

    device=$(eval echo "\$$1")

    eb-write $device $addr_cmd/4 0x1
    wait_seconds 1

    if [ "$1" == "DEV_RX" ]; then
        echo "set node type to RX (0x1)"
        eb-write $device $addr_set_node_type/4 0x1
        wait_seconds 1

        echo "tell LM32 to set the node type"
        eb-write $device $addr_cmd/4 0x15
        wait_seconds 1

        echo "verify the actual node type (expected 0x1)"
        eb-read $device $addr_get_node_type/4
        wait_seconds 1

        if [ -n "$2" ]; then
            set_senderid "$2"
        fi
    fi
}

set_senderid() {
    # $1 - sender groups, valid values: SENDER_TX, SENDER_ANY, SENDER_ALL

    # SENDER_TX - only TX node
    # SENDER_ANY - only any nodes
    # SENDER_ALL - TX and any nodes

    first_idx=1
    last_idx=15
    unset idx_mac_list  # list with idx_mac

    if [ "$1" == "SENDER_TX" ]; then
        idx_mac_list="$mac_tx_node"
    elif [ "$1" == "SENDER_ALL" ] || [ "$1" == "SENDER_ANY" ]; then
        if [ "$1" == "SENDER_ALL" ]; then
            idx_mac_list="$mac_tx_node"
            first_idx=$(( $first_idx - 1 ))
            last_idx=$(( $last_idx - 1 ))
        else
            idx_mac_list="$mac_any_node"
        fi

        # idx is used to specify an MPS channel of the same sender
        for i in $(seq $first_idx $last_idx); do
            idx=$(( $i << 48 ))
            idx_mac=$(( $idx + $mac_any_node ))
            idx_mac=$(printf "0x%x" $idx_mac)
            idx_mac_list="$idx_mac_list $idx_mac"
        done
    else
        return
    fi

    echo "tell LM32 to set the sender IDs"

    device=$DEV_RX
    i=0
    for idx_mac in $idx_mac_list; do
        pos=$(( $i << 56 ))                               # position in RX buffer
        senderid=$(( $pos + $idx_mac ))                   # sender ID = position + (idx + MAC)
        senderid=$(printf "0x%x" $senderid)
        eb-write -q $device $addr_senderid/8 $senderid
        eb-read -q $device $addr_senderid/8
        eb-write $device $addr_cmd/4 $instr_load_senderid
        i=$(( $i + 1 ))
        sleep 0.2
    done

}

make_node_ready() {
    # $1 - node type (DEV_RX or DEV_TX)

    check_node "$1"

    device=$(eval echo "\$$1")

    eb-write $device $addr_cmd/4 0x2
    wait_seconds 1
}

configure_tr() {
    # $1 - node type (DEV_RX or DEV_TX)

    check_node "$1"

    echo "destroy all unowned ECA conditions"
    saft-ecpu-ctl tr0 -x

    echo "disable all events from IO inputs to ECA"
    saft-io-ctl tr0 -w

    if [ "$1" == "DEV_RX" ]; then

        echo "configure ECA: set FBAS_WR_EVT, FBAS_WR_FLG for LM32 channel, tag 0x24 and 0x25"
        saft-ecpu-ctl tr0 -c $evt_mps_prot_std $evt_id_mask 0 0x24 -d
        saft-ecpu-ctl tr0 -c $evt_mps_prot_chg $evt_id_mask 0 0x25 -d

        echo "configure ECA: listen for FBAS_AUX_CYCLE event, tag 0x26"
        saft-ecpu-ctl tr0 -c $evt_new_cycle $evt_id_mask 0 0x26 -d
    else
        echo "configure ECA: set FBAS_GEN_EVT for LM32 channel, tag 0x42"
        saft-ecpu-ctl tr0 -c $evt_mps_flag_any $evt_id_mask 0 0x42 -d

        echo "configure ECA: listen for TLU event with the given ID, tag 0x43"
        saft-ecpu-ctl tr0 -c $evt_tlu $evt_id_mask 0 0x43 -d

        echo "configure ECA: listen for FBAS_AUX_CYCLE event, tag 0x26"
        saft-ecpu-ctl tr0 -c $evt_new_cycle $evt_id_mask 0 0x26 -d

        echo "configure TLU: on signal transition at B2 input, it will generate a timing event with the given ID"
        saft-io-ctl tr0 -n B2 -b $evt_tlu

        echo "events can be snooped with a following command: saft-ctl tr0 -xv snoop 0 0 0"
        echo "also events can be presented by the LM32 firmware if WR console is active: $ eb-console \$$1"
    fi

    echo "show actual ECA conditions"
    saft-ecpu-ctl tr0 -l

    echo "list all IO conditions in ECA"
    saft-io-ctl tr0 -l
}
######################
## Make 'mpstx' ready
######################

setup_mpstx() {

    echo "load firmware"
    load_fw "DEV_TX"

    echo "CONFIGURE state "
    configure_node "DEV_TX"

    echo "OPREADY state "
    make_node_ready "DEV_TX"

    echo "configure TR"
    configure_tr "DEV_TX"
}

####################
## Make mpsrx ready
####################

setup_mpsrx() {
    # $1 - LM32 firmware
    # $2 - sender node groups

    echo "load firmware"
    load_fw "DEV_RX" "$1"

    echo "CONFIGURE state "
    configure_node "DEV_RX" $2

    echo "OPREADY state "
    make_node_ready "DEV_RX"

    echo "configure TR"
    configure_tr "DEV_RX"
}

###################
# Tests
###################

do_inject_fbas_event() {

    saft-ctl tr0 -p inject $evt_mps_flag_tst 0 1000000
}

##########################################################
# Test 4: for scuxl0497/396 in TTF
#
# TX node (scuxl0497) sends timing messages (ID=0x1fcbfcb00)
# with MPS flag. If MPS event is injected locally, then it
# sends 2x timing messages (ID=0x1fccfcc00) with MPS event
# immediatelly.
#
# RX SCU receives timing messages with MPS flag and events and
# counts number of timing messages.
#
# There is no IO connection between RX and TX nodes
##########################################################

read_counters() {
    # $1 - dev/wbm0
    # $2 - verbosity

    device=$1
    verbose=$2
    addr_val="$addr_cnt1 $addr_eca_vld $addr_eca_ovf" # reg addresses as string
    unset counts

    for addr in $addr_val; do
        cnt=$(eb-read $device $addr/4)  # get counter value
        cnt_dec=$(printf "%d" 0x$cnt)
        counts="${counts}$cnt_dec "
    done
    if [ -n "$verbose" ]; then
        counts="${counts}(tx_msg rx_vld rx_ovf)\n"
    else
        counts="${counts}\n"
    fi

    echo -e "$counts"
}

start_test4() {
    # $1 - dev/wbm0

    echo -e "\nEnable MPS task on $1"
    enable_mps $1
}

stop_test4() {
    # $1 - dev/wbm0

    echo -e "\nDisable MPS task on $1"
    disable_mps $1
}

##########################################################
# Test 3: measure network performance
# TX SCU sends MPS flag periodically in timing msg with event ID=0x1fcbfcb00 and
# sends MPS event immediately in timing msg with event ID=0x1fccfcc00.
#
# RX SCU drive its B1 output according to MPS flag or on time-out.
#
# IO connection with LEMO: RX:B1 -> TX:B2
##########################################################

start_nw_perf() {
    # $1 - number of iterations

    n=10
    if [ -n "$1" ]; then
        n=$1
    fi

    echo "TX: MPS events will be generated locally ..."
    echo "TX: $n MPS events -> flag=NOK(2), grpID=1, evtID=0 ($(( $n * 3)) transmissions)"
    echo "TX: $n MPS events -> flag=OK(1), grpID=1, evtID=0 ($n transmissions)"

    for i in $(seq $n); do
        echo -n "$i "
        saft-ctl tr0 -p inject $evt_mps_flag_nok 0x0 0
        wait_seconds 1

        saft-ctl tr0 -p inject $evt_mps_flag_ok 0x0 0
        wait_seconds 1
    done

    echo
    echo "TX: $(( $n * 2 - 1))x IO events must be snooped by 'saft-ctl tr0 -vx snoop $evt_tlu $evt_id_mask 0'"

    wait_seconds 1
    echo "TX: send 'new cycle'"
    saft-ctl tr0 -p inject $evt_new_cycle $evt_id_mask 1000000
}

result_event_count() {
    # sent/received event count

    # $1 - dev/wbmo
    # $2 - event counter
    # $3 - verbosity

    cnt=$(eb-read $1 $2/4)
    cnt_dec=$(printf "%d" 0x$cnt)
    if [ -n "$3" ]; then
        echo -n "count: "
    fi
    echo "0x$cnt (${cnt_dec})"
}

result_tx_delay() {
    # $1 - dev/wbmo
    # $2 - verbosity

    if [ -n "$2" ]; then
        echo -n "Transmit delay: "
    fi
    read_measurement_results $1 $instr_st_tx_dly $addr_msr1 $2
}

result_sg_latency() {
    # $1 - dev/wbmo
    # $2 - verbosity

    if [ -n "$2" ]; then
        echo -n "Signalling latency:   "
    fi
    read_measurement_results $1 $instr_st_sg_lty $addr_msr1 $2
}

result_ow_delay() {
    # $1 - dev/wbmo
    # $2 - verbosity

    if [ -n "$2" ]; then
        echo -n "One-way delay:  "
    fi
    read_measurement_results $1 $instr_st_ow_dly $addr_msr1 $2
}

result_ttl_ival() {
    # $1 - dev/wbmo
    # $2 - verbosity

    if [ -n "$2" ]; then
        echo -n "TTL interval:   "
    fi
    read_measurement_results $1 $instr_st_ttl_ival $addr_msr1 $2
}

disable_mps() {
    echo "Stop MPS on $1"
    eb-write $1 $addr_cmd/4 0x31
}

enable_mps() {
    echo "Start MPS on $1"
    eb-write $1 $addr_cmd/4 0x30
}

disable_mps_all() {
    echo "Disable MPS"
    disable_mps $DEV_TX
    disable_mps $DEV_RX
}

enable_mps_all() {
    echo "Enable MPS"
    enable_mps $DEV_RX
    enable_mps $DEV_TX
}

read_measurement_results() {
    # $1 - device (dev/wbm0)
    # $2 - instruction code to store measurement results to a location in the shared memory
    # $3 - shared memory location where measurement results are stored
    # $4 - verbosity

    device=$1
    instr_msr=$2
    addr_msr=$3

    eb-write $device $addr_cmd/4 $instr_msr

    avg=$(eb-read -q $device ${addr_msr}/8)
    avg_dec=$(printf "%d" 0x$avg)
    #echo "avg= 0x$avg (${avg_dec})"

    addr_msr=$(( $addr_msr + 8 ))
    min=$(eb-read -q $device ${addr_msr}/8)
    min_dec=$(printf "%lli" 0x$min)

    addr_msr=$(( $addr_msr + 8 ))
    max=$(eb-read -q $device ${addr_msr}/8)
    max_dec=$(printf "%d" 0x$max)

    addr_msr=$(( $addr_msr + 8 ))
    cnt_val=$(eb-read -q $device ${addr_msr}/4)
    cnt_val_dec=$(printf "%d" 0x$cnt_val)

    addr_msr=$(( $addr_msr + 4 ))
    cnt_all=$(eb-read -q $device ${addr_msr}/4)
    cnt_all_dec=$(printf "%d" 0x$cnt_all)
    echo -n "${avg_dec} ${min_dec} ${max_dec} ${cnt_val_dec} ${cnt_all_dec}"
    if [ -n "$4" ]; then
        echo " (avg min max valid all)"
    else
        echo
    fi
}

##########################################################
# Test 2: Measure time between a signalling and TLU events ($evt_mps_flag_any and $evt_tlu)
#
# IO connection with LEMO: RX:B1 -> TX:B2
##########################################################

do_test2() {
    echo "injectg timing messages to DEV_TX that simulate the FBAS class 2 signals"
    saft-ctl tr0 -p inject $evt_mps_flag_tst 0x0 1000000

    # Case 1: consider the ahead time of 500 us (flagForceLate=0)
    # wrc output (TX)
    #   fbas0: ECA action (tag 42, flag 0, ts 1604323287001000000, now 1604323287001004448, poll 4448)
    #   fbas0: ECA action (tag 43, flag 1, ts 1604323287001512575, now 1604323287011358920, poll 9846345) -> takes too long to output dbg msg!
    #
    # time between injecting MPS event and polling TLU event:
    # =   12575 ns (calculated by timestamp, 1604323287001512575 - 1604323287001000000 - 500000) -> not exact, because of ahead interval!
    #       ahead interval for sending timing messages is considered (COMMON_AHEADT = 500000 ns)

    # Case 2: ignore the ahead time of 500 us (flagForceLate=1)
    # wrc output (TX)
    #   fbas0: ECA action (tag 42, flag 0, ts 1604322044001000000, now 1604322044001004424, poll 4424)
    #   fbas0: ECA action (tag 43, flag 1, ts 1604322044001031206, now 1604322044011356712, poll 10325506) -> takes too long to output dbg msg!
    #
    # time period from detecting MPS event and detecting TLU events:
    # =   31206 ns (calculated by timestamp, 1604322044001031206 - 1604322044001000000)
    # time to transmit FBAS events (TX->RX):
    # =   26782 ns (1604322044001031206 - 1604322044001004424)

    # Case 3: ignore the ahead time of 500 us (flagForceLate=1), and output debug msg after handling the TLU events
    # wrc output (TX)
    #   fbas0: TLU evt (tag 43, flag 1, ts 1604325955001026839, now 1604325955001034872, poll 8033)
    #   fbas0: generator evt timestamps (detect 1604325955001000000, send 1604325955001004272, poll 4272)
    #
    #   fbas0: TLU evt (tag 43, flag 1, ts 1604325967001030350, now 1604325967001042848, poll 12498)      -> max poll time
    #   fbas0: generator evt timestamps (detect 1604325967001000000, send 1604325967001004384, poll 4384)
    #
    # time period from detecting MPS event and detecting TLU events:
    # =   42848 ns (calculated by timestamp, 1604325967001042848 - 1604325967001000000)
    # time to transmit FBAS events (TX->RX):
    # =   25966 ns (1604325967001030350 - 1604325967001004384)
}

####################
## Reset FTRN node
#
# - FW is not re-loaded
# + reset LM32
# + set node type, if 'RX'
# + set oper. mode to 'OPREADY'
####################

reset_node() {
    # $1 - node type (DEV_TX or DEV_RX)
    # $2 - sender node groups

    check_node "$1"

    if [ "$1" == "DEV_RX" ]; then
        check_mpsrx
    else
        check_mpstx
    fi

    device=$(eval echo "\$$1")

    echo "reset LM32"
    eb-reset $device cpureset 0x0
    sleep 1

    echo "CONFIGURE state "
    configure_node "$1" "$2"

    echo "OPREADY state "
    make_node_ready "$1"
}

###################
# Inject the MPS events locally
#
# Use saft-dm to inject the MPS events
###################
inject_mps_events() {
    # $1 - iteration of schedule
    # $2 - filename with the MPS event schedule

    saft-dm bla -fp -n $1 $2
}