/********************************************************************************************
 *  b2b-pm.c
 *
 *  created : 2019
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 20-May-2019
 *
 *  firmware required for measuring the h=1 phase for ring machine
 *  
 *  - when receiving B2BTEST_ECADO_PHASEMEAS, the phase is measured as a timestamp for an 
 *    arbitraty period
 *  - the phase timestamp is then sent as a timing message to the network
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
 * Last update: 15-April-2019
 ********************************************************************************************/
#define B2BPM_FW_VERSION 0x000002                                       // make this consistent with makefile

/* standard includes */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

/* includes specific for bel_projects */
#include "dbg.h"                                                        // debug outputs
#include "ebm.h"                                                        // EB master
#include "mprintf.h"                                                    // print to console
#include "mini_sdb.h"                                                   // sdb stuff
#include "syscon.h"                                                     // usleep et al
#include "aux.h"                                                        // cpu and IRQ

/* includes for this project */
#include <b2b-common.h>                                                 // common stuff for b2b
#include <b2b-test.h>                                                   // defs
#include <b2bpm_shared_mmap.h>                                          // autogenerated upon building firmware

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

// global variables 

volatile uint32_t *pShared;             // pointer to begin of shared memory region
uint32_t *pSharedNTransfer;             // pointer to a "user defined" u32 register; here: # of transfers

uint32_t *pCpuRamExternal;              // external address (seen from host bridge) of this CPU's RAM            
uint32_t *pCpuRamExternalData4EB;       // external address (seen from host bridge) of this CPU's RAM: field for EB return values
uint32_t sumStatus;                     // all status infos are ORed bit-wise into sum status, sum status is then published


void init() // typical init for lm32
{
  discoverPeriphery();        // mini-sdb ...
  uart_init_hw();             // needed by WR console   
  cpuId = getCpuIdx();

  timer_init(1);              // needed by usleep_init() 
  usleep_init();              // needed for usleep ...
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

  // get address to data
  pSharedNTransfer        = (uint32_t *)(pShared + (B2BTEST_SHARED_NTRANSFER >> 2));
  
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


// clear project specific diagnostics
void extern_clearDiag()
{
} // extern_clearDiag
  

uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // configure EB master (SRC and DST MAC/IP are set from host)
  if ((status = common_ebmInit(2000, 0xffffffffffff, 0xffffffff, EBM_NOREPLY)) != COMMON_STATUS_OK) {
    DBPRINT1("b2b-test: ERROR - init of EB master failed! %u\n", (unsigned int)status);
    return status;
  } 

  // get and publish NIC data
  common_publishNICData();

  return status;
} // extern_entryActionConfigured


uint32_t extern_entryActionOperation()
{
  int      i;
  uint64_t tDummy;
  uint64_t pDummy;
  uint32_t flagDummy;

  // clear diagnostics
  common_clearDiag();             

  // flush ECA queue for lm32
  i = 0;
  while (common_wait4ECAEvent(1, &tDummy, &pDummy, &flagDummy) !=  COMMON_ECADO_TIMEOUT) {i++;}
  DBPRINT1("b2b-test: ECA queue flushed - removed %d pending entries from ECA queue\n", i);

  return COMMON_STATUS_OK;
} // extern_entryActionOperation


uint32_t extern_exitActionOperation()
{
  return COMMON_STATUS_OK;
} // extern_exitActionOperation


uint32_t doActionOperation(uint64_t *tAct,                    // actual time
                           uint32_t actStatus)                // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t flagIsLate;                                        // flag indicating that we received a 'late' event from ECA
  uint32_t ecaAction;                                         // action triggered by event received from ECA
  uint64_t recDeadline;                                       // deadline received
  uint64_t recParam;                                          // param received
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtid to send
  uint64_t sendParam;                                         // parameter to send
  
  int      nInput;
  uint32_t tsHi;
  uint32_t tsLo;


  status = actStatus;

  ecaAction = common_wait4ECAEvent(COMMON_ECATIMEOUT, &recDeadline, &recParam, &flagIsLate);
  
  switch (ecaAction) {
  case B2BTEST_ECADO_B2B_PMEXT :
    tsHi = (uint32_t)((recDeadline >> 32) & 0xffffffff);
    tsLo = (uint32_t)(recDeadline & 0xffffffff);
    //mprintf("b2b-test: action phase, action %d, ts %u %u\n", ecaAction, tsHi, tsLo);
    
    nInput = 0;
    common_ioCtrlSetGate(1, 2);                                      // enable input gate
    while (nInput < 2) {                                      // treat 1st TS as junk
      /*
        todo chk: enable/disable input gate, take a couple of timestamp and fit the phase
        using the 'period' value contained in the recParam field 
      */
      ecaAction = common_wait4ECAEvent(100, &recDeadline, &recParam, &flagIsLate);
      tsHi = (uint32_t)((recDeadline >> 32) & 0xffffffff);
      tsLo = (uint32_t)(recDeadline & 0xffffffff);
      // mprintf("b2b-test: action TLU,   action %d, ts %u %u\n", ecaAction, tsHi, tsLo);
      if (ecaAction == B2BTEST_ECADO_TLUINPUT)   nInput++;
      if (ecaAction == B2BTEST_ECADO_TIMEOUT)    break; 
    } // while nInput
    // mprintf("b2b-test: action phase, nInput %d\n", nInput);
    common_ioCtrlSetGate(0, 2);                                      // disable input gate 
    
    if (nInput == 2) {
      // send command: transmit measured phase value
      sendEvtId    = 0x1fff000000000000;                                        // FID, GID
      sendEvtId    = sendEvtId | ((uint64_t)B2BTEST_ECADO_B2B_PREXT << 36);     // EVTNO
      sendParam    = recDeadline;
      sendDeadline = getSysTime() + COMMON_AHEADT;
      
      common_ebmWriteTM(sendDeadline, sendEvtId, sendParam);
      
    } // if nInput
    else actStatus = B2BTEST_STATUS_PHASEFAILED;

    break;
    
  default :
    break;
  } // switch ecaActione

  status = actStatus; /* chk */
  
  return status;
} // doActionOperation


int main(void) {
  uint64_t tActCycle;                           // time of actual UNILAC cycle
  uint32_t status;                              // (error) status
  uint32_t actState;                            // actual FSM state
  uint32_t pubState;                            // value of published state
  uint32_t reqState;                            // requested FSM state
  uint32_t flagRecover;                         // flag indicating auto-recovery from error state;
  uint32_t dummy1;                              // dummy parameter
  uint32_t j;
 
  // init local variables
  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  flagRecover    = 0;
  common_clearDiag();

  init();                                                                   // initialize stuff for lm32
  initSharedMem();                                                          // initialize shared memory
  common_init((uint32_t *)_startshared, B2BPM_FW_VERSION);                  // init common stuff
  
  while (1) {
    common_cmdHandler(&reqState, &dummy1);                                  // check for commands and possibly request state changes
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

    switch (status) {
    case COMMON_STATUS_OK :                                                   // status OK
      sumStatus = sumStatus |  (0x1 << COMMON_STATUS_OK);                     // set OK bit
      break;
    default :                                                                 // status not OK
      if ((sumStatus >> COMMON_STATUS_OK) & 0x1) common_incBadStatusCnt();    // changing status from OK to 'not OK': increase 'bad status count'
      sumStatus = sumStatus & ~(0x1 << COMMON_STATUS_OK);                     // clear OK bit
      sumStatus = sumStatus |  (0x1 << status);                               // set status bit and remember other bits set
      break;
    } // switch status

    if ((pubState == COMMON_STATE_OPREADY) && (actState  != COMMON_STATE_OPREADY)) common_incBadStateCnt();
    common_publishSumStatus(sumStatus);
    pubState = actState;
    common_publishState(pubState);
  } // while
  return(1); // this should never happen ...
} // main
