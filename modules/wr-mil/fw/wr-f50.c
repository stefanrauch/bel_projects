/********************************************************************************************
 *  wr-f50.c
 *
 *  created : 2024
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 2024-may-14
 *
 *  firmware required for the 50 Hz mains -> WR gateway
 *  
 *  This firmware tries to lock the Injector Data Master to the 50 Hz mains frequency
 *  * receives a trigger signal from the 50 Hz mains,
 *  * receives a 50 Hz 'cycle start' signal via the WR network,
 *  * compares both signals,
 *  * calculates the the new set-value of the period (~ 20ms) for the Data Master
 *  * broadcasts this information to the timing network
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
#define WRF50_FW_VERSION      0x000001    // make this consistent with makefile


// standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// includes specific for bel_projects
#include "dbg.h"                                                        // debug outputs
#include <stack.h>                                                      // stack check
#include "pp-printf.h"                                                  // print
#include "mini_sdb.h"                                                   // sdb stuff
#include "aux.h"                                                        // cpu and IRQ
#include "uart.h"                                                       // WR console

// includes for this project 
#include <common-defs.h>                                                // common defs for firmware
#include <common-fwlib.h>                                               // common routines for firmware
#include <wr-f50.h>                                                     // specific defs for wr-f50
#include <wrmil_shared_mmap.h>                                          // autogenerated upon building firmware

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

// global variables 
volatile uint32_t *pShared;                // pointer to begin of shared memory region
volatile uint32_t *pSharedSetF50Offset;    // pointer to a "user defined" u32 register; here: offset to TLU signal         
volatile uint32_t *pSharedSetMode;         // pointer to a "user defined" u32 register; here: mode of 50 Hz synchronization
volatile uint32_t *pSharedGetTMainsAct;    // pointer to a "user defined" u32 register; here: period of mains cycle [ns], actual value                           
volatile uint32_t *pSharedGetTDMAct;       // pointer to a "user defined" u32 register; here: period of Data Master cycle [ns], actual value                     
volatile uint32_t *pSharedGetTDMSet;       // pointer to a "user defined" u32 register; here: period of Data Master cycle [ns], actual value                     
volatile uint32_t *pSharedGetOffsDMAct;    // pointer to a "user defined" u32 register; here: offset of cycle start: t_DM - t_mains; actual value                
volatile uint32_t *pSharedGetOffsDMMin;    // pointer to a "user defined" u32 register; here: offset of cycle start: t_DM - t_mains; min value                   
volatile uint32_t *pSharedGetOffsDMMax;    // pointer to a "user defined" u32 register; here: offset of cycle start: t_DM - t_mains; max value                   
volatile uint32_t *pSharedGetOffsMainsAct; // pointer to a "user defined" u32 register; here: offset of cycle start: t_mains_predict - t_mains; actual value     
volatile uint32_t *pSharedGetOffsMainsMin; // pointer to a "user defined" u32 register; here: offset of cycle start: t_mains_predict - t_mains; min value        
volatile uint32_t *pSharedGetOffsMainsMax; // pointer to a "user defined" u32 register; here: offset of cycle start: t_mains_predict - t_mains; max value        
volatile uint32_t *pSharedGetLockState;    // pointer to a "user defined" u32 register; here: lock state; how DM is locked to mains                              
volatile uint32_t *pSharedGetLockDateHi;   // pointer to a "user defined" u32 register; here: time when lock has been achieve [ns], high bits                    
volatile uint32_t *pSharedGetLockDateLo;   // pointer to a "user defined" u32 register; here: time when lock has been achieve [ns], low bits                     
volatile uint32_t *pSharedGetNLocked;      // pointer to a "user defined" u32 register; here: counts how many locks have been achieved                           
volatile uint32_t *pSharedGetNCycles;      // pointer to a "user defined" u32 register; here: number of UNILAC cycles                                            
volatile uint32_t *pSharedGetNEvtsLate;    // pointer to a "user defined" u32 register; here: number of translated events that could not be delivered in time    
volatile uint32_t *pSharedGetComLatency;   // pointer to a "user defined" u32 register; here: latency for messages received from via ECA (tDeadline - tNow)) [ns]

uint32_t *cpuRamExternal;                  // external address (seen from host bridge) of this CPU's RAM

// set and get values
uint32_t setF50Offset;  
uint32_t setMode;        
uint32_t getTMainsAct;   
uint32_t getTDMAct;      
uint32_t getTDMSet;      
uint32_t getOffsDMAct;   
uint32_t getOffsDMMin;   
uint32_t getOffsDMMax;   
uint32_t getOffsMainsAct;
uint32_t getOffsMainsMin;
uint32_t getOffsMainsMax;
uint32_t getLockState;   
uint64_t getLockDate;  
uint32_t getNLocked;     
uint32_t getNCycles;     
uint32_t getNEvtsLate;   
uint32_t getComLatency;  

uint64_t statusArray;                      // all status infos are ORed bit-wise into statusArray, statusArray is then published
uint32_t nEvtsLate;                        // # of late messages
int32_t  comLatency;                       // latency for messages received via ECA

// constants (as variables to have a defined type)
uint64_t  one_us_ns = 1000;

// debug 
uint64_t t1, t2;
int32_t  tmp1;

// important local variables
uint64_t f50Stamps[WRF50_N_STAMPS];        // previous timestamps of 50 Hz signal   (most recent is n = WR50_N_STAMPS)
uint64_t dmStamps[WRF50_N_STAMPS];         // previous timestamps of DM cycle start (most recent is n = WR50_N_STAMPS)
int      f50Valid;                         // mains timestamps are valid
int      dmValid;                          // Data Master timestamps are valid

void init() // typical init for lm32
{
  discoverPeriphery();        // mini-sdb ...
  uart_init_hw();             // needed by WR console   
  cpuId = getCpuIdx();
} // init


// determine address and clear shared mem
void initSharedMem(uint32_t *reqState, uint32_t *sharedSize)
{
  uint32_t idx;
  uint32_t *pSharedTemp;
  int      i; 
  const uint32_t c_Max_Rams = 10;
  sdb_location   found_sdb[c_Max_Rams];
  sdb_location   found_clu;
  
  // get pointer to shared memory
  pShared                 = (uint32_t *)_startshared;

  // get address to data
  pSharedSetF50Offset     = (uint32_t *)(pShared + (WRF50_SHARED_SET_F50OFFSET       >> 2));
  pSharedSetMode          = (uint32_t *)(pShared + (WRF50_SHARED_SET_MODE            >> 2));
  pSharedGetTMainsAct     = (uint32_t *)(pShared + (WRF50_SHARED_GET_T_MAINS_ACT     >> 2));
  pSharedGetTDMAct        = (uint32_t *)(pShared + (WRF50_SHARED_GET_T_DM_ACT        >> 2));
  pSharedGetTDMSet        = (uint32_t *)(pShared + (WRF50_SHARED_GET_T_DM_SET        >> 2));
  pSharedGetOffsDMAct     = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_DM_ACT     >> 2));
  pSharedGetOffsDMMin     = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_DM_MIN     >> 2));
  pSharedGetOffsDMMax     = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_DM_MAX     >> 2));
  pSharedGetOffsMainsAct  = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_MAINS_ACT  >> 2));
  pSharedGetOffsMainsMin  = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_MAINS_MIN  >> 2));
  pSharedGetOffsMainsMax  = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_MAINS_MAX  >> 2));
  pSharedGetLockState     = (uint32_t *)(pShared + (WRF50_SHARED_GET_LOCK_STATE      >> 2));
  pSharedGetLockDateHi    = (uint32_t *)(pShared + (WRF50_SHARED_GET_LOCK_DATE_HIGH  >> 2));
  pSharedGetLockDateLo    = (uint32_t *)(pShared + (WRF50_SHARED_GET_LOCK_DATE_LOW   >> 2));
  pSharedGetNLocked       = (uint32_t *)(pShared + (WRF50_SHARED_GET_N_LOCKED        >> 2));
  pSharedGetNCycles       = (uint32_t *)(pShared + (WRF50_SHARED_GET_N_CYCLES        >> 2));
  pSharedGetNEvtsLate     = (uint32_t *)(pShared + (WRF50_SHARED_GET_N_EVTS_LATE     >> 2));
  pSharedGetComLatency    = (uint32_t *)(pShared + (WRF50_SHARED_GET_COM_LATENCY     >> 2));

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("wr-f50: fatal error - did not find LM32-CB-CLUSTER!\n");
  } // if idx
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("wr-f50: fatal error - did not find THIS CPU!\n");
  } // if idx
  else cpuRamExternal = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  DBPRINT2("wr-f50: CPU RAM external 0x%8x, shared offset 0x%08x\n", cpuRamExternal, SHARED_OFFS);
  DBPRINT2("wr-f50: fw common shared begin   0x%08x\n", pShared);
  DBPRINT2("wr-f50: fw common shared end     0x%08x\n", pShared + (COMMON_SHARED_END >> 2));

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_END >> 2 ) + 1);
  DBPRINT2("wr-f50: fw specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (WRF50_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("wr-f50: fw specific shared end   0x%08x\n", pSharedTemp);

  *sharedSize        = (uint32_t)(pSharedTemp - pShared) << 2;

  // basic info to wr console
  DBPRINT1("\n");
  DBPRINT1("wr-f50: initSharedMem, shared size [bytes]: %d\n", *sharedSize);
  DBPRINT1("\n");
} // initSharedMem 


// clear project specific diagnostics
void extern_clearDiag()
{
  getTMainsAct    = 0x0;    
  getTDMAct       = 0x0;       
  getTDMSet       = 0x0;       
  getOffsDMAct    = 0x0;    
  getOffsDMMin    = 0x7fffffff;
  getOffsDMMax    = 0x80000000;
  getOffsMainsAct = 0x0; 
  getOffsMainsMin = 0x7fffffff;
  getOffsMainsMax = 0x80000000;
  getLockState    = WRF50_SLOCK_UNKWN;
  getLockDate     = 0x0;     
  getNLocked      = 0x0;      
  // getNCycles      = 0x0;  don't reset as this variable is important for the regulation    
  getNEvtsLate    = 0x0;    
  getComLatency   = 0x0;   

  statusArray     = 0x0;
  nEvtsLate       = 0x0;
  comLatency      = 0x0;
} // extern_clearDiag 


// entry action 'configured' state
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // get and publish NIC data
  fwlib_publishNICData(); 

  // clear diagnostic data
  fwlib_clearDiag();
  getNCycles = 0x0;
  
  // if everything is ok, we must return with COMMON_STATUS_OK
  if (status == MIL_STAT_OK) status = COMMON_STATUS_OK;

  return status;
} // extern_entryActionConfigured


// entry action 'operation' state
uint32_t extern_entryActionOperation()
{
  int      i;
  uint64_t tDummy;
  uint64_t eDummy;
  uint64_t pDummy;
  uint32_t fDummy;
  uint32_t flagDummy1, flagDummy2, flagDummy3, flagDummy4;
  int      enable_fifo;

  // clear diagnostics
  fwlib_clearDiag();
  getNCycles = 0;

  // flush ECA queue for lm32
  i = 0;
  while (fwlib_wait4ECAEvent(1000, &tDummy, &eDummy, &pDummy, &fDummy, &flagDummy1, &flagDummy2, &flagDummy3, &flagDummy4) !=  COMMON_ECADO_TIMEOUT) {i++;}
  DBPRINT1("wr-f50: ECA queue flushed - removed %d pending entries from ECA queue\n", i);
    
  // init get values
  *pSharedGetTMainsAct      = 0x0;   
  *pSharedGetTDMAct         = 0x0;      
  *pSharedGetTDMSet         = 0x0;      
  *pSharedGetOffsDMAct      = 0x0;   
  *pSharedGetOffsDMMin      = 0x0;   
  *pSharedGetOffsDMMax      = 0x0;   
  *pSharedGetOffsMainsAct   = 0x0;
  *pSharedGetOffsMainsMin   = 0x0;
  *pSharedGetOffsMainsMax   = 0x0;
  *pSharedGetLockState      = 0x0;   
  *pSharedGetLockDateHi     = 0x0;  
  *pSharedGetLockDateLo     = 0x0;  
  *pSharedGetNLocked        = 0x0;     
  *pSharedGetNCycles        = 0x0;     
  *pSharedGetNEvtsLate      = 0x0;   
  *pSharedGetComLatency     = 0x0;

  // init set values
  setF50Offset              = *pSharedSetF50Offset;
  setMode                   = *pSharedSetMode;

  return COMMON_STATUS_OK;
} // extern_entryActionOperation


uint32_t extern_exitActionOperation()
{ 
  return COMMON_STATUS_OK;
} // extern_exitActionOperation


// prepares evtId and param for sending a MIL telegram via the ECA /* chk */
void prepMilTelegramEca(uint32_t milTelegram, uint64_t *evtId, uint64_t *param)
{
  // evtId
  *evtId    &= 0xf000ffffffffffff;            // remove all bits for GID
  *evtId    |= (uint64_t)LOC_MIL_SEND << 48;  // set bit for GID to 'local message generating a MIL telegram'

  // param
  *param     = ((uint64_t)mil_domain) << 32;
  *param    |= (uint64_t)milTelegram & 0xffffffff;
} // void prepMilTelegramEca


// insert (and shift) tstamps
void manageStamps(uint64_t newStamp,               // new timestamp
                  uint64_t stamps[],               // all timestamps
                  uint32_t *actT,                  // actual cycle length
                  int      flagValid               // return values are valid
                  )
{
  int i;
  uint32_t cycLen;
  
  // timestamps: pop oldest, shift (not very clever, shame on me) and add new
  for (i=1; i<WRF50_N_STAMPS; i++) stamps[i-1] = stamps[i];
  stamps[WRF50_N_STAMPS - 1] = newStamp;

  cyclen = (uint32_t)((stamps[WRF50_N_STAMPS - 1] - stamps[0]) / WRF50_N_STAMPS - 1);

  if ((cyclen < (uint32_t)WRF50_CYCLELEN_MAX) && (cyclen > (uint32_t)WRF50_CYCLELEN_MIN)) flagValid = 1;
  else                                                                                    flagValid = 0;

  *actT = cyclen;
} // managestamps


// do action of state operation: This is THE central code of this firmware
uint32_t doActionOperation(uint64_t *tAct,                    // actual time
                           uint32_t actStatus)                // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t flagIsLate;                                        // flag indicating that we received a 'late' event from ECA
  uint32_t flagIsEarly;                                       // flag 'early'
  uint32_t flagIsConflict;                                    // flag 'conflict'
  uint32_t flagIsDelayed;                                     // flag 'delayed'
  uint32_t ecaAction;                                         // action triggered by event received from ECA
  uint64_t recDeadline;                                       // deadline received from ECA
  uint64_t reqDeadline;                                       // deadline requested by sender
  uint64_t recEvtId;                                          // evt ID received
  uint64_t recParam;                                          // param received
  uint32_t recTEF;                                            // TEF received
  uint32_t recGid;                                            // GID received
  uint32_t recEvtNo;                                          // event number received
  uint32_t recSid;                                            // SID received
  uint32_t recBpid;                                           // BPID received
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtid to send
  uint64_t sendParam;                                         // parameter to send
  uint32_t sendTEF;                                           // TEF to send

  uint32_t cycLen;                                            // help variable, cycle length [ns]
  
  status    = actStatus;

  ecaAction = fwlib_wait4ECAEvent(COMMON_ECATIMEOUT * 1000, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);

  switch (ecaAction) {
    // received WR timing message from Data Master (cycle start)
    case WRF50_ECADO_F50_DM:
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      
      if (recGid != PZU_F50) return WRF50_STATUS_BADSETTING;
      
      // update timestamps
      manageStamps(recDeadline, dmStamps, &cycLen, &dmValid);
      if (dmValid) getTDMAct = cycLen;
      
      break;
      
    // received WR timing message from TLU (50 Hz signal)
    // as this happens with a posttrigger of 100us, this should always (chk ?) happen after receiving the WR timing message from Data Master  
    case WRF50_ECADO_F50_TLU:
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);

      if (recGid != PZU_F50) return WRF50_STATUS_BADSETTING;

      // count number of received signals and check if we have sufficient data
      getNCycles++;

      // update timestamps
      manageStamps(recDeadline - (uint64_t)WRF50_POSTTRIGGER_TLU, f50Stamps, &cycLen, &f50Valid);
      if (f50Valid) getTMainsAct = cycLen;

      // assume worst case and see how far we get
      getLockState = WRF50_SLOCK_UNKWN;
      
      if (getNLocked > 50) {        // wait for one second prior we do s.th. useful
        if ((dmValid && f50Valid) { // data is valid?
            
        

        
        
      
      // deadline
      sendDeadline  = recDeadline + WRF50_PRETRIGGER_DM + mil_latency - WRF50_MILSEND_LATENCY;
       // protect from nonsense hi-frequency bursts
      if (sendDeadline < previous_time + WRF50_MILSEND_LATENCY) sendDeadline = previous_time + WRF50_MILSEND_LATENCY;
      previous_time = sendDeadline;

      // evtID + param
      sendEvtId     = recEvtId;
      prepMilTelegramEca(milTelegram, &sendEvtId, &sendParam);

      // clear MIL FIFO and write to ECA
      if (mil_mon) clearFifoEvtMil(pMilRec);
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 1);

      if (getSysTime() > sendDeadline) nEvtsLate++;
      nEvtsSnd++;

      // handle UTC events; here the UTC time (- offset) is distributed as a series of MIL telegrams
      if (recEvtNo == utc_trigger) {
        // send EVT_UTC_1/2/3/4/5 telegrams
        make_mil_timestamp(sendDeadline, evt_utc, utc_offset);
        sendDeadline += trig_utc_delay * one_us_ns;
        for (i=0; i<WRF50_N_UTC_EVTS; i++){
          sendDeadline += utc_utc_delay * one_us_ns;
          prepMilTelegramEca(evt_utc[i], &sendEvtId, &sendParam);
          // nicer evtId for debugging only
          sendEvtId &= 0xffff000fffffffff;
          sendEvtId |= (uint64_t)(0x0e0 + i) << 36;
          fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 1);
          nEvtsSnd++;
        } // for i
      } // if utc_trigger

      // reset inhibit counter for fill events
      inhibit_fill_events = RESET_INHIBIT_COUNTER;
        } // if dmValid ...
      } // if getNLocked

      break;

    // received timing message from TLU; this indicates a received MIL telegram on the MIL piggy
    case WRF50_ECADO_MIL_TLU:

      // in case of data monitoring, read message from MIL piggy FIFO and re-send it locally via the ECA
      if (mil_mon==2) {
        if (fwlib_wait4MILEvent(50, &recMilEvtData, &recMilEvtCode, &recMilVAcc, recMilEvts, 0) == COMMON_STATUS_OK) {

          // deadline
          sendDeadline  = recDeadline - WRF50_POSTTRIGGER_TLU; // time stamp when MIL telegram was decoded at MIL piggy
          sendDeadline += 1000000;                             // advance to 1ms into the future to avoid late messages
          // evtID
          sendEvtId     = fwlib_buildEvtidV1(LOC_MIL_REC, recMilEvtCode, 0x0, recMilVAcc, 0x0, recMilEvtData);
          // param
          sendParam     = ((uint64_t)mil_domain) << 32;

          fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 1);
          nEvtsRecD++;
        } // if wait4Milevent
      } // if mil_mon

      // read number of received 'broken' MIL telegrams
      readEventErrCntMil(pMilRec, &nEvtsErr);

      nEvtsRecT++;
      
      break;

    default :                                                         // flush ECA Queue
      flagIsLate = 0;                                                 // ignore late events
  } // switch ecaAction

  // send fill events if inactive for a long time
  if (request_fill_evt) {
    // decrease inhibit counter
    inhibit_fill_events--;

    // check for fill event
    if (!inhibit_fill_events) {
      // the last mil event is so far in the past that the inhibit counter is zero
      // so lets send the fill event and reset inhibit counter
      // deadline
      sendDeadline  = getSysTime();
      sendDeadline += COMMON_AHEADT;
      // evtID
      sendEvtId     = fwlib_buildEvtidV1(mil_domain, WRF50_DFLT_MIL_EVT_FILL, 0x0, 0x0, 0x0, 0x0); // chk
      convert_WReventID_to_milTelegram(sendEvtId, &milTelegram);                                   // --> MIL format
      prepMilTelegramEca(milTelegram, &sendEvtId, &sendParam);                                     // --> EvtId for internal use
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0);                                // --> ECA
      nEvtsSnd++;
      inhibit_fill_events = RESET_INHIBIT_COUNTER;
    } // if not inhibit fill events
  } // if request_fill_evt
    
  // check for late event
  if ((status == COMMON_STATUS_OK) && flagIsLate) status = WRF50_STATUS_LATEMESSAGE;
  
  // check WR sync state
  if (fwlib_wrCheckSyncState() == COMMON_STATUS_WRBADSYNC) return COMMON_STATUS_WRBADSYNC;
  else                                                     return status;
} // doActionOperation


int main(void) {
  uint64_t tActMessage;                         // time of actual message
  uint32_t status;                              // (error) status
  uint32_t actState;                            // actual FSM state
  uint32_t pubState;                            // value of published state
  uint32_t reqState;                            // requested FSM state
  uint32_t dummy1;                              // dummy parameter
  uint32_t sharedSize;                          // size of shared memory
  uint32_t *buildID;                            // build ID of lm32 firmware

  // init local variables
  buildID        = (uint32_t *)(INT_BASE_ADR + BUILDID_OFFS);                 // required for 'stack check'  

  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  nEvtsSnd       = 0;
  nEvtsRecT      = 0;
  nEvtsRecD      = 0;

  init();                                                                     // initialize stuff for lm32
  initSharedMem(&reqState, &sharedSize);                                      // initialize shared memory
  fwlib_init((uint32_t *)_startshared, cpuRamExternal, SHARED_OFFS, sharedSize, "wr-f50", WRF50_FW_VERSION); // init common stuff
  fwlib_clearDiag();                                                          // clear common diagnostics data
  
  while (1) {
    check_stack_fwid(buildID);                                                // check stack status
    fwlib_cmdHandler(&reqState, &dummy1);                                     // check for commands and possibly request state changes
    status = COMMON_STATUS_OK;                                                // reset status for each iteration

    // state machine
    status = fwlib_changeState(&actState, &reqState, status);                 // handle requested state changes
    switch(actState) {                                                        // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&tActMessage, status);
        if (status == COMMON_STATUS_WRBADSYNC)      reqState = COMMON_STATE_ERROR;
        if (status == COMMON_STATUS_ERROR)          reqState = COMMON_STATE_ERROR;
        break;
      default :                                                               // avoid flooding WB bus with unnecessary activity
        status = fwlib_doActionState(&reqState, actState, status);            // other 'do actions' are handled here
        break;
    } // switch
    
    switch (status) {
      case COMMON_STATUS_OK :                                                 // status OK
        statusArray = statusArray |  (0x1 << COMMON_STATUS_OK);               // set OK bit
        break;
      default :                                                               // status not OK
        if ((statusArray >> COMMON_STATUS_OK) & 0x1) fwlib_incBadStatusCnt(); // changing status from OK to 'not OK': increase 'bad status count'
        statusArray = statusArray & ~((uint64_t)0x1 << COMMON_STATUS_OK);     // clear OK bit
        statusArray = statusArray |  ((uint64_t)0x1 << status);               // set status bit and remember other bits set
        break;
    } // switch status
    
    if ((pubState == COMMON_STATE_OPREADY) && (actState  != COMMON_STATE_OPREADY)) fwlib_incBadStateCnt();
    fwlib_publishStatusArray(statusArray);
    pubState = actState;
    fwlib_publishState(pubState);
    
    *pSharedGetTMainsAct      = getTMainsAct;
    *pSharedGetTDMAct         = getTDMAct;
    *pSharedGetTDMSet         = getTDMSet;
    *pSharedGetOffsDMAct      = getOffsDMAct;
    *pSharedGetOffsDMMin      = getOffsDMMin;
    *pSharedGetOffsDMMax      = getOffsDMMax;
    *pSharedGetOffsMainsAct   = getOffsMainsAct;
    *pSharedGetOffsMainsMin   = getOffsMainsMin;
    *pSharedGetOffsMainsMax   = getOffsMainsMax;
    *pSharedGetLockState      = getLockState;
    *pSharedGetLockDateHi     = (uint32_t)(getLockDate >> 32);
    *pSharedGetLockDateLo     = (uint32_t)(getLockDate && 0xffffffff);
    *pSharedGetNLocked        = getNLocked;
    *pSharedGetNCycles        = getNCycles;
    *pSharedGetNEvtsLate      = getNEvtsLate;
    *pSharedGetComLatency     = getComLatency;
  } // while

  return(1); // this should never happen ...
} // main