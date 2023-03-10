/********************************************************************************************
 *  fbas.c
 *
 *  created : 2020
 *  author  : Enkhbold Ochirsuren, Dietrich Beck, GSI-Darmstadt
 *  version : 04-February-2021, 14-May-2020
 *
 *  FBAS firmware for lm32
 *  (based on common-libs/fw/example.c)
 *
 * -------------------------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2018  Dietrich Beck
 * GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 * Planckstrasse 1
 * D-64291 Darmstadt
 * Germany
 *
 * Contact: d.beck@gsi.de
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * For all questions and ideas contact: d.beck@gsi.de
 * Last update: 22-November-2018
 ********************************************************************************************/
#define FBAS_FW_VERSION 0x000002        // make this consistent with makefile

// standard includes
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// includes specific for bel_projects
#include "dbg.h"                        // debug outputs
#include "stack.h"
#include "pp-printf.h"                  // print statement
#include "mini_sdb.h"                   // sdb stuff
#include "aux.h"                        // cpu and IRQ
#include "uart.h"                       // WR console
#include "ebm.h"                        // EB master

// includes for this project
#include "common-defs.h"                // common defs for firmware
#include "common-fwlib.h"               // common routines for firmware
#include "fbas_shared_mmap.h"           // autogenerated upon building firmware
#include "fbas.h"                       // application header
#include "tmessage.h"                   // MPS flag transmission and receptions
#include "ioctl.h"                      // IO functions
#include "timer.h"                      // timer functions
#include "measure.h"                    // measurement of elapsed time, delays
#include "fwlib.h"                      // extension to fwlib

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

volatile uint32_t *pECAQ;               // WB address of ECA queue
volatile uint32_t *pPPSGen;             // WB address of PPS Gen
volatile uint32_t *pWREp;               // WB address of WR Endpoint

// global variables
// shared memory layout
uint32_t *pShared;                      // pointer to begin of shared memory region
uint32_t *pCpuRamExternal;              // external address (seen from host bridge) of this CPU's RAM
uint32_t *pSharedMacHi;                 // pointer to a "user defined" u32 register; here: high bits of MAC
uint32_t *pSharedMacLo;                 // pointer to a "user defined" u32 register; here: low bits of MAC
uint32_t *pSharedIp;                    // pointer to a "user defined" u32 register; here: IP
uint32_t *pSharedApp;                   // pointer to a "user defined" u32 register set; here: application-specific register set

// other global stuff
uint32_t statusArray;                   // all status infos are ORed bit-wise into sum status, sum status is then published

// application-specific variables
nodeType_t nodeType = FBAS_NODE_TX;     // default node type
opMode_t opMode = FBAS_OPMODE_DEF;      // default operation mode
uint32_t cntCmd = 0;                    // counter for user commands
uint32_t mpsTask = 0;                   // MPS-relevant tasks
uint64_t mpsTimMsgFlagId = 0;           // timing message ID for MPS flags
uint64_t mpsTimMsgEvntId = 0;           // timing message ID for MPS events
volatile uint64_t tsCpu = 0;            // lm32 uptime
volatile int64_t  prdTimer;             // timer period
uint32_t io_chnl = IO_CFG_CHANNEL_GPIO; // IO channel type (LVDS for Pexaria, GPIO for SCU)

// application-specific function prototypes
static void init();
static void initSharedMem(uint32_t *sharedSize);
static void initMpsData();
static void initIrqTable();
static void printSrcAddr();
static status_t convertMacToU8(uint8_t buf[ETH_ALEN], uint32_t* hi, uint32_t* lo);
static status_t convertMacToU64(uint64_t* buf, uint32_t* hi, uint32_t* lo);
static status_t setDstAddr(uint64_t dstMac, uint32_t dstIp);
static status_t loadSenderId(uint32_t* base, uint32_t offset);
static void clearError(size_t len, mpsMsg_t* buf);
static void setOpMode(uint64_t mode);
static void cmdHandler(uint32_t *reqState, uint32_t cmd);
static void timerHandler();
static uint32_t handleEcaEvent(uint32_t usTimeout, uint32_t* mpsTask, timedItr_t* itr, mpsMsg_t** head);
static void wrConsolePeriodic(uint32_t seconds);

/**
 * \brief init for lm32
 *
 * Basic initialization for lm32 firmware
 *
 **/
void init()
{
  discoverPeriphery();        // mini-sdb ...
  uart_init_hw();             // needed by WR console
  cpuId = getCpuIdx();
} // init

/**
 * \brief set up shared memory
 *
 * Set up user defined u32 register set in the shared memory
 *
 **/
void initSharedMem(uint32_t *sharedSize)
{
  uint32_t idx;
  uint32_t *pSharedTemp;
  int      i;
  const uint32_t c_Max_Rams = 10;
  sdb_location found_sdb[c_Max_Rams];
  sdb_location found_clu;

  // get pointer to shared memory
  pShared           = (uint32_t *)_startshared;

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if(idx >= cpuId) pCpuRamExternal           = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  // print WB addresses (shared RAM, range reserved to user, command buffer etc) to WR console
  pSharedTemp = pCpuRamExternal + (SHARED_OFFS >> 2) + (COMMON_SHARED_CMD >> 2);
  DBPRINT2("fbas: CPU RAM External 0x%8x, begin shared 0x%08x, command 0x%08x\n",
      pCpuRamExternal, SHARED_OFFS, pSharedTemp);

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_BEGIN >> 2 ));
  DBPRINT2("fbas: app specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (FBAS_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  }

  // get shared memory usage
  *sharedSize = ((uint32_t)(pSharedTemp - pShared) << 2);

  // print application-specific register set (in shared mem)
  pSharedApp = (uint32_t *)(pShared + (FBAS_SHARED_SET_GID >> 2));
  DBPRINT2("fbas%d: SHARED_SET_NODETYPE 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_SET_NODETYPE >> 2)));
  DBPRINT2("fbas%d: SHARED_GET_NODETYPE 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_GET_NODETYPE >> 2)));
  DBPRINT2("fbas%d: SHARED_GET_TS1 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_GET_TS1 >> 2)));

  // reset all event counters
  *(pSharedApp + (FBAS_SHARED_GET_CNT >> 2)) = msrSetCnt(TX_EVT_CNT, 0);
  *(pSharedApp + (FBAS_SHARED_ECA_VLD >> 2)) = msrSetCnt(ECA_VLD_ACT, 0);
  *(pSharedApp + (FBAS_SHARED_ECA_OVF >> 2)) = msrSetCnt(ECA_OVF_ACT, 0);
  DBPRINT2("fbas%d: SHARED_GET_CNT 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_GET_CNT >> 2)));
  DBPRINT2("fbas%d: SHARED_CNT_VAL 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_ECA_VLD >> 2)));
  DBPRINT2("fbas%d: SHARED_CNT_OVF 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_ECA_OVF >> 2)));
  DBPRINT2("fbas%d: SHARED_SENDERID 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_SENDERID >> 2)));
} // initSharedMem

/**
 * \brief Initialize application specific data structures
 *
 * Initialize task control flag, timing event IDs, MPS message buffer
 *
 **/
void initMpsData()
{
  mpsTask = 0;

  mpsTimMsgFlagId = fwlib_buildEvtidV1(FBAS_FLG_GID, FBAS_FLG_EVTNO,
      FBAS_FLG_FLAGS, FBAS_FLG_SID, FBAS_FLG_BPID, FBAS_FLG_RES);
  mpsTimMsgEvntId = fwlib_buildEvtidV1(FBAS_EVT_GID, FBAS_EVT_EVTNO,
      FBAS_EVT_FLAGS, FBAS_EVT_SID, FBAS_EVT_BPID, FBAS_EVT_RES);

  // initialize MPS message buffer
  // - RX node: use own MAC address as valid sender IDs -> other nodes do not have the same MAC
  // - TX node: sender ID is its MAC address
  uint64_t mac;
  convertMacToU64(&mac, pSharedMacHi, pSharedMacLo);

  for (int i = 0; i < N_MPS_CHANNELS; ++i) {
    bufMpsMsg[i].prot.flag  = MPS_FLAG_TEST;
    bufMpsMsg[i].prot.idx = 0;
    setMpsMsgSenderId(&bufMpsMsg[i], mac, 1);
    bufMpsMsg[i].ttl = 0;
    bufMpsMsg[i].tsRx = 0;
    DBPRINT1("%x: mac=%x:%x:%x:%x:%x:%x idx=%x flag=%x\n",
        i, bufMpsMsg[i].prot.addr[0], bufMpsMsg[i].prot.addr[1], bufMpsMsg[i].prot.addr[2],
        bufMpsMsg[i].prot.addr[3], bufMpsMsg[i].prot.addr[4], bufMpsMsg[i].prot.addr[5],
        bufMpsMsg[i].prot.idx, bufMpsMsg[i].prot.flag);
  }

  // initialize the read iterator for MPS flags
  initItr(&rdItr, N_MPS_CHANNELS, 0, F_MPS_BCAST);

  //TODO: include function call below in fwlib_doActionS0()
  // if (findEcaCtl() != COMMON_STATUS_OK) status = COMMON_STATUS_ERROR;
  if (findEcaCtl() != COMMON_STATUS_OK) {
    DBPRINT1("ECA ctl not found!");
  }
}

/**
 * \brief initialize IRQ table
 *
 * Configure the WB timer
 *
 * \param none
 * \ret none
 **/
void initIrqTable()
{
  isr_table_clr();                               // clear table
  // isr_ptr_table[0] = &irq_handler;            // 0: hard-wired MSI; don't use here
  isr_ptr_table[1] = &timerHandler;              // 1: hard-wired timer
  irq_set_mask(0x02);                            // only use timer
  irq_enable();                                  // enable IRQs
  DBPRINT2("Configured IRQ table.\n");
}

/**
 * \brief check the source MAC and IP addresses of Endpoint
 *
 * Check the source MAC and IP addresses of the Endpoint WB device
 *
 * \ret status
 **/
void printSrcAddr()
{
  uint32_t octet0 = 0x000000ff;
  uint32_t octet1 = octet0 << 8;
  uint32_t octet2 = octet0 << 16;
  uint32_t octet3 = octet0 << 24;

  DBPRINT1("fbas%d: MAC=%02x:%02x:%02x:%02x:%02x:%02x, IP=%d.%d.%d.%d\n", nodeType,
      (*pSharedMacHi & octet1) >> 8, (*pSharedMacHi & octet0),
      (*pSharedMacLo & octet3) >> 24,(*pSharedMacLo & octet2) >> 16,
      (*pSharedMacLo & octet1) >> 8, (*pSharedMacLo & octet0),
      (*pSharedIp & octet3) >> 24,(*pSharedIp & octet2) >> 16,
      (*pSharedIp & octet1) >> 8, (*pSharedIp & octet0));
}

/**
 * \brief Convert MAC address to array of uint8
 *
 * \param hi  Source buffer address with MAC high octets
 * \param lo  Source buffer address with MAC low octets
 * \param buf Destination buffer address
 *
 * \ret status Zero on success
 **/

status_t convertMacToU8(uint8_t buf[ETH_ALEN], uint32_t* hi, uint32_t* lo)
{
  status_t status;
  uint64_t mac = 0;
  uint8_t bits = 0;

  status = convertMacToU64(&mac, hi, lo);

  if (status == COMMON_STATUS_OK) {
    for (int i = ETH_ALEN - 1; i >= 0; i--) {
      buf[i] = mac >> bits;
      bits += 8;
    }
  }

  return status;
}

/**
 * \brief Convert MAC address to uint64
 *
 * \param hi  Source buffer address with MAC high octets
 * \param lo  Source buffer address with MAC low octets
 * \param buf Destination buffer address
 *
 * \ret status Zero on success
 **/

status_t convertMacToU64(uint64_t* buf, uint32_t* hi, uint32_t* lo)
{
  if (!(hi && lo && buf)) {
    DBPRINT1("null pointer: %x %x %x\n", buf, hi, lo);
    return COMMON_STATUS_ERROR;
  }

  *buf = *hi & 0x0000ffff;
  *buf = *buf << 32;
  *buf += *lo;

  return COMMON_STATUS_OK;
}

/**
 * \brief set the destination MAC and IP addresses
 *
 * Set the destination MAC and IP addresses of the Endpoint WB device.
 *
 * \ret status
 *
 **/
status_t setDstAddr(uint64_t dstMac, uint32_t dstIp)
{
  uint32_t status = COMMON_STATUS_OK;

  // configure Etherbone master (src MAC and IP are set by host, i.e. by eb-console or BOOTP)
  if ((status = fwlib_ebmInit(TIM_2000_MS, dstMac, dstIp, EBM_NOREPLY)) != COMMON_STATUS_OK) {
    DBPRINT1("fbas%d: Error - source IP is not set!\n", nodeType); // IP unset
  }

  return status;
}

/**
 * \brief Load sender ID
 *
 * Read the raw sender ID and sets it to designated MPS message buffer.
 * Raw data contains: position + index + MAC
 * where, position specifies a concrete MPS message buffer.
 *
 * \param base   Base address of shared memory
 * \param offset Offset to a location with a valid sender ID
 *
 * \ret status   Zero on success
 **/
status_t loadSenderId(uint32_t* base, uint32_t offset)
{
  uint64_t* pSenderId = (uint64_t *)(base + (offset >> 2));
  uint8_t pos = *pSenderId >> 56;         // position of MPS message buffer

  if (pos >= N_MPS_CHANNELS) {
    DBPRINT1("fbas%d: pos %d in %llx is out of range!\n", nodeType, pos, *pSenderId);
    return COMMON_STATUS_ERROR;
  }

  mpsMsg_t* msg = &bufMpsMsg[pos];
  msg->prot.idx = *pSenderId >> 48;       // index of MPS channel

  setMpsMsgSenderId(msg, *pSenderId, 1);

  return COMMON_STATUS_OK;
}

/**
 * \brief Clear latched error
 *
 * Errors caused by lost messages or NOK flag are being latched until new cycle.
 * [MPS_FS_600]
 *
 * \param buf Pointer to MPS message buffer
 *
 **/
void clearError(size_t len, mpsMsg_t* buf) {

  for (size_t i = 0; i < len; ++i) {
    driveEffLogOut(io_chnl, buf + i);
  }
}

/**
 * \brief set operation mode
 *
 * \param mode raw data with operation mode
 *
 **/
void setOpMode(uint64_t mode) {
  if (mode)
    opMode = FBAS_OPMODE_TEST;
  else
    opMode = FBAS_OPMODE_DEF;
}

/**
 * \brief handle pending ECA event
 *
 * On FBAS_GEN_EVT event the buffer for MPS flag is updated and \head returns
 * pointer to it. Otherwise, \head is returned with null value.
 *
 * On FBAS_WR_EVT or FBAS_WR_FLG event the effective logic output is driven.
 *
 * \param usTimeout maximum interval in microseconds to poll ECA
 * \param mpsTask   pointer to MPS-relevant task flag
 * \param itr       pointer to the read iterator for MPS flags
 * \param head      pointer to pointer of the MPS message buffer
 *
 * \return ECA action tag (COMMON_ECADO_TIMEOUT on timeout, otherwise non-zero tag)
 **/
uint32_t handleEcaEvent(uint32_t usTimeout, uint32_t* mpsTask, timedItr_t* itr, mpsMsg_t** head)
{
  uint32_t nextAction;    // action triggered by received ECA event
  uint64_t ecaDeadline;   // deadline of received ECA event
  uint64_t ecaEvtId;      // ID of received ECA event
  uint64_t ecaParam;      // parameter value in received ECA event
  uint32_t ecaTef;        // TEF value in received ECA event
  uint32_t flagIsLate;    // flag indicates that received ECA event is 'late'
  uint32_t flagIsEarly;   // flag indicates that received ECA event is 'early'
  uint32_t flagIsConflict;// flag indicates that received ECA event is 'conflict'
  uint32_t flagIsDelayed; // flag indicates that received ECA event is 'delayed'
  uint64_t now;           // actual timestamp of the system time
  int64_t  poll;          // elapsed time to poll a pending ECA event
  uint32_t actions;

  nextAction = fwlib_wait4ECAEvent(usTimeout, &ecaDeadline, &ecaEvtId, &ecaParam, &ecaTef,
    &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);

  if (nextAction) {
    now = getSysTime();
    storeTimestamp(pSharedApp, FBAS_SHARED_GET_TS5, now);

    switch (nextAction) {
      case FBAS_AUX_NEWCYCLE:
        if (nodeType == FBAS_NODE_TX) { // it takes 1848/6328 ns for 2/32 MPS channels
          // reset MPS msgs
          resetMpsMsg(N_MPS_CHANNELS, *head);

          // init the read iterator for MPS flags, so that iteration is delayed for 52 ms [MPS_FS_630]
          now += TIM_52_MS;
          initItr(itr, N_MPS_CHANNELS, now, F_MPS_BCAST);

        } else if (nodeType == FBAS_NODE_RX) { // it takes 2480/31048 ns for 2/32 MPS channels
          // reset effective logic input to HIGH bit (delay for 52 ms) [MPS_FS_630]
          resetMpsMsg(N_MPS_CHANNELS, *head);
          // clear latched errors [MPS_FS_600]
          clearError(N_MPS_CHANNELS, *head);
        }
        now = getSysTime();
        DBPRINT2("%lli\n", getElapsedTime(pSharedApp, FBAS_SHARED_GET_TS5, now));
        break;

      case FBAS_AUX_OPMODE:
        setOpMode(ecaParam);

        if (nodeType == FBAS_NODE_TX) { // TODO: measure elapsed time
          // each gate shall be fully qualifed [MPS_FS_740]
          // no variable besides deliberate exceptions shall be unmasked [MPS_FS_740]
          // flag change suppressed 0,5 us after test begin or end [MPS_FS_550]
          qualifyInput(N_MPS_CHANNELS, *head);

        } else if (nodeType == FBAS_NODE_RX) {
          // invert output
          testOutput(N_MPS_CHANNELS, *head);
        }
        now = getSysTime();
        DBPRINT2("%lli\n", getElapsedTime(pSharedApp, FBAS_SHARED_GET_TS5, now));
        break;

      case FBAS_GEN_EVT:
        if (nodeType == FBAS_NODE_TX) {// only FBAS TX node handles the MPS events
          // update MPS flag
          *head = updateMpsMsg(*head, ecaEvtId);

          if (*head && (*mpsTask & TSK_TX_MPS_EVENTS)) {
            // send MPS event
            if (sendMpsMsgSpecific(itr, *head, mpsTimMsgFlagId, N_EXTRA_MPS_NOK) == COMMON_STATUS_OK) {
              // count sent timing messages with MPS event
              *(pSharedApp + (FBAS_SHARED_GET_CNT >> 2)) = msrCnt(TX_EVT_CNT, 1);
              if ((*head)->prot.flag == MPS_FLAG_NOK) {
                *(pSharedApp + (FBAS_SHARED_GET_CNT >> 2)) = msrCnt(TX_EVT_CNT, N_EXTRA_MPS_NOK);
              }
            }

            // store timestamps to measure delays
            storeTsMeasureDelays(pSharedApp, FBAS_SHARED_GET_TS1, now, ecaDeadline);
          }
        }
        break;
      case FBAS_TLU_EVT:
        if (nodeType == FBAS_NODE_TX && *mpsTask & TSK_TX_MPS_EVENTS) {// only FBAS TX node handles the TLU events
          // measure network delay (broadcast MPS events from TX to RX nodes) and
          // signalling latency (from MPS event generation at TX to IO event detection at RX)
          measureNwPerf(pSharedApp, FBAS_SHARED_GET_TS1, nextAction, flagIsLate, now, ecaDeadline, 0);
        }
        break;
      case FBAS_WR_EVT:
      case FBAS_WR_FLG:
        if (nodeType == FBAS_NODE_RX) { // FBAS RX generates MPS class 2 signals

          // count received timing messages with MPS flag or MPS event
          if (COMMON_STATUS_OK == fwlib_getEcaValidCnt(&actions))    // number of the valid actions
            *(pSharedApp + (FBAS_SHARED_ECA_VLD >> 2)) = msrCnt(ECA_VLD_ACT, actions);

          if (COMMON_STATUS_OK == fwlib_getEcaOverflowCnt(&actions)) // number of the overflow actions
            *(pSharedApp + (FBAS_SHARED_ECA_OVF >> 2)) = msrCnt(ECA_OVF_ACT, actions);

          // store and handle received MPS flag
          *head = storeMpsMsg(ecaParam, ecaDeadline, itr);
          if (*head) {
            driveEffLogOut(io_chnl, *head);

            // measure one-way delay
            measureOwDelay(now, ecaDeadline, 0);
          }
        }
        break;
      default:
        break;
    }
  }

  if (nextAction != FBAS_GEN_EVT)
    *head = 0;

  return nextAction;
}

/**
 * \brief write a debug text to console
 *
 * Write a debug text to the WR console in given period (seconds)
 *
 * \param seconds period in seconds
 *
 * \ret none
 **/
void wrConsolePeriodic(uint32_t seconds)
{
  static uint64_t tsLast = 0;                 // timestamp of last call

  uint64_t period = seconds * TIM_1000_MS;    // period in system time
  uint64_t soon = tsLast + period;            // next time point for the action
  uint64_t now = getSysTime();                // get the current time

  if (now >= soon) {                          // if the given period is over, then proceed
    DBPRINT3("fbas%d: now %llu, elap %lli\n", nodeType, now, now - tsLast);
    tsLast = now;
  }

  // lm32 cpu time, timer interval and period
  if (tsCpu) {
    DBPRINT2("cpu %llu [ns], ival %lli [ns], prd %lli [ns]\n", tsCpu, getElapsedTime(pSharedApp, FBAS_SHARED_GET_TS3, tsCpu), prdTimer);
    tsCpu = 0;
  }
}


// clears all statistics
void extern_clearDiag()
{
  // ... insert code here
} // clearDiag

// entry action configured state
/**
 * \brief initialize application relevant components
 *
 * Routine is performed in the configuration stage
 *
 * \ret status
 **/
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  DBPRINT2("fbas%d: pIOCtrl=%08x, pECAQ=%08x\n", nodeType, pIOCtrl, pECAQ);

  DBPRINT1("fbas%d: designated platform = %s\n", nodeType, MYPLATFORM);
  if (MYPLATFORM == "pcicontrol")   // GPIO for SCU, LVDS for Pexiara
    io_chnl=IO_CFG_CHANNEL_LVDS;

  fwlib_ioCtrlSetGate(0, 2);        // disable input gate
  setIoOe(io_chnl, 0);              // enable output for the IO1 (or B1) port

  fwlib_publishNICData();           // NIC data (MAC, IP) are assigned to global variables (pSharedIp, pSharedMacHi/Lo)
  printSrcAddr();                   // output the source MAC/IP address of the Endpoint WB device to the WR console

  status = setDstAddr(BROADCAST_MAC, BROADCAST_IP); // set the destination broadcast MAC/IP address of the Endpoint WB device
  if (status != COMMON_STATUS_OK) return status;

  if ((uint32_t)pCpuWbTimer != ERROR_NOT_FOUND) {
    setupTimer(TIM_1_MS);
    initIrqTable();
    startTimer();
  }

  initMpsData();                    // initialize application specific data structure

  return status;
} // entryActionConfigured

// entry action state 'op ready'
uint32_t extern_entryActionOperation()
{
  uint32_t status = COMMON_STATUS_OK;

  //... insert code here

  return status;
} // entryActionOperation

// exit action state 'op ready'
uint32_t extern_exitActionOperation(){
  uint32_t status = COMMON_STATUS_OK;

  //... insert code here

  return status;
} // exitActionOperation

/**
 * \brief handle user-defined commands (instructions)
 *
 * \param reqState  request state?
 * \param cmd       received user command
 *
 * \ret none
 **/
void cmdHandler(uint32_t *reqState, uint32_t cmd)
{
  uint32_t u32val;
  uint8_t u8val;
  // check, if the command is valid and request state change
  if (cmd) {                             // check, if cmd is valid
    cntCmd++;
    switch (cmd) {                       // do action according to command
      case FBAS_CMD_SET_NODETYPE:
        u32val = *(pSharedApp + (FBAS_SHARED_SET_NODETYPE >> 2));
        if (u32val < FBAS_NODE_UNDEF) {
          nodeType = u32val;
          *(pSharedApp + (FBAS_SHARED_GET_NODETYPE >> 2)) = nodeType;
          DBPRINT2("fbas%d: node type %x\n", nodeType, nodeType);
        } else {
          DBPRINT2("fbas%d: invalid node type %x\n", nodeType, u32val);
        }
        break;
      case FBAS_CMD_GET_SENDERID:
        // read valid sender ID (MAC, idx) from the shared memory
        loadSenderId(pSharedApp, FBAS_SHARED_SENDERID);
        break;
      case FBAS_CMD_SET_IO_OE:
        setIoOe(io_chnl, 0);  // enable output for the IO1 (B1) port
        break;
      case FBAS_CMD_GET_IO_OE:
        u32val = getIoOe(io_chnl);
        if (1) {
          DBPRINT2("fbas%d: GPIO OE %x\n", nodeType, u32val);
        }
        break;
      case FBAS_CMD_TOGGLE_IO:
        u8val = cntCmd & 0x01;
        u32val = 0;
        driveIo(io_chnl, u32val, u8val);
        DBPRINT2("fbas%d: IO%d=%x\n", nodeType, u32val+1, u8val);
        break;
      case FBAS_CMD_EN_MPS_FWD:
        mpsTask |= TSK_TX_MPS_FLAGS;  // enable transmission of the MPS flags
        mpsTask |= TSK_TX_MPS_EVENTS; // enable transmission of the MPS events
        mpsTask |= TSK_TTL_MPS_FLAGS; // enable lifetime monitoring of the MPS flags
        DBPRINT2("fbas%d: enabled MPS %x\n", nodeType, mpsTask);
        break;
      case FBAS_CMD_DIS_MPS_FWD:
        mpsTask &= ~TSK_TX_MPS_FLAGS;  // disable transmission of the MPS flags
        mpsTask &= ~TSK_TX_MPS_EVENTS; // disable transmission of the MPS events
        mpsTask &= ~TSK_TTL_MPS_FLAGS; // disable lifetime monitoring of the MPS flags
        DBPRINT2("fbas%d: disabled MPS %x\n", nodeType, mpsTask);
        break;
      case FBAS_CMD_PRINT_NW_DLY:
        printMeasureTxDelay(pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_PRINT_SG_LTY:
        printMeasureSgLatency(pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_PRINT_OWD:
        printMeasureOwDelay(pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_PRINT_TTL:
        printMeasureTtl(pSharedApp, FBAS_SHARED_GET_AVG);
        break;

      default:
        DBPRINT2("fbas%d: received unknown command '0x%08x'\n", nodeType, cmd);
        break;
    } // switch
  } // if command
} // cmdHandler

/**
 * \brief timer interrupt handler
 *
 * Callback routine for timer interrupt
 *
 * \param none
 *
 * \ret none
 **/
void timerHandler()
{
  static uint32_t prescaler = 0;

  uint64_t cputime = getCpuTime();
  prdTimer = getElapsedTime(pSharedApp, FBAS_SHARED_GET_TS6, cputime);

  ++prescaler;
  if (prescaler == PSCR_1S_TIM_1MS) {
    prescaler = 0;
    tsCpu = cputime;
  }
}

// do action state 'op ready' - this is the main code of this FW
uint32_t doActionOperation(uint32_t* pMpsTask,          // MPS-relevant tasks
                           mpsMsg_t* pBufMpsMsg,        // pointer to MPS message buffer
                           timedItr_t* pRdItr,          // iterator used to read MPS flags buffer
                           uint32_t actStatus)          // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t nSeconds = 15;                                     // time period in secondes
  uint32_t nUSeconds = 100;                                   // time period in microseconds
  uint32_t action;                                            // ECA action tag
  mpsMsg_t* buf = pBufMpsMsg;                                 // pointer to MPS message buffer

  status = actStatus;

  // action driven by ECA event, polls for nUSeconds [us]
  action = handleEcaEvent(nUSeconds, pMpsTask, pRdItr, &buf);   // handle ECA event

  switch (nodeType) {

    case FBAS_NODE_TX:
      // transmit MPS flags (flags are sent in specified period, but events immediately)
      // tx_period=1000000/(N_MPS_CHANNELS * F_MPS_BCAST) [us], tx_period(max) < nUSeconds
      if (*pMpsTask & TSK_TX_MPS_FLAGS) {
        if (sendMpsMsgBlock(N_MPS_FLAGS, pRdItr, mpsTimMsgFlagId) == COMMON_STATUS_OK)
          // count sent timing messages with MPS flag
          *(pSharedApp + (FBAS_SHARED_GET_CNT >> 2)) = msrCnt(TX_EVT_CNT, N_MPS_FLAGS);
      }
      else
        wrConsolePeriodic(nSeconds);   // periodic debug (level 3) output at console
      break;

    case FBAS_NODE_RX:
      if (*pMpsTask & TSK_TTL_MPS_FLAGS) {
        // monitor lifetime of MPS flags periodically and handle expired MPS flag
        buf = expireMpsMsg(pRdItr);
        if (buf) {
          driveEffLogOut(io_chnl, buf);
          measureTtlInterval(buf);
        }
      }
      break;

    default:
      break;
  }

  return status;
} // doActionOperation

int main(void) {
  uint32_t status;                                            // (error) status
  uint32_t cmd;                                               // command via shared memory
  uint32_t actState;                                          // actual FSM state
  uint32_t pubState;                                          // value of published state
  uint32_t reqState;                                          // requested FSM state
  uint32_t *buildID;
  uint32_t sharedSize;                                        // shared memory size

  // init local variables
  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  buildID        = (uint32_t *)(INT_BASE_ADR + BUILDID_OFFS); // required for 'stack check'

  // init
  init();                                                              // initialize stuff for lm32
  fwlib_clearDiag();                                                   // clear common diagnostic data
  initSharedMem(&sharedSize);                                          // initialize shared memory
  fwlib_init((uint32_t *)_startshared, pCpuRamExternal, SHARED_OFFS, sharedSize, "fbas", FBAS_FW_VERSION); // init common stuff
  //initMpsData();                                                       // initialize application specific data structure

  while (1) {
    check_stack_fwid(buildID);                                         // check for stack corruption
    fwlib_cmdHandler(&reqState, &cmd);                                 // check for common commands and possibly request state changes
    cmdHandler(&reqState, cmd);                                        // check for project relevant commands
    status = COMMON_STATUS_OK;                                         // reset status for each iteration
    status = fwlib_changeState(&actState, &reqState, status);          // handle requested state changes
    switch(actState) {                                                 // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&mpsTask, bufMpsMsg, &rdItr, status);
        if (status == COMMON_STATUS_WRBADSYNC) reqState = COMMON_STATE_ERROR;
        if (status == COMMON_STATUS_ERROR)     reqState = COMMON_STATE_ERROR;
        break;
      default :                                                        // avoid flooding WB bus with unnecessary activity
        status = fwlib_doActionState(&reqState, actState, status);     // handle do actions states
        break;
    } // switch

    // update sum status
    switch (status) {
      case COMMON_STATUS_OK :                                                     // status OK
        statusArray = statusArray |  (0x1 << COMMON_STATUS_OK);                   // set OK bit
        break;
      default :                                                                   // status not OK
        if ((statusArray >> COMMON_STATUS_OK) & 0x1) fwlib_incBadStatusCnt();     // changing status from OK to 'not OK': increase 'bad status count'
        statusArray = statusArray & ~(0x1 << COMMON_STATUS_OK);                   // clear OK bit
        statusArray = statusArray |  (0x1 << status);                             // set status bit and remember other bits set
        break;
    } // switch status

    // update shared memory
    if ((pubState == COMMON_STATE_OPREADY) && (actState  != COMMON_STATE_OPREADY)) fwlib_incBadStateCnt();
    fwlib_publishStatusArray(statusArray);
    pubState             = actState;
    fwlib_publishState(pubState);
    // ... insert code here
  } // while

  return(1);
} // main
