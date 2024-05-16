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
#define FBAS_FW_VERSION 0x010300        // make this consistent with makefile

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
#define  SHARED  __attribute__((section(".shared")))
extern uint32_t* _startshared[];
static unsigned int cpuId;

// common-fwlib globals
extern volatile uint32_t *pECAQ;        // WB address of ECA queue
extern uint32_t *pSharedMacHi;          // pointer to a "user defined" u32 register; here: high bits of MAC
extern uint32_t *pSharedMacLo;          // pointer to a "user defined" u32 register; here: low bits of MAC
extern uint32_t *pSharedIp;             // pointer to a "user defined" u32 register; here: IP

uint64_t myMac;                         // own MAC address
uint8_t  myIdx;                         // base index of TX node (used for MPS messaging)

// shared memory layout
static uint32_t *pCpuRamExternal;       // external address (seen from host bridge) of this CPU's RAM
static uint32_t *pSharedExt;            // external address of shared memory reserved for the application
static uint32_t *pSharedApp;            // internal address of shared memory reserved for the application

// other global stuff
static uint32_t statusArray;            // all status infos are ORed bit-wise into sum status, sum status is then published

// application-specific global variables
static nodeType_t nodeType = FBAS_NODE_TX;  // default node type
static opMode_t opMode = FBAS_OPMODE_DEF;   // default operation mode
static uint32_t cntCmd = 0;                 // counter for user commands
static uint32_t mpsTask = 0;                // MPS-relevant tasks

static volatile int64_t  prdTimer;          // timer period
static nw_addr_t dstNwAddr[N_DST_ADDR];     // valid destination addresses for the Endpoint WB device
// timers
static struct timer_s *pTimerMpsTtl;
static struct timer_s *pTimerRegistr;
static struct timer_s *pTimerConsole;
// timer debug
static struct timer_dbg_s timerDbg;

// application-specific function prototypes
static void init();
static status_t initSharedMem(uint32_t *const sharedStart);
static void initMpsData();
static void initIrqTable();
static void printSrcAddr();
static status_t convertMacToU8(uint8_t buf[ETH_ALEN], uint32_t* hi, uint32_t* lo);
static status_t convertMacToU64(uint64_t* buf, uint32_t* hi, uint32_t* lo);
static status_t setEndpDstAddr(int idx);
static status_t readNodeId(uint32_t* base, uint32_t offset);
static void clearError(size_t len, mpsMsg_t* buf);
static void setOpMode(uint64_t mode);
static void cmdHandler(uint32_t *reqState, uint32_t cmd);
static void timerHandler(void);
static uint32_t handleEcaEvent(uint32_t usTimeout, uint32_t* mpsTask, msgCtrl_t* pMsgCtrl, mpsMsg_t** head);
static void wrConsolePeriodic(void);

/**
 * \brief init for lm32
 *
 * Basic initialization for lm32 firmware
 *
 **/
static void init()
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
static status_t initSharedMem(uint32_t *const sharedStart)
{
  uint32_t idx, sharedSize;
  uint32_t *pShared, *pSharedTemp;
  const uint32_t c_Max_Rams = 10;
  sdb_location found_sdb[c_Max_Rams];
  sdb_location found_clu;

  // get pointer to shared memory
  pSharedApp = sharedStart;

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if (idx < cpuId)
    return COMMON_STATUS_ERROR;

  pCpuRamExternal = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  // print WB addresses to WR console (shared RAM, begin of common shared region)
  pSharedExt = pCpuRamExternal + ((SHARED_OFFS + COMMON_SHARED_BEGIN) >> 2);
  DBPRINT2("fbas: CPU RAM: 0x%8p, common shared: 0x%8p\n", pCpuRamExternal, pSharedExt);

  // init common shared region
  pSharedTemp = (uint32_t *)(pSharedApp + (FBAS_SHARED_END >> 2 ));
  sharedSize = ((uint32_t)(pSharedTemp - pSharedApp) << 2);
  fwlib_init(pSharedApp, pCpuRamExternal, SHARED_OFFS, sharedSize, "fbas", FBAS_FW_VERSION);

  // print application-specific register set (in shared mem)
  DBPRINT2("fbas%d: COMMON_CMD 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (COMMON_SHARED_CMD >> 2)), (pSharedExt + (COMMON_SHARED_CMD >> 2)));
  DBPRINT2("fbas%d: FBAS_BEGIN 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_BEGIN >> 2)), (pSharedExt + (FBAS_SHARED_BEGIN >> 2)));
  DBPRINT2("fbas%d: FBAS_SET_NODETYPE 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_SET_NODETYPE >> 2)), (pSharedExt + (FBAS_SHARED_SET_NODETYPE >> 2)));
  DBPRINT2("fbas%d: FBAS_GET_NODETYPE 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_GET_NODETYPE >> 2)), (pSharedExt + (FBAS_SHARED_GET_NODETYPE >> 2)));
  DBPRINT2("fbas%d: FBAS_GET_CNT 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_GET_CNT >> 2)), (pSharedExt + (FBAS_SHARED_GET_CNT >> 2)));
  DBPRINT2("fbas%d: FBAS_GET_AVG 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_GET_AVG >> 2)), (pSharedExt + (FBAS_SHARED_GET_AVG >> 2)));
  DBPRINT2("fbas%d: FBAS_ECA_VLD 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_ECA_VLD >> 2)), (pSharedExt + (FBAS_SHARED_ECA_VLD >> 2)));
  DBPRINT2("fbas%d: FBAS_ECA_OVF 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_ECA_OVF >> 2)), (pSharedExt + (FBAS_SHARED_ECA_OVF >> 2)));
  DBPRINT2("fbas%d: FBAS_SENDERID 0x%8p (0x%8p)\n", nodeType, (pSharedApp + (FBAS_SHARED_SENDERID >> 2)), (pSharedExt + (FBAS_SHARED_SENDERID >> 2)));

  // clear the app-spec region of the shared memory
  pSharedTemp = (uint32_t *)(pSharedApp + (FBAS_SHARED_END >> 2 ));
  pShared = pSharedApp;
  while (pShared < pSharedTemp) {
    *pShared = 0;
    pShared++;
  }

  return COMMON_STATUS_OK;
}

/**
 * \brief Initialize application specific data structures
 *
 * Initialize task control flag, timing event IDs, MPS message buffer
 *
 **/
static void initMpsData()
{
  mpsTask = 0;

  // initialize MPS message buffer
  // - RX node: use own MAC address as valid sender IDs -> other nodes do not have the same MAC
  // - TX node: sender ID is its MAC address
  convertMacToU64(&myMac, pSharedMacHi, pSharedMacLo);

  // initialize the MPS message buffer
  msgInitMpsMsg(&myMac);

  // initialize the MPS messaging controller
  msgInitMsgCtrl(&mpsMsgCtrl, N_MPS_CHANNELS, 0, F_MPS_BCAST);

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
static void initIrqTable()
{
  isr_table_clr();                               // clear table
  // isr_ptr_table[0] = &irq_handler;            // 0: hard-wired MSI; don't use here
  isr_ptr_table[1] = &timerHandler;              // 1: hard-wired timer
  irq_set_mask(0x02);                            // only use timer
  irq_enable();                                  // enable IRQs
  DBPRINT2("Configured IRQ table.\n");
}

static status_t initTimers(void)
{
  // timers
  pTimerMpsTtl  = timerRegister(1);       // 1 ms
  pTimerRegistr = timerRegister(1000);    // 1 sec
  pTimerConsole = timerRegister(60000);   // 60 sec

  // timer debug
  timerInitDbg(&timerDbg);

  if (pTimerMpsTtl && pTimerRegistr && pTimerConsole)
    return COMMON_STATUS_OK;
  else
    return COMMON_STATUS_ERROR;
}

/**
 * \brief check the source MAC and IP addresses of Endpoint
 *
 * Check the source MAC and IP addresses of the Endpoint WB device
 *
 * \ret status
 **/
static void printSrcAddr()
{
  uint32_t octet0 = 0x000000ff;
  uint32_t octet1 = octet0 << 8;
  uint32_t octet2 = octet0 << 16;
  uint32_t octet3 = octet0 << 24;

  DBPRINT1("fbas%d: MAC=%02lx:%02lx:%02lx:%02lx:%02lx:%02lx, IP=%lu.%lu.%lu.%lu\n", nodeType,
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

static status_t convertMacToU8(uint8_t buf[ETH_ALEN], uint32_t* hi, uint32_t* lo)
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

static status_t convertMacToU64(uint64_t* buf, uint32_t* hi, uint32_t* lo)
{
  if (!(hi && lo && buf)) {
    DBPRINT1("null pointer: %p %p %p\n", buf, hi, lo);
    return COMMON_STATUS_ERROR;
  }

  *buf = *hi & 0x0000ffff;
  *buf = *buf << 32;
  *buf += *lo;

  return COMMON_STATUS_OK;
}

/**
 * \brief Set the destination MAC and IP addresses of Endpoint
 *
 * Set the destination MAC and IP addresses of the Endpoint WB device.
 *
 * \param idx Address index (0=actual, 1=RX node, 2=broadcast etc)
 *
 * \ret status
 *
 **/
static status_t setEndpDstAddr(int idx)
{
  uint32_t status = COMMON_STATUS_OK;

  // check index
  if ((idx < 0) || idx >= N_DST_ADDR)  // invalid or out of range
    return COMMON_STATUS_ERROR;

  // check the desired address is already set (do not consider IP address)
  if ((dstNwAddr[DST_ADDR_EBM].mac == dstNwAddr[idx].mac))
    return status;

  // set the destination addresses
  fwlib_setEbmDstAddr(dstNwAddr[idx].mac, dstNwAddr[idx].ip);

  // update the destination address
  dstNwAddr[DST_ADDR_EBM].mac = dstNwAddr[idx].mac;

  return status;
}

/**
 * \brief Read a node ID
 *
 * RX node accepts the timing messages only from valid sender nodes.
 * The node ID is normally same as its MAC address.
 *
 * This function allows RX node to read a node ID from the given location
 * in the shared memory. The number of sender nodes for each RX node is
 * limited to N_MAX_TX_NODES (eg., 16). Hence, node ID is prepended with
 * a 8-bit index (index < N_MAX_TX_NODES).
 *
 * Full node ID format in shared memory: index + reserved + ID (MAC)
 * where, index and reserved are each 8-bit value.
 *
 * \param base   Base address of shared memory
 * \param offset Offset to a location with a valid sender ID
 *
 * \return status   Zero on success, otherwise non-zero
 **/
static status_t readNodeId(uint32_t* base, uint32_t offset)
{
  uint64_t* pId = (uint64_t *)(base + (offset >> 2)); // point to a node ID
  uint8_t idx = *pId >> 56;                           // index of the node ID

  if (!(idx < N_MAX_TX_NODES)) {
    DBPRINT1("fbas%d: index %d in %llx is out of range!\n", nodeType, idx, *pId);
    return COMMON_STATUS_ERROR;
  }

  // update the MPS message buffer
  msgUpdateMpsBuf(pId);

  // app-specific IO setup (RX: enable IO output for signaling latency)
  // set up the direct mapping between the MPS message buffer and output ports
  if (ioSetOutEnable(idx, true) == COMMON_STATUS_OK)  // enable output
    ioMapOutput(idx, idx);  // direct mapping (MPS buffer[idx] -> out_port[idx])

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
static void clearError(size_t len, mpsMsg_t* buf) {

  for (size_t i = 0; i < len; ++i) {
    ioDriveOutput(buf + i, i);
  }
}

/**
 * \brief set operation mode
 *
 * \param mode raw data with operation mode
 *
 **/
static void setOpMode(uint64_t mode) {
  if (mode)
    opMode = FBAS_OPMODE_TEST;
  else
    opMode = FBAS_OPMODE_DEF;
}

/**
 * \brief Handle pending ECA event
 *
 * On FBAS_GEN_EVT event the buffer for MPS flag is updated and \head returns
 * pointer to it. Otherwise, \head is returned with null value.
 *
 * On FBAS_WR_EVT or FBAS_WR_FLG event the effective logic output is driven.
 *
 * \param usTimeout Maximum interval in microseconds to poll ECA
 * \param mpsTask   Pointer to MPS-relevant task flag
 * \param pMsgCtrl  Pointer to the MPS messaging controller
 * \param head      Pointer to pointer to the head of the MPS message buffer
 *
 * \return ECA action tag (COMMON_ECADO_TIMEOUT on timeout, otherwise non-zero tag)
 **/
static uint32_t handleEcaEvent(uint32_t usTimeout, uint32_t* mpsTask, msgCtrl_t* pMsgCtrl, mpsMsg_t** head)
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
  uint64_t ts;            // temporary timestamp
  uint32_t actions;

  nextAction = fwlib_wait4ECAEvent(usTimeout, &ecaDeadline, &ecaEvtId, &ecaParam, &ecaTef,
    &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);

  if (nextAction != COMMON_ECADO_TIMEOUT) {
    now = getSysTime();

    uint64_t nodeId;  // node ID (MAC address) is in the 'param' field (high 6 bytes)
    uint8_t  regCmd;  // node registration command
    uint8_t  info;    // additional info (# of MPS channels)
    int      offset;  // offset to the MPS msg buffer, where received MPS message will be stored

    switch (nextAction) {
      case FBAS_AUX_NEWCYCLE:
        if (nodeType == FBAS_NODE_TX) { // it takes 1848/6328 ns for 2/32 MPS channels
          // force the CMOS input virtually to high voltage [MPS_FS_620]
          msgForceHigh(*head);

          // init the MPS messaging controller so that next messaging is delayed for 52 ms [MPS_FS_630]
          now += TIM_52_MS;
          msgInitMsgCtrl(pMsgCtrl, N_MPS_CHANNELS, now, F_MPS_BCAST);

        } else if (nodeType == FBAS_NODE_RX) { // it takes 2480/31048 ns for 2/32 MPS channels
          // force effective logic input to HIGH bit (delay for 52 ms) [MPS_FS_630]
          msgForceHigh(*head);
          // clear latched errors [MPS_FS_600]
          clearError(N_MAX_MPS_CHANNELS, *head);
        }
        ts = getSysTime();
        DBPRINT2("%lli\n", (ts - now));
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
          testOutput(N_MAX_MPS_CHANNELS, *head);
        }
        ts = getSysTime();
        DBPRINT2("%lli\n", (ts - now));
        break;

      case FBAS_GEN_EVT:
        if (nodeType == FBAS_NODE_TX) {// only FBAS TX node handles the MPS events
          // fetch the detected MPS event
          *head = msgFetchMps(myIdx, ecaEvtId, ecaDeadline);

          // forward the fetched MPS event
          if (*head && (*mpsTask & TSK_TX_MPS_EVENTS)) {
            // select the transmission type (broadcast: not registered or NOK flag, unicast: otherwise)
            uint32_t status;
            if (!(*mpsTask & TSK_REG_COMPLETE) || ((*head)->prot.flag == MPS_FLAG_NOK))
              status = setEndpDstAddr(DST_ADDR_BROADCAST);
            else
              status = setEndpDstAddr(DST_ADDR_RXNODE);

            if (status != COMMON_STATUS_OK) {
              DBPRINT1("Err - nothing sent! TODO: set failed status\n");
              break;
            }

            // send MPS event
            uint32_t count = msgSignalMpsEvent(pMsgCtrl, *head, FBAS_FLG_EID, N_EXTRA_MPS_NOK);
            // count sent timing messages with MPS event
            *(pSharedApp + (FBAS_SHARED_GET_CNT >> 2)) = measureCountEvt(TX_EVT_CNT, count);

            // measure ECA handling delay
            measureSummarize(MSR_ECA_HANDLE, ecaDeadline, now, DISABLE_VERBOSITY);
            measureExportSummary(MSR_ECA_HANDLE, pSharedApp, FBAS_SHARED_ECA_HNDL_AVG);

            // store timestamps to measure delays
            measurePutTimestamp(MSR_TX_DLY, now);
            measurePutTimestamp(MSR_SG_LTY, ecaDeadline);
          }
        }
        break;
      case FBAS_TLU_EVT:
        /* FBAS TX node handles the TLU events.
        TLU events (configured externally by saft-ctl-io) are used to detect the feedback
        by the RX node on reception of the timing messages with the MPS flag/event. */
        if (nodeType == FBAS_NODE_TX && *mpsTask & TSK_TX_MPS_EVENTS) {
          // measure transmission delay (from timing message transmission at TX to timing message reception at RX node)
          ts = measureGetTimestamp(MSR_TX_DLY);
          measureSummarize(MSR_TX_DLY, ts, ecaDeadline, DISABLE_VERBOSITY);
          measureExportSummary(MSR_TX_DLY, pSharedApp, FBAS_SHARED_TX_DLY_AVG);

          /* signaling latency
          Time period measured with the ECA timestamps between MPS event generation and
          associated feedback IO event at a TX node. */
          ts = measureGetTimestamp(MSR_SG_LTY);
          measureSummarize(MSR_SG_LTY, ts, ecaDeadline, DISABLE_VERBOSITY);
          measureExportSummary(MSR_SG_LTY, pSharedApp, FBAS_SHARED_SG_LTY_AVG);
        }
        break;
      case FBAS_WR_EVT:
      case FBAS_WR_FLG:
        if (nodeType == FBAS_NODE_RX) { // FBAS RX generates MPS class 2 signals
          // store and handle received MPS flag
          offset = msgStoreMpsMsg(&ecaParam, &ecaDeadline, pMsgCtrl);
          if (offset >= 0) {
            // new MPS msg
            if (offset < N_MAX_MPS_CHANNELS) {
              // drive the assigned output port
              if (ioDriveOutput((mpsMsg_t*)(*head + offset), offset) == COMMON_STATUS_OK) {
                // measure the ECA handling delay
                measureSummarize(MSR_ECA_HANDLE, ecaDeadline, now, DISABLE_VERBOSITY);
                measureExportSummary(MSR_ECA_HANDLE, pSharedApp, FBAS_SHARED_MSG_DLY_AVG);
              }

              // measure the average messaging delay
              measureSummarize(MSR_MSG_DLY, ecaDeadline, now, DISABLE_VERBOSITY);
              measureExportSummary(MSR_MSG_DLY, pSharedApp, FBAS_SHARED_MSG_DLY_AVG);
            }
          }

          // count received timing messages with MPS flag or MPS event
          actions=1; // do not use fwlib_getEcaValidCnt() to get the ECA channel valid count => it returns zero value
          *(pSharedApp + (FBAS_SHARED_ECA_VLD >> 2)) = measureCountEvt(ECA_VLD_ACT, actions);

          if (COMMON_STATUS_OK == fwlib_getEcaOverflowCnt(&actions)) // number of the overflow actions
            *(pSharedApp + (FBAS_SHARED_ECA_OVF >> 2)) = measureCountEvt(ECA_OVF_ACT, actions);
        }
        break;

      case FBAS_NODE_REG:
        // tie registration to MPS operation -> otherwise RX node registers TX nodes any time (in OpReady)
        if (!(*mpsTask & TSK_TX_MPS_EVENTS))
          break;

        nodeId = ecaParam >> 16; // node ID (MAC address) is in the 'param' field (high 6 bytes)
        regCmd = (uint8_t)(ecaParam >> 8);  // node registration command
        info   = ecaParam;       // info: number of the MPS channels managed by TX node

        if (nodeType == FBAS_NODE_RX) {  // registration request from TX
          if (regCmd == REG_REQ) {
            // find the given sender node in the dedicated array and return the index
            int8_t idx = msgGetNodeIndex(&nodeId);
            if ((0 <= idx) && (idx < N_MAX_TX_NODES)) {
              // TODO: reserve the MPS buffer for this sender node
              // TODO: idx=reserveBuffer(pos, nodeId);
              // unicast the reg. response
              fwlib_setEbmDstAddr(nodeId, BROADCAST_IP);
              msgRegisterNode(myMac, REG_RSP, idx);
              DBPRINT1("reg OK: MAC=%llx\n", nodeId);
            }
          }
        }
        else if (nodeType == FBAS_NODE_TX) { // registration response from RX
          if (regCmd == REG_RSP) {
            dstNwAddr[DST_ADDR_RXNODE].mac = nodeId;
            myIdx = info;
            DBPRINT3("reg.rsp: RX MAC=%llx\n", dstNwAddr[DST_ADDR_RXNODE].mac);
            *mpsTask |= TSK_REG_COMPLETE;
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
 * \brief Output a debug message to WR console
 *
 * Print timer period statistics and call period.
 **/
static void wrConsolePeriodic(void)
{
  static uint64_t lastSysTime = 0;

  uint64_t now = getSysTime();

  DBPRINT1("timer avg: %lli min: %lli max: %lli, call %lli\n",
            timerDbg.period.avg, timerDbg.period.min, timerDbg.period.max, (now - lastSysTime));
  lastSysTime = now;
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

  DBPRINT2("fbas%d: pIOCtrl=0x%8p, pECAQ=0x%8p\n", nodeType, pIOCtrl, pECAQ);

  DBPRINT1("fbas%d: designated platform = %s\n", nodeType, MYPLATFORM);
  if (strcmp(MYPLATFORM, "pcicontrol") == 0) {   // re-set the default output port ( LVDS for Pexiara)
    outPortCfg.type = IO_CFG_CHANNEL_LVDS;
    outPortCfg.total= N_OUT_LEMO_PEXARIA;
  }

  // app specific IO setup (TX: B2 as input, all output disabled)
  fwlib_ioCtrlSetGate(0, 2);        // disable input gate

  ioInitPortMap();                  // init IO output port map
  for (uint8_t idx = 0; idx < outPortCfg.total; ++idx)
    ioSetOutEnable(idx, false);     // disable all output ports

  fwlib_publishNICData();           // NIC data (MAC, IP) are assigned to global variables (pSharedIp, pSharedMacHi/Lo)
  printSrcAddr();                   // output the source MAC/IP address of the Endpoint WB device to the WR console

  dstNwAddr[DST_ADDR_BROADCAST].mac = BROADCAST_MAC;
  dstNwAddr[DST_ADDR_BROADCAST].ip  = BROADCAST_IP;
  status = setEndpDstAddr(DST_ADDR_BROADCAST); // set the destination broadcast MAC/IP address of the Endpoint WB device
  if (status != COMMON_STATUS_OK) return status;

  if ((uint32_t)pCpuWbTimer != ERROR_NOT_FOUND) {
    initTimers();           // set up SW Timers
    timerSetupHw(TIM_1_MS); // set up the HW timer
    initIrqTable();         // enable IRQ of HW timer
    timerEnableHw();        // enable the HW timer
  }

  initMpsData();                    // initialize application specific data structure

  return status;
}

// entry action state 'op ready'
uint32_t extern_entryActionOperation()
{
  uint32_t status = COMMON_STATUS_OK;

  // flush ECA queue for eCPU
  uint64_t t64;
  uint32_t t32;
  int i = 0;
  while (fwlib_wait4ECAEvent(1000, &t64, &t64, &t64, &t32, &t32, &t32, &t32, &t32) != COMMON_ECADO_TIMEOUT)
    {i++;}
  DBPRINT1("ECA eCPU queue flushed - cleared %d pending actions\n", i);

  // initiate node registry
  if (nodeType == FBAS_NODE_TX) {
    if (!(mpsTask & TSK_REG_COMPLETE)) {
      if (setEndpDstAddr(DST_ADDR_BROADCAST) == COMMON_STATUS_OK)
        msgRegisterNode(myMac, REG_REQ, N_MPS_CHANNELS);
      else
        DBPRINT1("Err - nothing sent! TODO: set failed status\n");
    }
  }

  return status;
}

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
static void cmdHandler(uint32_t *reqState, uint32_t cmd)
{
  uint32_t u32val;
  uint8_t u8val;
  io_port_t outPort;
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
          DBPRINT2("fbas%d: invalid node type %lx\n", nodeType, u32val);
        }
        break;
      case FBAS_CMD_GET_SENDERID:
        // read sender node ID (MAC, idx) from the shared memory and
        // assign output port for signaling latency measurement
        readNodeId(pSharedApp, FBAS_SHARED_SENDERID);
        break;
      case FBAS_CMD_SET_IO_OE:
        u8val = 0;   // index = 0, by default
        ioSetOutEnable(u8val, true);  // enable output of the chosen IO port
        break;
      case FBAS_CMD_GET_IO_OE:
        u8val  = 0;  // index = 0, by default
        if (ioIsOutEnabled(u8val, &u32val) == COMMON_STATUS_OK)
          DBPRINT2("fbas%d: OE: idx %x, val %lx\n", nodeType, u8val, u32val);
        else
          DBPRINT2("fbas%d: OE read failed: idx %x\n", nodeType, u8val);
        break;
      case FBAS_CMD_TOGGLE_IO:
        u8val = cntCmd & 0x01;
        outPort.type = outPortCfg.type;
        outPort.idx  = 0;
        driveOutPort(&outPort, u8val);
        DBPRINT2("fbas%d: IO%lu=%x\n", nodeType, u32val+1, u8val);
        break;
      case FBAS_CMD_EN_MPS_FWD:
        mpsTask |= TSK_TX_MPS_FLAGS;  // enable transmission of the MPS flags
        mpsTask |= TSK_TX_MPS_EVENTS; // enable transmission of the MPS events
        mpsTask |= TSK_MONIT_MPS_TTL; // enable lifetime monitoring of the MPS flags
        // clear ECA queue by polling it => done in extern_entryActionOperation()
        timerStart(pTimerMpsTtl);     // start timers
        timerStart(pTimerRegistr);
        DBPRINT2("fbas%d: enabled MPS %lx\n", nodeType, mpsTask);
        break;
      case FBAS_CMD_DIS_MPS_FWD:
        mpsTask &= ~TSK_TX_MPS_FLAGS;  // disable transmission of the MPS flags
        mpsTask &= ~TSK_TX_MPS_EVENTS; // disable transmission of the MPS events
        mpsTask &= ~TSK_MONIT_MPS_TTL; // disable lifetime monitoring of the MPS flags
        mpsTask &= ~TSK_REG_COMPLETE;  // reset the node registration
        timerStart(pTimerConsole);     // start timer
        DBPRINT2("fbas%d: disabled MPS %lx\n", nodeType, mpsTask);
        break;
      case FBAS_CMD_PRINT_NW_DLY:
        measurePrintSummary(MSR_TX_DLY);
        measureExportSummary(MSR_TX_DLY, pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_PRINT_SG_LTY:
        measurePrintSummary(MSR_SG_LTY);
        measureExportSummary(MSR_SG_LTY, pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_PRINT_MSG_DLY:
        measurePrintSummary(MSR_MSG_DLY);
        measureExportSummary(MSR_MSG_DLY, pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_PRINT_TTL:
        measurePrintSummary(MSR_TTL);
        measureExportSummary(MSR_TTL, pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_PRINT_ECA_HANDLE:
        measurePrintSummary(MSR_ECA_HANDLE);
        measureExportSummary(MSR_ECA_HANDLE, pSharedApp, FBAS_SHARED_GET_AVG);
        break;
      case FBAS_CMD_CLR_SUM_STATS:
        measureClearSummary(ENABLE_VERBOSITY);
        break;

      case FBAS_CMD_PRINT_MPS_BUF:
        ioPrintMpsBuf();
        ioPrintPortMap();
        break;
      default:
        DBPRINT2("fbas%d: received unknown command '0x%08lx'\n", nodeType, cmd);
        break;
    } // switch
  } // if command
} // cmdHandler

/**
 * \brief Timer interrupt handler
 *
 * Callback routine for timer interrupt
 *
 * \param none
 *
 * \return none
 **/
static void timerHandler(void)
{
  // calculate (moving) average of the timer period
  uint64_t now = getCpuTime();
  timerUpdateDbg(&timerDbg, now);

  // tick all registered timers
  timerTickTimers();
}

// do action state 'op ready' - this is the main code of this FW
uint32_t doActionOperation(uint32_t* pMpsTask,          // MPS-relevant tasks
                           mpsMsg_t* pBufMpsMsg,        // pointer to MPS message buffer
                           msgCtrl_t* pMsgCtrl,         // pointer to the messaging controller
                           uint32_t actStatus)          // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t usTimeout = 0;                                     // time-out in microseconds
  uint32_t action;                                            // ECA action tag
  mpsMsg_t* buf = pBufMpsMsg;                                 // pointer to MPS message buffer
  uint64_t now, last;                                         // used to measure the period of the main loop

  status = actStatus;

  // action driven by ECA event, blocking poll up to usTimeout [us]
  action = handleEcaEvent(usTimeout, pMpsTask, pMsgCtrl, &buf);   // handle ECA event

  // check periodic timer events
  if (mpsTask & TSK_MONIT_MPS_TTL)
    if (timerIsExpired(pTimerMpsTtl)) {
      mpsTask |= TSK_EVAL_MPS_TTL;
      timerStart(pTimerMpsTtl);
    }

  if (timerIsExpired(pTimerRegistr)) {
    mpsTask |= TSK_REG_PER_OVER;
    timerStart(pTimerRegistr);
  }

  switch (nodeType) {

    case FBAS_NODE_TX:
      // transmit MPS flags (flags are sent in specified period, but events immediately)
      if (*pMpsTask & TSK_TX_MPS_FLAGS) {

        // registration incomplete
        if (!(*pMpsTask & TSK_REG_COMPLETE)) {
          // registration period is over?
          if (*pMpsTask & TSK_REG_PER_OVER) {
            *pMpsTask &= ~TSK_REG_PER_OVER;
            if (setEndpDstAddr(DST_ADDR_BROADCAST) == COMMON_STATUS_OK)
              msgRegisterNode(myMac, REG_REQ, N_MPS_CHANNELS);
            else
              DBPRINT1("Err - nothing sent! TODO: set failed status\n");
          }
          break;
        }

        // periodic messaging of the MPS flags (unicast transmission)
        if (setEndpDstAddr(DST_ADDR_RXNODE) == COMMON_STATUS_OK) {
          uint32_t count = msgSendMpsFlag(pMsgCtrl, FBAS_FLG_EID);
          // export the counter of sent timing messages
          *(pSharedApp + (FBAS_SHARED_GET_CNT >> 2)) = measureCountEvt(TX_EVT_CNT, count);
        }
        else {
          DBPRINT1("Err - nothing sent! TODO: set failed status\n");
        }
      }
      else if (timerIsExpired(pTimerConsole)) {
        wrConsolePeriodic();        // periodic output message to WR console
        timerStart(pTimerConsole);
      }
      break;

    case FBAS_NODE_RX:
      if (*pMpsTask & TSK_EVAL_MPS_TTL) {
        // evaluate lifetime of the MPS protocols and handle expired MPS protocols
        *pMpsTask &= ~TSK_EVAL_MPS_TTL;
        uint64_t now = getSysTime();
        for (int i = 0; i < N_MAX_MPS_CHANNELS; i++) {
          buf = evalMpsMsgTtl(now, i);
          if (buf) {
            ioDriveOutput(buf, i);
            if (!buf->ttl) {
              measureSummarize(MSR_TTL, buf->tsRx, now, DISABLE_VERBOSITY);
              measureExportSummary(MSR_TTL, pSharedApp, FBAS_SHARED_TTL_PRD_AVG);
            }
          }
        }
      }
      break;

    default:
      break;
  }

  // measure the period of the main loop
  now = getSysTime();
  last = measureGetTimestamp(MSR_MAIN_LOOP_PRD);
  if (last) {
    measureSummarize(MSR_MAIN_LOOP_PRD, last, now, DISABLE_VERBOSITY);
    measureExportSummary(MSR_MAIN_LOOP_PRD, pSharedApp, FBAS_SHARED_ML_PRD_AVG);
  }
  measurePutTimestamp(MSR_MAIN_LOOP_PRD, now);

  return status;
}

int main(void) {
  uint32_t status;                                            // (error) status
  uint32_t cmd;                                               // command via shared memory
  uint32_t actState;                                          // actual FSM state
  uint32_t pubState;                                          // value of published state
  uint32_t reqState;                                          // requested FSM state
  uint32_t *buildID;

  // init local variables
  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  buildID        = (uint32_t *)(INT_BASE_ADR + BUILDID_OFFS); // required for 'stack check'

  // init
  init();                                                              // initialize stuff for lm32
  fwlib_clearDiag();                                                   // clear common diagnostic data
  status = initSharedMem((uint32_t *)_startshared);                    // initialize the shared memory
  if (status == COMMON_STATUS_ERROR)
    return 1;

  while (1) {
    check_stack_fwid(buildID);                                         // check for stack corruption
    fwlib_cmdHandler(&reqState, &cmd);                                 // check for common commands and possibly request state changes
    cmdHandler(&reqState, cmd);                                        // check for project relevant commands
    status = COMMON_STATUS_OK;                                         // reset status for each iteration
    status = fwlib_changeState(&actState, &reqState, status);          // handle requested state changes
    switch(actState) {                                                 // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&mpsTask, bufMpsMsg, &mpsMsgCtrl, status);
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
