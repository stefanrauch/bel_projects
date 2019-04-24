/********************************************************************************************
 *  b2b-cbu.c
 *
 *  created : 2019
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 24-Apr-2019
 *
 *  firmware required to implement the CBU (Central Buncht-To-Bucket Unit)
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
 * Last update: 23-April-2019
 ********************************************************************************************/
// #define B2BTEST_FW_VERSION 0x000001                                     // make this consistent with makefile

/* standard includes */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include "mprintf.h"
#include "mini_sdb.h"

/* includes specific for bel_projects */
#include "dbg.h"
#include "../../../top/gsi_scu/scu_mil.h"                               // register layout of 'MIL macro' /* chk */

#include <b2b-common.h>                                                 // common stuff for b2b
#include <b2b-test.h>                                                   // defs
#include <b2bcbu_shared_mmap.h>                                         // autogenerated upon building firmware


// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

volatile uint32_t *pShared;             // pointer to begin of shared memory region
uint32_t *pSharedNTransfer;             // pointer to a "user defined" u32 register; here: # of transfers
volatile uint32_t *pSharedTH1ExtHi;     // pointer to a "user defined" u32 register; here: period of h=1 extraction, high bits
volatile uint32_t *pSharedTH1ExtLo;     // pointer to a "user defined" u32 register; here: period of h=1 extraction, low bits
volatile uint32_t *pSharedTH1InjHi;     // pointer to a "user defined" u32 register; here: period of h=1 injection, high bits
volatile uint32_t *pSharedTH1InjLo;     // pointer to a "user defined" u32 register; here: period of h=1 injecion, low bits

uint32_t *pCpuRamExternal;              // external address (seen from host bridge) of this CPU's RAM            
uint32_t *pCpuRamExternalData4EB;       // external address (seen from host bridge) of this CPU's RAM: field for EB return values

uint32_t sumStatus;                     // all status infos are ORed bit-wise into sum status, sum status is then published
uint32_t nBadStatus;                    // # of bad status (=error) incidents
uint32_t nBadState;                     // # of bad state (=FATAL, ERROR, UNKNOWN) incidents

uint64_t tH1;                           // rise edge of H = 1 signal


void init() // typical init for lm32
{
  discoverPeriphery();        // mini-sdb ...
  uart_init_hw();             // needed by WR console   
  cpuId = getCpuIdx();

  timer_init(1);              // needed by usleep_init() 
  usleep_init();              // needed by scu_mil.c

  // set MSI IRQ handler
  isr_table_clr();
  //irq_set_mask(0x01);
  irq_disable();
} // init


void initSharedMem() // determine address and clear shared mem
{
  uint32_t idx;
  uint32_t *pSharedTemp;
  int      i; 
  const uint32_t c_Max_Rams = 10;
  sdb_location found_sdb[c_Max_Rams];
  sdb_location found_clu;
  
  // get pointer to shared memory
  pShared           = (uint32_t *)_startshared;

  pSharedNTransfer        = (uint32_t *)(pShared + (B2BTEST_SHARED_NTRANSFER >> 2));
  pSharedTH1ExtHi         = (uint32_t *)(pShared + (B2BTEST_SHARED_TH1EXTHI >> 2));
  pSharedTH1ExtLo         = (uint32_t *)(pShared + (B2BTEST_SHARED_TH1EXTLO >> 2));
  pSharedTH1InjHi         = (uint32_t *)(pShared + (B2BTEST_SHARED_TH1INJHI >> 2));
  pSharedTH1InjLo         = (uint32_t *)(pShared + (B2BTEST_SHARED_TH1INJLO >> 2));
  
  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);	
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if(idx >= cpuId) {
    pCpuRamExternal           = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective
    pCpuRamExternalData4EB    = (uint32_t *)(pCpuRamExternal + ((COMMON_SHARED_DATA_4EB + SHARED_OFFS) >> 2));
  }

  DBPRINT2("b2b-test: CPU RAM External 0x%8x, begin shared 0x%08x\n", pCpuRamExternal, SHARED_OFFS);

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_BEGIN >> 2 ));
  while (pSharedTemp < (uint32_t *)(pShared + (B2BTEST_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("b2b-test: used size of shared mem is %d words (uint32_t), begin %x, end %x\n", i, pShared, pSharedTemp-1);
  } // initSharedMem 


uint32_t doActionOperation(uint64_t *tAct,                    // actual time
                           uint32_t actStatus)                // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t flagIsLate;                                        // flag indicating that we received a 'late' event from ECA
  uint32_t ecaAction;                                         // action triggered by event received from ECA
  uint64_t tDummy;                                            // dummy timestamp
  int      i,j;
  int      nInput;
  uint32_t tsHi;
  uint32_t tsLo;
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtID to send
  uint64_t sendParam;                                         // param to send
  uint64_t recDeadline;                                       // deadline received
  uint64_t recEvtId;                                          // evtID received
  uint64_t recParam;                                          // param received
    


  status = actStatus;

  ecaAction = common_wait4ECAEvent(COMMON_ECATIMEOUT, &recDeadline, &recParam, &flagIsLate);
    
  switch (ecaAction) {
  case B2BTEST_ECADO_B2B_START :
    // received: B2B_START from DM
    // send command: phase measurement at extraction machine
    sendEvtId    = 0x1fff000000000000;                                        // FID, GID
    sendEvtId    = sendEvtId | ((uint64_t)B2BTEST_ECADO_B2B_PMEXT << 36);     // EVTNO

    sendParam    = (uint64_t)(*pSharedTH1ExtHi) << 32 | (uint64_t)(*pSharedTH1ExtLo);

    sendDeadline = getSysTime() + (uint64_t)COMMON_AHEADT;

    common_ebmWriteTM(sendDeadline, sendEvtId, sendParam);
    mprintf("b2b-test: got B2B_START\n");

    break;
  case B2BTEST_ECADO_B2B_PREXT :
    // received: measured phase from extraction machine
    // do some math
    sendDeadline = recParam + (uint64_t)100000000; /* chk, hack: 1. fixed period 2. need PRINJ too */

    // send TR_EXT_INJ to extraction machine
    sendEvtId    = 0x1fff000000000000;                                        // FID, GID
    sendEvtId    = sendEvtId | ((uint64_t)B2BTEST_ECADO_B2B_SYNCEXT << 36);   // EVTNO
    
    sendParam    = 0x0;

    common_ebmWriteTM(sendDeadline, sendEvtId, sendParam);
    mprintf("b2b-test: got B2B_START\n");

    break;
    
  default :
    break;
  } // switch ecaActione

  status = actStatus; /* chk */
  
  return status;
} // doActionOperation


void main(void) {
 
  uint32_t j;
 
  uint64_t tPrevCycle;                          // time of previous UNILAC cycle
  uint64_t tActCycle;                           // time of actual UNILAC cycle
  uint32_t status;                              // (error) status
  uint32_t actState;                            // actual FSM state
  uint32_t pubState;                            // published state value
  uint32_t reqState;                            // requested FSM state
  uint32_t flagRecover;                         // flag indicating auto-recovery from error state;  

  mprintf("\n");
  mprintf("b2b-test: ***** firmware v %06d started from scratch *****\n", B2BTEST_FW_VERSION);
  mprintf("\n");
  
  // init local variables
  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  flagRecover    = 0;
  common_clearDiag();

  init();                                                                   // initialize stuff for lm32
  initSharedMem();                                                          // initialize shared memory
  common_init((uint32_t *)_startshared, B2BTEST_FW_VERSION);                // init common stuff

 /* hack init chk */
  *pSharedTH1ExtHi = 0x00002D79; // 20 Hz
  *pSharedTH1ExtLo = 0x883D2000;
  
  while (1) {
    common_cmdHandler(&reqState);                                           // check for commands and possibly request state changes
    status = COMMON_STATUS_OK;                                              // reset status for each iteration
    status = common_changeState(&actState, &reqState, status);              // handle requested state changes
    switch(actState)                                                        // state specific do actions
      {
      case COMMON_STATE_S0 :
        status = common_doActionS0();                                       // important initialization that must succeed!
        if (status != COMMON_STATUS_OK) reqState = COMMON_STATE_FATAL;      // failed:  -> FATAL
        else                            reqState = COMMON_STATE_IDLE;       // success: -> IDLE
        break;
      case COMMON_STATE_OPREADY :
        flagRecover = 0;
        status = doActionOperation(&tActCycle, status);
        if (status == COMMON_STATUS_WRBADSYNC)      reqState = COMMON_STATE_ERROR;
        if (status == COMMON_STATUS_ERROR)          reqState = COMMON_STATE_ERROR;
        break;
      case COMMON_STATE_ERROR :
        flagRecover = 1;                                                    // start autorecovery
        break; 
      case COMMON_STATE_FATAL :
        pubState = actState;
        common_publishState(pubState);
        common_publishSumStatus(sumStatus);
        mprintf("b2b-test: a FATAL error has occured. Good bye.\n");
        while (1) asm("nop"); // RIP!
        break;
      default :                                                             // avoid flooding WB bus with unnecessary activity
        for (j = 0; j < (COMMON_DEFAULT_TIMEOUT * COMMON_MS_ASMNOP); j++) { asm("nop"); }
      } // switch

    // autorecovery from state ERROR
    if (flagRecover) common_doAutoRecovery(actState, &reqState);

    // update shared memory
    // ... 
        
    switch (status) {
    case COMMON_STATUS_OK :                                                   // status OK
      sumStatus = sumStatus |  (0x1 << COMMON_STATUS_OK);                     // set OK bit
      break;
    default :                                                                 // status not OK
      if ((sumStatus >> COMMON_STATUS_OK) & 0x1) nBadStatus++;                // changing status from OK to 'not OK': increase 'bad status count'
      sumStatus = sumStatus & ~(0x1 << COMMON_STATUS_OK);                     // clear OK bit
      sumStatus = sumStatus |  (0x1 << status);                               // set status bit and remember other bits set
      break;
    } // switch status

    if ((pubState == COMMON_STATE_OPREADY) && (actState  != COMMON_STATE_OPREADY)) nBadState++;
    common_publishSumStatus(sumStatus);
    pubState = actState;
    common_publishState(pubState);
    common_publishNBadStatus(nBadStatus);
    common_publishNBadState(nBadState);
  } // while  
} // main
